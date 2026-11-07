#include "../include/Router.h"
#include <iostream>
#include <iomanip>
#include <cmath>
#include <set>

// ==========================================
// ReplanningPolicy Implementation
// ==========================================
ReplanningPolicy::ReplanningPolicy(double demandThreshold, double latencyThreshold, bool serverChange)
    : demandChangeThresholdFraction(demandThreshold),
      latencyChangeThresholdMs(latencyThreshold),
      replanOnServerStatusChange(serverChange) {}

// ==========================================
// Router Implementation
// ==========================================
Router::Router(Network& net, const CostCalculator& calc, const ReplanningPolicy& pol)
    : network(net),
      calculator(calc),
      policy(pol),
      lastPlannedTotalDemand(0.0),
      hasInitialPlan(false) {}

void Router::setReplanningPolicy(const ReplanningPolicy& pol) {
    policy = pol;
}

const ReplanningPolicy& Router::getReplanningPolicy() const {
    return policy;
}

void Router::recordCurrentStateSnapshot() {
    lastPlannedTotalDemand = network.getTotalTrafficDemand();

    lastServerStatus.clear();
    for (const auto& server : network.getServers()) {
        lastServerStatus[server.name] = server.isActive;
    }

    lastLinkLatencies.clear();
    for (const auto& link : network.getLinks()) {
        std::string key = link.regionName + "->" + link.serverName;
        lastLinkLatencies[key] = link.getEffectiveLatency();
    }
}

bool Router::shouldReplan(std::string& outReason) const {
    // 1. Initial Plan check
    if (!hasInitialPlan) {
        outReason = "Initial network routing plan required";
        return true;
    }

    // 2. Server Availability Trigger
    if (policy.replanOnServerStatusChange) {
        for (const auto& server : network.getServers()) {
            auto it = lastServerStatus.find(server.name);
            if (it != lastServerStatus.end() && it->second != server.isActive) {
                outReason = "Server health state change: '" + server.name + "' transitioned to " 
                          + (server.isActive ? "ACTIVE (Online)" : "INACTIVE (Failed)");
                return true;
            }
        }
    }

    // 3. Server Capacity Violation Check (Ensure no flow is directed to offline nodes or exceeds available capacity)
    std::map<std::string, double> plannedServerLoads;
    for (const auto& alloc : currentAllocations) {
        if (network.getServer(alloc.toNode) != nullptr) {
            plannedServerLoads[alloc.toNode] += alloc.flow;
        }
    }
    for (const auto& pair : plannedServerLoads) {
        const Server* s = network.getServer(pair.first);
        if (s) {
            if (!s->isActive && pair.second > 1e-6) {
                outReason = "Constraint Violation: Existing plan routes " + std::to_string(static_cast<int>(pair.second)) 
                          + " req/s to offline server '" + s->name + "'";
                return true;
            }
            if (s->isActive && pair.second > s->availableCapacity + 1e-6) {
                outReason = "Constraint Violation: Existing plan assigns " + std::to_string(static_cast<int>(pair.second))
                          + " req/s, exceeding available capacity of '" + s->name + "' (" + std::to_string(static_cast<int>(s->availableCapacity)) + " req/s)";
                return true;
            }
        }
    }

    // 4. Significant Latency Shift Trigger
    for (const auto& link : network.getLinks()) {
        std::string key = link.regionName + "->" + link.serverName;
        auto it = lastLinkLatencies.find(key);
        if (it != lastLinkLatencies.end()) {
            double deltaLat = std::abs(link.getEffectiveLatency() - it->second);
            if (deltaLat >= policy.latencyChangeThresholdMs) {
                outReason = "Link latency shift on [" + key + "]: Delta = +" 
                          + std::to_string(deltaLat).substr(0, 4) + " ms (Threshold: " 
                          + std::to_string(policy.latencyChangeThresholdMs).substr(0, 4) + " ms)";
                return true;
            }
        }
    }

    // 5. Significant Traffic Demand Shift Trigger
    double currentDemand = network.getTotalTrafficDemand();
    double demandDelta = std::abs(currentDemand - lastPlannedTotalDemand);
    double pctChange = (lastPlannedTotalDemand > 0.0) ? (demandDelta / lastPlannedTotalDemand) : 1.0;
    if (pctChange >= policy.demandChangeThresholdFraction) {
        outReason = "Traffic demand surge/drop: " + std::to_string(pctChange * 100.0).substr(0, 4) 
                  + "% shift (Threshold: " 
                  + std::to_string(policy.demandChangeThresholdFraction * 100.0).substr(0, 4) + "%)";
        return true;
    }

    // No condition met: Retain existing plan
    outReason = "Network state is stable; variations are within configured thresholds";
    return false;
}

