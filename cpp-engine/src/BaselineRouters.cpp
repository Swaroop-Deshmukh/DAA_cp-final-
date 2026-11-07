#include "../include/BaselineRouters.h"
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <cmath>
#include <sstream>

namespace {
    // Helper to evaluate server utilization bands cost (same 4 parallel load bands as Graph.cpp)
    double calculateServerLoadBandCost(double prevLoad, double addedFlow, double serverCapacity) {
        if (serverCapacity <= 0.0 || addedFlow <= 0.0) return 0.0;
        double bandCap = serverCapacity / 4.0;
        double cost = 0.0;
        double currentStart = prevLoad;
        double currentEnd = prevLoad + addedFlow;

        const double bandCosts[4] = {0.0, 6.0, 15.0, 60.0};

        for (int b = 0; b < 4; ++b) {
            double bandStart = b * bandCap;
            double bandEnd = (b + 1) * bandCap;

            // Overlap of [currentStart, currentEnd] with [bandStart, bandEnd]
            double overlapStart = std::max(currentStart, bandStart);
            double overlapEnd = std::min(currentEnd, bandEnd);

            if (overlapEnd > overlapStart) {
                double flowInBand = overlapEnd - overlapStart;
                cost += flowInBand * bandCosts[b];
            }
        }
        return cost;
    }

    // Helper to compute origin fallback unit cost identically to Phase 4 graph
    double getOriginFallbackUnitCost(const CostCalculator& calculator) {
        RouteConnectionProfile fallbackProfile("Any", "Origin Server", 100.0, 5.0, 0.0, calculator.getDefaultMissPenalty());
        CostBreakdown fbBd;
        calculator.calculateCost(fallbackProfile, fbBd);
        return fbBd.totalContinuousCost + 50.0; // Origin link cost + 50.0 compute penalty
    }

    void populateGraphTopology(DecisionTrace& trace, const Network& network, const CostCalculator& calculator) {
        Graph graph;
        graph.buildFromNetwork(network, calculator);

        trace.initialNodes.clear();
        for (const auto& node : graph.getAllNodes()) {
            GraphNodeDto nDto;
            nDto.id = node.id;
            nDto.name = node.name;
            switch (node.type) {
                case NodeType::SOURCE: nDto.type = "SOURCE"; break;
                case NodeType::REGION: nDto.type = "REGION"; break;
                case NodeType::SERVER: nDto.type = "SERVER"; break;
                case NodeType::ORIGIN: nDto.type = "ORIGIN"; break;
                case NodeType::SINK: nDto.type = "SINK"; break;
            }
            nDto.capacity = 0.0;
            nDto.demand = 0.0;
            nDto.isActive = true;
            trace.initialNodes.push_back(nDto);
        }

        trace.initialEdges.clear();
        int edgeCounter = 0;
        int n = graph.getNodeCount();
        for (int u = 0; u < n; ++u) {
            for (const auto& edge : graph.getEdgesFrom(u)) {
                GraphEdgeDto eDto;
                eDto.id = edgeCounter++;
                eDto.fromId = edge.from;
                eDto.toId = edge.to;
                eDto.fromName = graph.getNodeInfo(edge.from).name;
                eDto.toName = graph.getNodeInfo(edge.to).name;
                eDto.capacity = edge.capacity;
                eDto.flow = edge.flow;
                eDto.residualCapacity = edge.getResidualCapacity();
                eDto.unitCost = static_cast<double>(edge.unitCost) / 1000.0;
                eDto.scaledCost = edge.unitCost;
                eDto.isReverse = edge.isReverse;
                eDto.reverseEdgeIndex = edge.reverseEdgeIndex;
                eDto.label = edge.label;
                trace.initialEdges.push_back(eDto);
            }
        }
    }
}

StrategyResult BaselineRouters::runRoundRobin(const Network& network, const CostCalculator& calculator, bool recordTrace) {
    auto startClock = std::chrono::high_resolution_clock::now();

    StrategyResult res;
    res.strategyName = "Round Robin";
    res.totalTrafficRequested = network.getTotalTrafficDemand();
    res.edgeTrafficRouted = 0.0;
    res.originTrafficRouted = 0.0;
    res.totalRoutingCost = 0.0;
    double weightedLatency = 0.0;
    double originUnitCost = getOriginFallbackUnitCost(calculator);

    int stepCount = 1;
    if (recordTrace) {
        res.trace.strategy = "Round Robin";
        res.trace.finalExplanation = "Round Robin cyclically balances regional requests across active healthy edge servers, respecting server and link capacities before spilling overflow to Origin.";
        populateGraphTopology(res.trace, network, calculator);
    }

    // Track available remaining capacity on servers and links
    std::map<std::string, double> serverRemaining;
    for (const auto& s : network.getServers()) {
        serverRemaining[s.name] = (s.isActive) ? s.availableCapacity : 0.0;
        res.serverLoads[s.name] = 0.0;
    }

    std::map<std::string, double> linkRemaining;
    for (const auto& l : network.getLinks()) {
        std::string linkKey = l.regionName + "->" + l.serverName;
        linkRemaining[linkKey] = l.bandwidthCapacity;
        res.linkLoads[linkKey] = 0.0;
    }

    std::vector<Server> activeServers;
    for (const auto& s : network.getServers()) {
        if (s.isActive && s.availableCapacity > 0.0) {
            activeServers.push_back(s);
        }
    }

    if (recordTrace) {
        std::map<std::string, std::string> initD;
        std::string srvList = "";
        for (size_t i = 0; i < activeServers.size(); ++i) {
            srvList += activeServers[i].name + " [" + std::to_string(static_cast<long long>(activeServers[i].availableCapacity)) + " req/s]";
            if (i + 1 < activeServers.size()) srvList += ", ";
        }
        initD["active_servers"] = srvList;
        initD["total_active_servers"] = std::to_string(activeServers.size());
        initD["total_demand"] = std::to_string(static_cast<long long>(res.totalTrafficRequested)) + " req/s";
        res.trace.addStep(stepCount++, "ROTATION_INIT", "Round Robin Server Rotation Pool Initialized",
                          "Identified " + std::to_string(activeServers.size()) + " healthy active edge servers in cyclic order: " + srvList + ".", initD);
    }

    // Process each region
    for (const auto& region : network.getRegions()) {
        double demandToRoute = region.trafficDemand;

        if (!activeServers.empty()) {
            double targetShare = demandToRoute / static_cast<double>(activeServers.size());

            if (recordTrace) {
                std::map<std::string, std::string> regD;
                regD["region"] = region.name;
                regD["demand"] = std::to_string(static_cast<long long>(region.trafficDemand)) + " req/s";
                regD["target_equal_share"] = std::to_string(static_cast<long long>(targetShare)) + " req/s";
                res.trace.addStep(stepCount++, "REGION_EVALUATION", "Evaluating Ingress Demand: " + region.name,
                                  "Processing " + std::to_string(static_cast<long long>(region.trafficDemand)) + " req/s for region " + region.name +
                                  ". Target equal share across " + std::to_string(activeServers.size()) + " active servers is " +
                                  std::to_string(static_cast<long long>(targetShare)) + " req/s per server.", regD);
            }

            // Primary pass: equal division among available active servers
            for (const auto& server : activeServers) {
                if (demandToRoute <= EPSILON) break;

                const NetworkLink* link = network.getLink(region.name, server.name);
                if (!link) continue;

                std::string linkKey = region.name + "->" + server.name;
                double maxAllowed = std::min({targetShare, serverRemaining[server.name], linkRemaining[linkKey], demandToRoute});

                if (maxAllowed > EPSILON) {
                    RouteConnectionProfile profile(region.name, server.name, link->getEffectiveLatency(),
                                                  link->bandwidthCost, link->cacheHitRatio, calculator.getDefaultMissPenalty());
                    CostBreakdown bd;
                    calculator.calculateCost(profile, bd);

                    double linkCost = maxAllowed * bd.totalContinuousCost;
                    double bandCost = calculateServerLoadBandCost(res.serverLoads[server.name], maxAllowed, server.configuredCapacity);

                    res.totalRoutingCost += (linkCost + bandCost);
                    weightedLatency += maxAllowed * link->getEffectiveLatency();

                    res.serverLoads[server.name] += maxAllowed;
                    res.linkLoads[linkKey] += maxAllowed;
                    serverRemaining[server.name] -= maxAllowed;
                    linkRemaining[linkKey] -= maxAllowed;
                    demandToRoute -= maxAllowed;
                    res.edgeTrafficRouted += maxAllowed;

                    FlowAllocation alloc;
                    alloc.fromNode = region.name;
                    alloc.toNode = server.name;
                    alloc.flow = maxAllowed;
                    alloc.capacity = link->bandwidthCapacity;
                    alloc.unitCostContinuous = bd.totalContinuousCost;
                    alloc.totalCostContinuous = linkCost + bandCost;
                    alloc.label = region.name + " -> " + server.name;
                    res.allocations.push_back(alloc);

                    if (recordTrace) {
                        std::map<std::string, std::string> allocD;
                        allocD["region"] = region.name;
                        allocD["server"] = server.name;
                        allocD["flow_allocated"] = std::to_string(static_cast<long long>(maxAllowed)) + " req/s";
                        allocD["server_remaining_post"] = std::to_string(static_cast<long long>(serverRemaining[server.name])) + " req/s";
                        allocD["link_remaining_post"] = std::to_string(static_cast<long long>(linkRemaining[linkKey])) + " req/s";
                        allocD["demand_remaining_post"] = std::to_string(static_cast<long long>(demandToRoute)) + " req/s";
                        res.trace.addStep(stepCount++, "ASSIGNMENT",
                                          "Allocated " + std::to_string(static_cast<long long>(maxAllowed)) + " req/s to " + server.name,
                                          "Dispatched " + std::to_string(static_cast<long long>(maxAllowed)) + " req/s from " + region.name +
                                          " to " + server.name + " in rotation. Remaining server capacity is " +
                                          std::to_string(static_cast<long long>(serverRemaining[server.name])) + " req/s.", allocD);
                    }
                } else if (recordTrace && (serverRemaining[server.name] <= EPSILON || linkRemaining[linkKey] <= EPSILON)) {
                    std::map<std::string, std::string> capD;
                    capD["region"] = region.name;
                    capD["server"] = server.name;
                    capD["server_remaining"] = std::to_string(static_cast<long long>(serverRemaining[server.name])) + " req/s";
                    capD["link_remaining"] = std::to_string(static_cast<long long>(linkRemaining[linkKey])) + " req/s";
                    res.trace.addStep(stepCount++, "CAPACITY_EXHAUSTED",
                                      "Server or Link Saturated for " + server.name,
                                      "Cannot route flow from " + region.name + " to " + server.name + " due to zero available capacity. Rotating to next server.", capD);
                }
            }

            // Secondary pass: greedily fill any remaining server capacity for unassigned regional demand
            for (const auto& server : activeServers) {
                if (demandToRoute <= EPSILON) break;

                const NetworkLink* link = network.getLink(region.name, server.name);
                if (!link) continue;

                std::string linkKey = region.name + "->" + server.name;
                double maxAllowed = std::min({serverRemaining[server.name], linkRemaining[linkKey], demandToRoute});

                if (maxAllowed > EPSILON) {
                    RouteConnectionProfile profile(region.name, server.name, link->getEffectiveLatency(),
                                                  link->bandwidthCost, link->cacheHitRatio, calculator.getDefaultMissPenalty());
                    CostBreakdown bd;
                    calculator.calculateCost(profile, bd);

                    double linkCost = maxAllowed * bd.totalContinuousCost;
                    double bandCost = calculateServerLoadBandCost(res.serverLoads[server.name], maxAllowed, server.configuredCapacity);

                    res.totalRoutingCost += (linkCost + bandCost);
                    weightedLatency += maxAllowed * link->getEffectiveLatency();

                    res.serverLoads[server.name] += maxAllowed;
                    res.linkLoads[linkKey] += maxAllowed;
                    serverRemaining[server.name] -= maxAllowed;
                    linkRemaining[linkKey] -= maxAllowed;
                    demandToRoute -= maxAllowed;
                    res.edgeTrafficRouted += maxAllowed;

                    FlowAllocation alloc;
                    alloc.fromNode = region.name;
                    alloc.toNode = server.name;
                    alloc.flow = maxAllowed;
                    alloc.capacity = link->bandwidthCapacity;
                    alloc.unitCostContinuous = bd.totalContinuousCost;
                    alloc.totalCostContinuous = linkCost + bandCost;
                    alloc.label = region.name + " -> " + server.name;
                    res.allocations.push_back(alloc);

                    if (recordTrace) {
                        std::map<std::string, std::string> allocD;
                        allocD["region"] = region.name;
                        allocD["server"] = server.name;
                        allocD["flow_allocated"] = std::to_string(static_cast<long long>(maxAllowed)) + " req/s";
                        allocD["server_remaining_post"] = std::to_string(static_cast<long long>(serverRemaining[server.name])) + " req/s";
                        allocD["demand_remaining_post"] = std::to_string(static_cast<long long>(demandToRoute)) + " req/s";
                        res.trace.addStep(stepCount++, "ASSIGNMENT",
                                          "Secondary Allocation: " + std::to_string(static_cast<long long>(maxAllowed)) + " req/s to " + server.name,
                                          "Secondary pass absorbed " + std::to_string(static_cast<long long>(maxAllowed)) + " req/s on " + server.name +
                                          ". Remaining demand for " + region.name + " is " + std::to_string(static_cast<long long>(demandToRoute)) + " req/s.", allocD);
                    }
                }
            }
        }

        // Spillover to Origin Fallback
        if (demandToRoute > EPSILON) {
            double originCost = demandToRoute * originUnitCost;
            res.totalRoutingCost += originCost;
            weightedLatency += demandToRoute * 100.0; // Origin latency: 100 ms
            res.originTrafficRouted += demandToRoute;

            FlowAllocation alloc;
            alloc.fromNode = region.name;
            alloc.toNode = "Origin Server";
            alloc.flow = demandToRoute;
            alloc.capacity = region.trafficDemand;
            alloc.unitCostContinuous = originUnitCost;
            alloc.totalCostContinuous = originCost;
            alloc.label = region.name + " -> Origin [Fallback]";
            res.allocations.push_back(alloc);

            if (recordTrace) {
                std::map<std::string, std::string> fbD;
                fbD["region"] = region.name;
                fbD["overflow_demand"] = std::to_string(static_cast<long long>(demandToRoute)) + " req/s";
                fbD["origin_latency"] = "100.0 ms";
                fbD["origin_unit_cost"] = std::to_string(originUnitCost);
                res.trace.addStep(stepCount++, "FALLBACK",
                                  "Origin DC Fallback Activated for " + region.name + " (" + std::to_string(static_cast<long long>(demandToRoute)) + " req/s)",
                                  "Edge server capacity saturated. Diverted " + std::to_string(static_cast<long long>(demandToRoute)) +
                                  " req/s to central Origin datacenter at high latency penalty (100.0 ms).", fbD);
            }
        }
    }

    auto endClock = std::chrono::high_resolution_clock::now();
    res.executionTimeMicroseconds = std::chrono::duration_cast<std::chrono::microseconds>(endClock - startClock).count();

    res.totalTrafficRouted = res.edgeTrafficRouted + res.originTrafficRouted;
    res.unservedTraffic = std::max(0.0, res.totalTrafficRequested - res.totalTrafficRouted);
    res.averageRoutingCostPerReq = (res.totalTrafficRouted > 0.0) ? (res.totalRoutingCost / res.totalTrafficRouted) : 0.0;
    res.averageLatencyMs = (res.totalTrafficRouted > 0.0) ? (weightedLatency / res.totalTrafficRouted) : 0.0;

    for (const auto& s : network.getServers()) {
        res.serverUtilPct[s.name] = (s.configuredCapacity > 0.0) ? (res.serverLoads[s.name] / s.configuredCapacity * 100.0) : 0.0;
    }

    for (const auto& l : network.getLinks()) {
        std::string linkKey = l.regionName + "->" + l.serverName;
        res.linkUtilPct[linkKey] = (l.bandwidthCapacity > 0.0) ? (res.linkLoads[linkKey] / l.bandwidthCapacity * 100.0) : 0.0;
    }

    if (recordTrace) {
        std::map<std::string, std::string> termD;
        termD["total_traffic_routed"] = std::to_string(static_cast<long long>(res.totalTrafficRouted)) + " req/s";
        termD["edge_traffic"] = std::to_string(static_cast<long long>(res.edgeTrafficRouted)) + " req/s";
        termD["origin_spillover"] = std::to_string(static_cast<long long>(res.originTrafficRouted)) + " req/s";
        termD["average_latency"] = std::to_string(res.averageLatencyMs) + " ms";
        res.trace.addStep(stepCount++, "TERMINATION", "Round Robin Allocation Completed",
                          "Completed cyclic allocation. Total routed traffic: " + std::to_string(static_cast<long long>(res.totalTrafficRouted)) +
                          " req/s, Average latency: " + std::to_string(res.averageLatencyMs) + " ms.", termD);
    }

    return res;
}