MCMFResult Router::routeTraffic(bool force) {
    std::string reason;
    bool needsReplanning = force || shouldReplan(reason);

    if (!needsReplanning) {
        std::cout << "[Router Policy] PLAN RETAINED: " << reason << ".\n";
        return currentResult;
    }

    std::cout << "[Router Policy] REPLANNING TRIGGERED: " << reason << ".\n";

    // 1. Rebuild directed flow graph with latest network parameters
    currentGraph.buildFromNetwork(network, calculator);

    // 2. Execute Min-Cost Max-Flow optimization
    currentResult = MinCostMaxFlow::calculateMinCostMaxFlow(
        currentGraph, 
        currentGraph.getSourceId(), 
        currentGraph.getSinkId(), 
        network.getTotalTrafficDemand(), 
        calculator.getCostScalingFactor()
    );

    // 3. Extract updated flow assignments
    currentAllocations = MinCostMaxFlow::extractFlowAllocations(
        currentGraph, 
        calculator.getCostScalingFactor()
    );

    // 4. Update snapshot state
    recordCurrentStateSnapshot();
    hasInitialPlan = true;

    return currentResult;
}

const Graph& Router::getCurrentGraph() const { return currentGraph; }
const MCMFResult& Router::getCurrentResult() const { return currentResult; }
const std::vector<FlowAllocation>& Router::getCurrentAllocations() const { return currentAllocations; }

void Router::displayCurrentRoutingPlan() const {
    MinCostMaxFlow::printFlowSummary(currentGraph, currentResult, calculator.getCostScalingFactor());
}

void Router::displayPlanComparison(const std::vector<FlowAllocation>& beforeAlloc,
                                   const std::vector<FlowAllocation>& afterAlloc,
                                   const std::string& eventTitle) const {
    std::cout << "\n========================================================================================\n";
    std::cout << "                 ADAPTIVE ROUTING COMPARISON: " << eventTitle << "\n";
    std::cout << "========================================================================================\n";

    // Map route labels to flows
    std::map<std::string, double> beforeMap;
    std::map<std::string, double> afterMap;
    std::set<std::string> allLabels;

    for (const auto& a : beforeAlloc) {
        beforeMap[a.label] = a.flow;
        allLabels.insert(a.label);
    }
    for (const auto& a : afterAlloc) {
        afterMap[a.label] = a.flow;
        allLabels.insert(a.label);
    }

    std::cout << "+------------------------------------------+--------------------+--------------------+----------------+\n";
    std::cout << "| Route Path                               | Before (req/s)     | After (req/s)      | Delta (req/s)  |\n";
    std::cout << "+------------------------------------------+--------------------+--------------------+----------------+\n";

    double totalBefore = 0.0, totalAfter = 0.0;
    double trafficDiverted = 0.0;
    double originTraffic = 0.0;

    for (const auto& label : allLabels) {
        double b = beforeMap.count(label) ? beforeMap[label] : 0.0;
        double a = afterMap.count(label) ? afterMap[label] : 0.0;
        double delta = a - b;

        totalBefore += b;
        totalAfter += a;

        if (label.find("Origin") != std::string::npos && label.find("Source") == std::string::npos) {
            originTraffic += a;
        }

        if (b > 0.0 && a == 0.0) {
            trafficDiverted += b;
        }

        std::cout << "| " << std::left << std::setw(40) << label
                  << " | " << std::right << std::setw(18) << std::fixed << std::setprecision(0) << b
                  << " | " << std::setw(18) << std::fixed << std::setprecision(0) << a
                  << " | " << std::setw(13) << (delta > 0 ? "+" : "") << std::fixed << std::setprecision(0) << delta << " req/s |\n";
    }

    std::cout << "+------------------------------------------+--------------------+--------------------+----------------+\n";
    std::cout << "  Traffic Diverted From Saturated/Failed Nodes : " << std::fixed << std::setprecision(0) << trafficDiverted << " req/s\n";
    std::cout << "  Traffic Absorbed by Origin Fallback           : " << std::fixed << std::setprecision(0) << originTraffic << " req/s\n";
    std::cout << "  Total Active Routed Traffic Delivery          : " << std::fixed << std::setprecision(0) << (totalAfter / 2.0) << " req/s\n";
    std::cout << "========================================================================================\n\n";
}