StrategyResult BaselineRouters::runNearestServer(const Network& network, const CostCalculator& calculator, bool recordTrace) {
    auto startClock = std::chrono::high_resolution_clock::now();

    StrategyResult res;
    res.strategyName = "Nearest Server";
    res.totalTrafficRequested = network.getTotalTrafficDemand();
    res.edgeTrafficRouted = 0.0;
    res.originTrafficRouted = 0.0;
    res.totalRoutingCost = 0.0;
    double weightedLatency = 0.0;
    double originUnitCost = getOriginFallbackUnitCost(calculator);

    int stepCount = 1;
    if (recordTrace) {
        res.trace.strategy = "Nearest Server";
        res.trace.finalExplanation = "Nearest Server directs traffic greedily to the edge server with lowest effective latency, spilling over to next-closest servers and finally Origin when capacities are exhausted.";
        populateGraphTopology(res.trace, network, calculator);
        std::map<std::string, std::string> initD;
        initD["total_regions"] = std::to_string(network.getRegions().size());
        initD["total_servers"] = std::to_string(network.getServers().size());
        initD["total_demand"] = std::to_string(static_cast<long long>(res.totalTrafficRequested)) + " req/s";
        res.trace.addStep(stepCount++, "STRATEGY_INIT", "Nearest Server Strategy Initialized",
                          "Initialized greedy latency-based routing for " + std::to_string(network.getRegions().size()) + " regional traffic sources.", initD);
    }

    std::map<std::string, double> serverRemaining;
    for (const auto& s : network.getServers()) {
        serverRemaining[s.name] = (s.isActive) ? s.availableCapacity : 0.0;
        res.serverLoads[s.name] = 0.0;
    }

    std::map<std::string, double> linkRemaining;
    for (const auto& l : network.getLinks()) {
        std::string linkKey = l.regionName + "->" + l.serverName;
        linkRemaining[linkKey] = l.bandwidthCapacity;
        res.linkLoads[linkKey] = 0.0;
    }

    // Process each region
    for (const auto& region : network.getRegions()) {
        double demandToRoute = region.trafficDemand;

        // Collect all links originating from this region to active servers, sorted by lowest latency
        std::vector<NetworkLink> candidateLinks;
        for (const auto& link : network.getLinks()) {
            if (link.regionName == region.name) {
                const Server* s = network.getServer(link.serverName);
                if (s && s->isActive && s->availableCapacity > 0.0) {
                    candidateLinks.push_back(link);
                }
            }
        }

        std::sort(candidateLinks.begin(), candidateLinks.end(), [](const NetworkLink& a, const NetworkLink& b) {
            return a.getEffectiveLatency() < b.getEffectiveLatency();
        });

        if (recordTrace) {
            std::map<std::string, std::string> candD;
            std::string rankStr = "";
            for (size_t i = 0; i < candidateLinks.size(); ++i) {
                rankStr += "#" + std::to_string(i + 1) + " " + candidateLinks[i].serverName +
                           " (" + std::to_string(candidateLinks[i].getEffectiveLatency()) + " ms)";
                if (i + 1 < candidateLinks.size()) rankStr += ", ";
            }
            candD["region"] = region.name;
            candD["demand"] = std::to_string(static_cast<long long>(region.trafficDemand)) + " req/s";
            candD["ranked_candidates"] = rankStr;
            res.trace.addStep(stepCount++, "CANDIDATE_EVALUATION", "Candidate Server Ranking for " + region.name,
                              "Ranked candidate edge servers for " + region.name + " in ascending order of effective latency: " + rankStr + ".", candD);
        }

        // Greedily fill lowest-latency servers first
        for (const auto& link : candidateLinks) {
            if (demandToRoute <= EPSILON) break;

            std::string linkKey = region.name + "->" + link.serverName;
            double maxAllowed = std::min({serverRemaining[link.serverName], linkRemaining[linkKey], demandToRoute});

            if (maxAllowed > EPSILON) {
                RouteConnectionProfile profile(region.name, link.serverName, link.getEffectiveLatency(),
                                              link.bandwidthCost, link.cacheHitRatio, calculator.getDefaultMissPenalty());
                CostBreakdown bd;
                calculator.calculateCost(profile, bd);

                double linkCost = maxAllowed * bd.totalContinuousCost;
                double bandCost = calculateServerLoadBandCost(res.serverLoads[link.serverName], maxAllowed, 
                                                            network.getServer(link.serverName)->configuredCapacity);

                res.totalRoutingCost += (linkCost + bandCost);
                weightedLatency += maxAllowed * link.getEffectiveLatency();

                res.serverLoads[link.serverName] += maxAllowed;
                res.linkLoads[linkKey] += maxAllowed;
                serverRemaining[link.serverName] -= maxAllowed;
                linkRemaining[linkKey] -= maxAllowed;
                demandToRoute -= maxAllowed;
                res.edgeTrafficRouted += maxAllowed;

                FlowAllocation alloc;
                alloc.fromNode = region.name;
                alloc.toNode = link.serverName;
                alloc.flow = maxAllowed;
                alloc.capacity = link.bandwidthCapacity;
                alloc.unitCostContinuous = bd.totalContinuousCost;
                alloc.totalCostContinuous = linkCost + bandCost;
                alloc.label = region.name + " -> " + link.serverName;
                res.allocations.push_back(alloc);

                if (recordTrace) {
                    std::map<std::string, std::string> allocD;
                    allocD["region"] = region.name;
                    allocD["server"] = link.serverName;
                    allocD["flow_allocated"] = std::to_string(static_cast<long long>(maxAllowed)) + " req/s";
                    allocD["effective_latency"] = std::to_string(link.getEffectiveLatency()) + " ms";
                    allocD["server_remaining_post"] = std::to_string(static_cast<long long>(serverRemaining[link.serverName])) + " req/s";
                    allocD["demand_remaining_post"] = std::to_string(static_cast<long long>(demandToRoute)) + " req/s";
                    res.trace.addStep(stepCount++, "ASSIGNMENT",
                                      "Allocated " + std::to_string(static_cast<long long>(maxAllowed)) + " req/s to " + link.serverName + " (" + std::to_string(link.getEffectiveLatency()) + " ms)",
                                      "Greedily routed " + std::to_string(static_cast<long long>(maxAllowed)) + " req/s from " + region.name +
                                      " to nearest available edge server " + link.serverName + " with latency " + std::to_string(link.getEffectiveLatency()) + " ms.", allocD);
                }
            } else if (recordTrace && (serverRemaining[link.serverName] <= EPSILON || linkRemaining[linkKey] <= EPSILON)) {
                std::map<std::string, std::string> satD;
                satD["region"] = region.name;
                satD["server"] = link.serverName;
                satD["server_remaining"] = std::to_string(static_cast<long long>(serverRemaining[link.serverName])) + " req/s";
                res.trace.addStep(stepCount++, "CAPACITY_EXHAUSTED",
                                  "Server " + link.serverName + " Capacity Saturated",
                                  "Server " + link.serverName + " has 0 req/s available capacity. Shifting remaining demand to next-nearest server.", satD);
            }
        }

        // Spillover to Origin Fallback
        if (demandToRoute > EPSILON) {
            double originCost = demandToRoute * originUnitCost;
            res.totalRoutingCost += originCost;
            weightedLatency += demandToRoute * 100.0;
            res.originTrafficRouted += demandToRoute;

            FlowAllocation alloc;
            alloc.fromNode = region.name;
            alloc.toNode = "Origin Server";
            alloc.flow = demandToRoute;
            alloc.capacity = region.trafficDemand;
            alloc.unitCostContinuous = originUnitCost;
            alloc.totalCostContinuous = originCost;
            alloc.label = region.name + " -> Origin [Fallback]";
            res.allocations.push_back(alloc);

            if (recordTrace) {
                std::map<std::string, std::string> fbD;
                fbD["region"] = region.name;
                fbD["overflow_demand"] = std::to_string(static_cast<long long>(demandToRoute)) + " req/s";
                fbD["origin_latency"] = "100.0 ms";
                fbD["origin_unit_cost"] = std::to_string(originUnitCost);
                res.trace.addStep(stepCount++, "FALLBACK",
                                  "Origin Fallback Activated for " + region.name + " (" + std::to_string(static_cast<long long>(demandToRoute)) + " req/s)",
                                  "Edge server capacity saturated. Diverted " + std::to_string(static_cast<long long>(demandToRoute)) +
                                  " req/s to central Origin DC at high latency penalty (100.0 ms).", fbD);
            }
        }
    }

    auto endClock = std::chrono::high_resolution_clock::now();
    res.executionTimeMicroseconds = std::chrono::duration_cast<std::chrono::microseconds>(endClock - startClock).count();

    res.totalTrafficRouted = res.edgeTrafficRouted + res.originTrafficRouted;
    res.unservedTraffic = std::max(0.0, res.totalTrafficRequested - res.totalTrafficRouted);
    res.averageRoutingCostPerReq = (res.totalTrafficRouted > 0.0) ? (res.totalRoutingCost / res.totalTrafficRouted) : 0.0;
    res.averageLatencyMs = (res.totalTrafficRouted > 0.0) ? (weightedLatency / res.totalTrafficRouted) : 0.0;

    for (const auto& s : network.getServers()) {
        res.serverUtilPct[s.name] = (s.configuredCapacity > 0.0) ? (res.serverLoads[s.name] / s.configuredCapacity * 100.0) : 0.0;
    }

    for (const auto& l : network.getLinks()) {
        std::string linkKey = l.regionName + "->" + l.serverName;
        res.linkUtilPct[linkKey] = (l.bandwidthCapacity > 0.0) ? (res.linkLoads[linkKey] / l.bandwidthCapacity * 100.0) : 0.0;
    }

    if (recordTrace) {
        std::map<std::string, std::string> termD;
        termD["total_traffic_routed"] = std::to_string(static_cast<long long>(res.totalTrafficRouted)) + " req/s";
        termD["edge_traffic"] = std::to_string(static_cast<long long>(res.edgeTrafficRouted)) + " req/s";
        termD["origin_spillover"] = std::to_string(static_cast<long long>(res.originTrafficRouted)) + " req/s";
        termD["average_latency"] = std::to_string(res.averageLatencyMs) + " ms";
        res.trace.addStep(stepCount++, "TERMINATION", "Nearest Server Allocation Completed",
                          "Completed greedy nearest-server routing. Total routed traffic: " + std::to_string(static_cast<long long>(res.totalTrafficRouted)) +
                          " req/s, Average latency: " + std::to_string(res.averageLatencyMs) + " ms.", termD);
    }

    return res;
}

StrategyResult BaselineRouters::runGreedyCost(const Network& network, const CostCalculator& calculator, bool recordTrace) {
    auto startClock = std::chrono::high_resolution_clock::now();

    StrategyResult res;
    res.strategyName = "Greedy Cost";
    res.totalTrafficRequested = network.getTotalTrafficDemand();
    res.edgeTrafficRouted = 0.0;
    res.originTrafficRouted = 0.0;
    res.totalRoutingCost = 0.0;
    double weightedLatency = 0.0;
    double originUnitCost = getOriginFallbackUnitCost(calculator);

    int stepCount = 1;
    if (recordTrace) {
        res.trace.strategy = "Greedy Cost";
        res.trace.finalExplanation = "Greedy Cost calculates multi-factor composite cost (latency, bandwidth, cache miss penalty) for each eligible link and allocates traffic greedily to the cheapest routes first.";
        populateGraphTopology(res.trace, network, calculator);
        std::map<std::string, std::string> initD;
        initD["alpha_latency"] = std::to_string(calculator.getAlpha());
        initD["beta_bandwidth"] = std::to_string(calculator.getBeta());
        initD["miss_penalty"] = std::to_string(calculator.getDefaultMissPenalty());
        initD["total_demand"] = std::to_string(static_cast<long long>(res.totalTrafficRequested)) + " req/s";
        res.trace.addStep(stepCount++, "STRATEGY_INIT", "Greedy Cost Strategy Initialized",
                          "Initialized composite multi-factor cost routing with cost weights alpha=" + std::to_string(calculator.getAlpha()) +
                          ", beta=" + std::to_string(calculator.getBeta()) + ", missPenalty=" + std::to_string(calculator.getDefaultMissPenalty()) + ".", initD);
    }

    std::map<std::string, double> serverRemaining;
    for (const auto& s : network.getServers()) {
        serverRemaining[s.name] = (s.isActive) ? s.availableCapacity : 0.0;
        res.serverLoads[s.name] = 0.0;
    }

    std::map<std::string, double> linkRemaining;
    for (const auto& l : network.getLinks()) {
        std::string linkKey = l.regionName + "->" + l.serverName;
        linkRemaining[linkKey] = l.bandwidthCapacity;
        res.linkLoads[linkKey] = 0.0;
    }

    // Process each region
    for (const auto& region : network.getRegions()) {
        double demandToRoute = region.trafficDemand;

        // Collect all links originating from this region to active servers, sorted by lowest continuous cost
        struct LinkWithCost {
            NetworkLink link;
            double cost;
        };
        std::vector<LinkWithCost> candidateLinks;

        for (const auto& link : network.getLinks()) {
            if (link.regionName == region.name) {
                const Server* s = network.getServer(link.serverName);
                if (s && s->isActive && s->availableCapacity > 0.0) {
                    RouteConnectionProfile profile(region.name, link.serverName, link.getEffectiveLatency(),
                                                  link.bandwidthCost, link.cacheHitRatio, calculator.getDefaultMissPenalty());
                    CostBreakdown bd;
                    calculator.calculateCost(profile, bd);
                    candidateLinks.push_back({link, bd.totalContinuousCost});
                }
            }
        }

        std::sort(candidateLinks.begin(), candidateLinks.end(), [](const LinkWithCost& a, const LinkWithCost& b) {
            return a.cost < b.cost;
        });

        if (recordTrace) {
            std::map<std::string, std::string> costD;
            std::string costStr = "";
            for (size_t i = 0; i < candidateLinks.size(); ++i) {
                std::ostringstream cs;
                cs << std::fixed << std::setprecision(2) << candidateLinks[i].cost;
                costStr += "#" + std::to_string(i + 1) + " " + candidateLinks[i].link.serverName + " ($" + cs.str() + "/req)";
                if (i + 1 < candidateLinks.size()) costStr += ", ";
            }
            costD["region"] = region.name;
            costD["demand"] = std::to_string(static_cast<long long>(region.trafficDemand)) + " req/s";
            costD["ranked_routes"] = costStr;
            res.trace.addStep(stepCount++, "ROUTE_EVALUATION", "Multi-Factor Cost Ranking for " + region.name,
                              "Calculated composite routing cost for candidate links from " + region.name + ". Ranked cheapest routes: " + costStr + ".", costD);
        }

        // Greedily fill cheapest server connections first
        for (const auto& item : candidateLinks) {
            if (demandToRoute <= EPSILON) break;

            const auto& link = item.link;
            std::string linkKey = region.name + "->" + link.serverName;
            double maxAllowed = std::min({serverRemaining[link.serverName], linkRemaining[linkKey], demandToRoute});

            if (maxAllowed > EPSILON) {
                double linkCost = maxAllowed * item.cost;
                double bandCost = calculateServerLoadBandCost(res.serverLoads[link.serverName], maxAllowed, 
                                                            network.getServer(link.serverName)->configuredCapacity);

                res.totalRoutingCost += (linkCost + bandCost);
                weightedLatency += maxAllowed * link.getEffectiveLatency();

                res.serverLoads[link.serverName] += maxAllowed;
                res.linkLoads[linkKey] += maxAllowed;
                serverRemaining[link.serverName] -= maxAllowed;
                linkRemaining[linkKey] -= maxAllowed;
                demandToRoute -= maxAllowed;
                res.edgeTrafficRouted += maxAllowed;

                FlowAllocation alloc;
                alloc.fromNode = region.name;
                alloc.toNode = link.serverName;
                alloc.flow = maxAllowed;
                alloc.capacity = link.bandwidthCapacity;
                alloc.unitCostContinuous = item.cost;
                alloc.totalCostContinuous = linkCost + bandCost;
                alloc.label = region.name + " -> " + link.serverName;
                res.allocations.push_back(alloc);

                if (recordTrace) {
                    std::map<std::string, std::string> allocD;
                    allocD["region"] = region.name;
                    allocD["server"] = link.serverName;
                    allocD["flow_allocated"] = std::to_string(static_cast<long long>(maxAllowed)) + " req/s";
                    std::ostringstream cs;
                    cs << std::fixed << std::setprecision(2) << item.cost;
                    allocD["unit_cost"] = "$" + cs.str() + "/req";
                    allocD["server_remaining_post"] = std::to_string(static_cast<long long>(serverRemaining[link.serverName])) + " req/s";
                    allocD["demand_remaining_post"] = std::to_string(static_cast<long long>(demandToRoute)) + " req/s";
                    res.trace.addStep(stepCount++, "ASSIGNMENT",
                                      "Allocated " + std::to_string(static_cast<long long>(maxAllowed)) + " req/s to cheapest route -> " + link.serverName + " ($" + cs.str() + "/req)",
                                      "Dispatched " + std::to_string(static_cast<long long>(maxAllowed)) + " req/s from " + region.name +
                                      " to cheapest candidate server " + link.serverName + " at unit cost of $" + cs.str() + "/req.", allocD);
                }
            } else if (recordTrace && (serverRemaining[link.serverName] <= EPSILON || linkRemaining[linkKey] <= EPSILON)) {
                std::map<std::string, std::string> satD;
                satD["region"] = region.name;
                satD["server"] = link.serverName;
                satD["server_remaining"] = std::to_string(static_cast<long long>(serverRemaining[link.serverName])) + " req/s";
                res.trace.addStep(stepCount++, "CAPACITY_EXHAUSTED",
                                  "Route to " + link.serverName + " Saturated",
                                  "Cheapest route to " + link.serverName + " has 0 req/s available capacity. Shifting remaining demand to next cheapest candidate.", satD);
            }
        }

        // Spillover to Origin Fallback
        if (demandToRoute > EPSILON) {
            double originCost = demandToRoute * originUnitCost;
            res.totalRoutingCost += originCost;
            weightedLatency += demandToRoute * 100.0;
            res.originTrafficRouted += demandToRoute;

            FlowAllocation alloc;
            alloc.fromNode = region.name;
            alloc.toNode = "Origin Server";
            alloc.flow = demandToRoute;
            alloc.capacity = region.trafficDemand;
            alloc.unitCostContinuous = originUnitCost;
            alloc.totalCostContinuous = originCost;
            alloc.label = region.name + " -> Origin [Fallback]";
            res.allocations.push_back(alloc);

            if (recordTrace) {
                std::map<std::string, std::string> fbD;
                fbD["region"] = region.name;
                fbD["overflow_demand"] = std::to_string(static_cast<long long>(demandToRoute)) + " req/s";
                fbD["origin_latency"] = "100.0 ms";
                fbD["origin_unit_cost"] = std::to_string(originUnitCost);
                res.trace.addStep(stepCount++, "FALLBACK",
                                  "Origin Fallback Activated for " + region.name + " (" + std::to_string(static_cast<long long>(demandToRoute)) + " req/s)",
                                  "Edge server capacity saturated. Diverted " + std::to_string(static_cast<long long>(demandToRoute)) +
                                  " req/s to central Origin DC at high latency penalty (100.0 ms).", fbD);
            }
        }
    }

    auto endClock = std::chrono::high_resolution_clock::now();
    res.executionTimeMicroseconds = std::chrono::duration_cast<std::chrono::microseconds>(endClock - startClock).count();

    res.totalTrafficRouted = res.edgeTrafficRouted + res.originTrafficRouted;
    res.unservedTraffic = std::max(0.0, res.totalTrafficRequested - res.totalTrafficRouted);
    res.averageRoutingCostPerReq = (res.totalTrafficRouted > 0.0) ? (res.totalRoutingCost / res.totalTrafficRouted) : 0.0;
    res.averageLatencyMs = (res.totalTrafficRouted > 0.0) ? (weightedLatency / res.totalTrafficRouted) : 0.0;

    for (const auto& s : network.getServers()) {
        res.serverUtilPct[s.name] = (s.configuredCapacity > 0.0) ? (res.serverLoads[s.name] / s.configuredCapacity * 100.0) : 0.0;
    }

    for (const auto& l : network.getLinks()) {
        std::string linkKey = l.regionName + "->" + l.serverName;
        res.linkUtilPct[linkKey] = (l.bandwidthCapacity > 0.0) ? (res.linkLoads[linkKey] / l.bandwidthCapacity * 100.0) : 0.0;
    }

    if (recordTrace) {
        std::map<std::string, std::string> termD;
        termD["total_traffic_routed"] = std::to_string(static_cast<long long>(res.totalTrafficRouted)) + " req/s";
        termD["edge_traffic"] = std::to_string(static_cast<long long>(res.edgeTrafficRouted)) + " req/s";
        termD["origin_spillover"] = std::to_string(static_cast<long long>(res.originTrafficRouted)) + " req/s";
        termD["average_latency"] = std::to_string(res.averageLatencyMs) + " ms";
        res.trace.addStep(stepCount++, "TERMINATION", "Greedy Cost Allocation Completed",
                          "Completed greedy cost routing. Total routed traffic: " + std::to_string(static_cast<long long>(res.totalTrafficRouted)) +
                          " req/s, Average latency: " + std::to_string(res.averageLatencyMs) + " ms.", termD);
    }

    return res;
}

StrategyResult BaselineRouters::runMCMF(const Network& network, const CostCalculator& calculator, bool recordTrace) {
    auto startClock = std::chrono::high_resolution_clock::now();

    Graph flowGraph;
    flowGraph.buildFromNetwork(network, calculator);

    StrategyResult res;
    res.strategyName = "Min-Cost Max-Flow";

    MCMFResult mcmfRes = MinCostMaxFlow::calculateMinCostMaxFlow(
        flowGraph, 
        flowGraph.getSourceId(), 
        flowGraph.getSinkId(), 
        network.getTotalTrafficDemand(), 
        calculator.getCostScalingFactor(),
        recordTrace ? &res.trace : nullptr
    );

    auto endClock = std::chrono::high_resolution_clock::now();

    res.totalTrafficRequested = network.getTotalTrafficDemand();
    res.totalRoutingCost = mcmfRes.totalContinuousCost;
    res.executionTimeMicroseconds = std::chrono::duration_cast<std::chrono::microseconds>(endClock - startClock).count();
    res.allocations = MinCostMaxFlow::extractFlowAllocations(flowGraph, calculator.getCostScalingFactor());

    res.edgeTrafficRouted = 0.0;
    res.originTrafficRouted = 0.0;
    double weightedLatency = 0.0;

    for (const auto& s : network.getServers()) {
        res.serverLoads[s.name] = 0.0;
    }

    for (const auto& l : network.getLinks()) {
        std::string linkKey = l.regionName + "->" + l.serverName;
        res.linkLoads[linkKey] = 0.0;
    }

    for (const auto& a : res.allocations) {
        if (network.getServer(a.toNode) != nullptr) {
            res.serverLoads[a.toNode] += a.flow;
            res.edgeTrafficRouted += a.flow;
            std::string linkKey = a.fromNode + "->" + a.toNode;
            res.linkLoads[linkKey] += a.flow;

            const NetworkLink* link = network.getLink(a.fromNode, a.toNode);
            if (link) {
                weightedLatency += a.flow * link->getEffectiveLatency();
            }
        } else if (a.toNode == "Origin Server") {
            res.originTrafficRouted += a.flow;
            weightedLatency += a.flow * 100.0; // Origin latency: 100 ms
        }
    }

    res.totalTrafficRouted = res.edgeTrafficRouted + res.originTrafficRouted;
    res.unservedTraffic = std::max(0.0, res.totalTrafficRequested - res.totalTrafficRouted);
    res.averageRoutingCostPerReq = (res.totalTrafficRouted > 0.0) ? (res.totalRoutingCost / res.totalTrafficRouted) : 0.0;
    res.averageLatencyMs = (res.totalTrafficRouted > 0.0) ? (weightedLatency / res.totalTrafficRouted) : 0.0;

    for (const auto& s : network.getServers()) {
        res.serverUtilPct[s.name] = (s.configuredCapacity > 0.0) ? (res.serverLoads[s.name] / s.configuredCapacity * 100.0) : 0.0;
    }

    for (const auto& l : network.getLinks()) {
        std::string linkKey = l.regionName + "->" + l.serverName;
        res.linkUtilPct[linkKey] = (l.bandwidthCapacity > 0.0) ? (res.linkLoads[linkKey] / l.bandwidthCapacity * 100.0) : 0.0;
    }

    return res;
}

std::vector<StrategyResult> BaselineRouters::compareAll(const Network& network, 
                                                       const CostCalculator& calculator, 
                                                       const std::string& scenarioTitle,
                                                       bool printTable) {
    std::vector<StrategyResult> results;
    results.push_back(runRoundRobin(network, calculator));
    results.push_back(runNearestServer(network, calculator));
    results.push_back(runGreedyCost(network, calculator));
    results.push_back(runMCMF(network, calculator));

    if (printTable) {
        printComparisonTable(results, scenarioTitle);
    }
    return results;
}

void BaselineRouters::printComparisonTable(const std::vector<StrategyResult>& results, 
                                          const std::string& scenarioTitle) {
    std::cout << "\n========================================================================================================================\n";
    std::cout << "                               STRATEGY BENCHMARK COMPARISON: " << scenarioTitle << "\n";
    std::cout << "========================================================================================================================\n";
    std::cout << "+---------------------+------------+------------+------------+------------+---------------+----------+-----------+----------+\n";
    std::cout << "| Strategy Name       | Requested  | Edge Flow  | Origin Sp. | Total Flow | Unserved Flow | Avg Cost | Avg Lat.  | Runtime  |\n";
    std::cout << "+---------------------+------------+------------+------------+------------+---------------+----------+-----------+----------+\n";

    for (const auto& r : results) {
        std::cout << "| " << std::left << std::setw(19) << r.strategyName
                  << " | " << std::right << std::setw(7) << std::fixed << std::setprecision(0) << r.totalTrafficRequested << " req"
                  << " | " << std::setw(7) << std::fixed << std::setprecision(0) << r.edgeTrafficRouted << " req"
                  << " | " << std::setw(7) << std::fixed << std::setprecision(0) << r.originTrafficRouted << " req"
                  << " | " << std::setw(7) << std::fixed << std::setprecision(0) << r.totalTrafficRouted << " req"
                  << " | " << std::setw(8) << std::fixed << std::setprecision(0) << r.unservedTraffic << " req"
                  << " | " << std::setw(6) << std::fixed << std::setprecision(2) << r.averageRoutingCostPerReq << " $/r"
                  << " | " << std::setw(6) << std::fixed << std::setprecision(2) << r.averageLatencyMs << " ms"
                  << " | " << std::setw(6) << r.executionTimeMicroseconds << " us |\n";
    }
    std::cout << "+---------------------+------------+------------+------------+------------+---------------+----------+-----------+----------+\n";

    std::cout << "\n--- [Edge Server Utilization Breakdown (%)] ---\n";
    std::cout << "+---------------------+";
    if (!results.empty()) {
        for (const auto& pair : results[0].serverUtilPct) {
            std::cout << " " << std::left << std::setw(16) << pair.first << " |";
        }
    }
    std::cout << "\n+---------------------+";
    if (!results.empty()) {
        for (size_t i = 0; i < results[0].serverUtilPct.size(); ++i) {
            std::cout << "------------------+";
        }
    }
    std::cout << "\n";

    for (const auto& r : results) {
        std::cout << "| " << std::left << std::setw(19) << r.strategyName << " |";
        for (const auto& pair : r.serverUtilPct) {
            std::cout << " " << std::right << std::setw(14) << std::fixed << std::setprecision(1) << pair.second << "% |";
        }
        std::cout << "\n";
    }
    std::cout << "+---------------------+";
    if (!results.empty()) {
        for (size_t i = 0; i < results[0].serverUtilPct.size(); ++i) {
            std::cout << "------------------+";
        }
    }
    std::cout << "\n========================================================================================================================\n\n";
}

std::string BaselineRouters::exportResultsJson(const std::string& scenarioTitle, 
                                              const std::vector<StrategyResult>& results) {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"scenario\": \"" << scenarioTitle << "\",\n";
    ss << "  \"strategies\": [\n";

    for (size_t i = 0; i < results.size(); ++i) {
        const auto& r = results[i];
        ss << "    {\n";
        ss << "      \"strategyName\": \"" << r.strategyName << "\",\n";
        ss << "      \"totalTrafficRequested\": " << r.totalTrafficRequested << ",\n";
        ss << "      \"edgeTrafficRouted\": " << r.edgeTrafficRouted << ",\n";
        ss << "      \"originTrafficRouted\": " << r.originTrafficRouted << ",\n";
        ss << "      \"totalTrafficRouted\": " << r.totalTrafficRouted << ",\n";
        ss << "      \"unservedTraffic\": " << r.unservedTraffic << ",\n";
        ss << "      \"totalRoutingCost\": " << r.totalRoutingCost << ",\n";
        ss << "      \"averageRoutingCostPerReq\": " << r.averageRoutingCostPerReq << ",\n";
        ss << "      \"averageLatencyMs\": " << r.averageLatencyMs << ",\n";
        ss << "      \"executionTimeMicroseconds\": " << r.executionTimeMicroseconds << ",\n";

        // Server utilization map
        ss << "      \"serverUtilPct\": {";
        size_t sIdx = 0;
        for (const auto& pair : r.serverUtilPct) {
            ss << "\"" << pair.first << "\": " << pair.second;
            if (++sIdx < r.serverUtilPct.size()) ss << ", ";
        }
        ss << "},\n";

        // Server loads map
        ss << "      \"serverLoads\": {";
        sIdx = 0;
        for (const auto& pair : r.serverLoads) {
            ss << "\"" << pair.first << "\": " << pair.second;
            if (++sIdx < r.serverLoads.size()) ss << ", ";
        }
        ss << "}\n";

        ss << "    }" << (i + 1 < results.size() ? "," : "") << "\n";
    }

    ss << "  ]\n";
    ss << "}\n";
    return ss.str();
}

std::string BaselineRouters::exportSingleResultJson(const Network& network, 
                                                   const StrategyResult& r, 
                                                   const std::string& scenarioName, 
                                                   const std::string& explanation) {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"strategyName\": \"" << r.strategyName << "\",\n";
    ss << "  \"scenarioName\": \"" << scenarioName << "\",\n";
    ss << "  \"totalTrafficRequested\": " << r.totalTrafficRequested << ",\n";
    ss << "  \"edgeTrafficRouted\": " << r.edgeTrafficRouted << ",\n";
    ss << "  \"originTrafficRouted\": " << r.originTrafficRouted << ",\n";
    ss << "  \"totalTrafficRouted\": " << r.totalTrafficRouted << ",\n";
    ss << "  \"unservedTraffic\": " << r.unservedTraffic << ",\n";
    ss << "  \"totalRoutingCost\": " << r.totalRoutingCost << ",\n";
    ss << "  \"averageRoutingCostPerReq\": " << r.averageRoutingCostPerReq << ",\n";
    ss << "  \"averageLatencyMs\": " << r.averageLatencyMs << ",\n";
    ss << "  \"executionTimeMicroseconds\": " << r.executionTimeMicroseconds << ",\n";

    // Server loads
    ss << "  \"serverLoads\": {";
    size_t idx = 0;
    for (const auto& pair : r.serverLoads) {
        ss << "\"" << pair.first << "\": " << pair.second;
        if (++idx < r.serverLoads.size()) ss << ", ";
    }
    ss << "},\n";

    // Server utilization %
    ss << "  \"serverUtilPct\": {";
    idx = 0;
    for (const auto& pair : r.serverUtilPct) {
        ss << "\"" << pair.first << "\": " << pair.second;
        if (++idx < r.serverUtilPct.size()) ss << ", ";
    }
    ss << "},\n";

    // Link loads
    ss << "  \"linkLoads\": {";
    idx = 0;
    for (const auto& pair : r.linkLoads) {
        ss << "\"" << pair.first << "\": " << pair.second;
        if (++idx < r.linkLoads.size()) ss << ", ";
    }
    ss << "},\n";

    // Link utilization %
    ss << "  \"linkUtilPct\": {";
    idx = 0;
    for (const auto& pair : r.linkUtilPct) {
        ss << "\"" << pair.first << "\": " << pair.second;
        if (++idx < r.linkUtilPct.size()) ss << ", ";
    }
    ss << "},\n";

    // Regional demands
    ss << "  \"regionalDemands\": {";
    const auto& regions = network.getRegions();
    for (size_t i = 0; i < regions.size(); ++i) {
        ss << "\"" << regions[i].name << "\": " << regions[i].trafficDemand;
        if (i + 1 < regions.size()) ss << ", ";
    }
    ss << "},\n";

    // Failed servers list
    ss << "  \"failedServers\": [";
    std::vector<std::string> failed;
    for (const auto& s : network.getServers()) {
        if (!s.isActive) failed.push_back(s.name);
    }
    for (size_t i = 0; i < failed.size(); ++i) {
        ss << "\"" << failed[i] << "\"" << (i + 1 < failed.size() ? ", " : "");
    }
    ss << "],\n";

    // Congested links list
    ss << "  \"congestedLinks\": [";
    std::vector<std::string> congested;
    for (const auto& l : network.getLinks()) {
        if (l.congestionLatencyMs > 0.0) {
            congested.push_back(l.regionName + "->" + l.serverName + " (+" + std::to_string(l.congestionLatencyMs) + "ms)");
        }
    }
    for (size_t i = 0; i < congested.size(); ++i) {
        ss << "\"" << congested[i] << "\"" << (i + 1 < congested.size() ? ", " : "");
    }
    ss << "],\n";

    // Allocations
    ss << "  \"allocations\": [\n";
    for (size_t i = 0; i < r.allocations.size(); ++i) {
        const auto& a = r.allocations[i];
        ss << "    {\"from\": \"" << a.fromNode << "\", \"to\": \"" << a.toNode
           << "\", \"flow\": " << a.flow << ", \"capacity\": " << a.capacity
           << ", \"unitCost\": " << a.unitCostContinuous
           << ", \"totalCost\": " << a.totalCostContinuous
           << ", \"label\": \"" << a.label << "\"}" << (i + 1 < r.allocations.size() ? "," : "") << "\n";
    }
    ss << "  ],\n";

    // Explanation
    ss << "  \"explanation\": \"" << explanation << "\"";
    if (r.trace.totalSteps > 0) {
        ss << ",\n  \"trace\": " << r.trace.toJson() << "\n";
    } else {
        ss << "\n";
    }
    ss << "}\n";
    return ss.str();
}

std::string BaselineRouters::exportNetworkJson(const Network& network) {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"totalDemand\": " << network.getTotalTrafficDemand() << ",\n";
    ss << "  \"totalActiveCapacity\": " << network.getTotalActiveServerCapacity() << ",\n";

    // Regions
    ss << "  \"regions\": [\n";
    const auto& regions = network.getRegions();
    for (size_t i = 0; i < regions.size(); ++i) {
        ss << "    {\"name\": \"" << regions[i].name << "\", \"trafficDemand\": " << regions[i].trafficDemand << "}"
           << (i + 1 < regions.size() ? "," : "") << "\n";
    }
    ss << "  ],\n";

    // Servers
    ss << "  \"servers\": [\n";
    const auto& servers = network.getServers();
    for (size_t i = 0; i < servers.size(); ++i) {
        ss << "    {\"name\": \"" << servers[i].name << "\", \"configuredCapacity\": " << servers[i].configuredCapacity
           << ", \"availableCapacity\": " << servers[i].availableCapacity << ", \"isActive\": " << (servers[i].isActive ? "true" : "false") << "}"
           << (i + 1 < servers.size() ? "," : "") << "\n";
    }
    ss << "  ],\n";

    // Links
    ss << "  \"links\": [\n";
    const auto& links = network.getLinks();
    for (size_t i = 0; i < links.size(); ++i) {
        ss << "    {\"regionName\": \"" << links[i].regionName << "\", \"serverName\": \"" << links[i].serverName
           << "\", \"baseLatencyMs\": " << links[i].baseLatencyMs << ", \"congestionLatencyMs\": " << links[i].congestionLatencyMs
           << ", \"effectiveLatencyMs\": " << links[i].getEffectiveLatency() << ", \"bandwidthCost\": " << links[i].bandwidthCost
           << ", \"bandwidthCapacity\": " << links[i].bandwidthCapacity << ", \"cacheHitRatio\": " << links[i].cacheHitRatio << "}"
           << (i + 1 < links.size() ? "," : "") << "\n";
    }
    ss << "  ]\n";
    ss << "}\n";
    return ss.str();
}
