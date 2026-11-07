#ifndef ROUTER_H
#define ROUTER_H

#include "Network.h"
#include "CostCalculator.h"
#include "Graph.h"
#include "Algorithms.h"
#include <string>
#include <vector>
#include <map>
#include <iostream>

/**
 * @brief Configurable threshold parameters governing the router's replanning policy.
 * 
 * Prevents unnecessary recomputations when network variations are negligible,
 * while guaranteeing immediate failover upon critical operational triggers.
 */
struct ReplanningPolicy {
    double demandChangeThresholdFraction;   // Fractional threshold (e.g., 0.10 = 10% change triggers replan)
    double latencyChangeThresholdMs;        // Absolute latency threshold (e.g., 5.0 ms increase triggers replan)
    bool replanOnServerStatusChange;        // Immediate replanning if any server fails or recovers

    ReplanningPolicy(double demandThreshold = 0.10,
                     double latencyThreshold = 5.0,
                     bool serverChange = true);
};

/**
 * @brief Autonomous Adaptive CDN Traffic Router.
 * 
 * Acts as the central coordinator between network monitoring, failure handling,
 * graph transformation, and the Min-Cost Max-Flow optimization algorithm.
 * 
 * Responsibilities:
 * 1. Reads live network state (demands, capacities, server health, link latencies).
 * 2. Evaluates the replanning policy against past plan snapshots.
 * 3. Triggers MCMF graph reconstruction and flow augmentation when necessary.
 * 4. Isolates failed servers, redistributing traffic to healthy edge servers or origin.
 * 5. Provides comparative differential analysis between successive routing plans.
 */
class Router {
private:
    Network& network;                           // Live CDN network reference
    CostCalculator calculator;                  // Routing cost evaluation engine
    ReplanningPolicy policy;                    // Active replanning thresholds

    Graph currentGraph;                         // Current flow graph
    MCMFResult currentResult;                   // Latest optimization result
    std::vector<FlowAllocation> currentAllocations; // Latest edge flow allocations

    // Snapshot of network conditions from the last plan calculation
    double lastPlannedTotalDemand;
    std::map<std::string, bool> lastServerStatus;
    std::map<std::string, double> lastLinkLatencies;
    bool hasInitialPlan;

    // Updates internal snapshot after successful replanning
    void recordCurrentStateSnapshot();

public:
    Router(Network& net, const CostCalculator& calc, const ReplanningPolicy& pol = ReplanningPolicy());

    // Policy configuration
    void setReplanningPolicy(const ReplanningPolicy& pol);
    const ReplanningPolicy& getReplanningPolicy() const;

    /**
     * @brief Evaluates whether network conditions justify a full route recalculation.
     * @param outReason Populated with the operational trigger explaining the decision.
     * @return true if replanning should occur, false if the existing plan should be retained.
     */
    bool shouldReplan(std::string& outReason) const;

    /**
     * @brief Evaluates current state and routes traffic.
     * Only executes Min-Cost Max-Flow if shouldReplan() triggers or if force is true.
     * @param force Bypasses threshold checks and forces an immediate replan.
     * @return Updated MCMFResult.
     */
    MCMFResult routeTraffic(bool force = false);

    // Accessors
    const Graph& getCurrentGraph() const;
    const MCMFResult& getCurrentResult() const;
    const std::vector<FlowAllocation>& getCurrentAllocations() const;

    // Formatted diagnostic display methods
    void displayCurrentRoutingPlan() const;

    /**
     * @brief Compares two routing allocation snapshots and displays exact traffic migrations.
     * @param beforeAlloc Allocations prior to the event.
     * @param afterAlloc Allocations after replanning.
     * @param eventTitle Description of the event triggering the migration.
     */
    void displayPlanComparison(const std::vector<FlowAllocation>& beforeAlloc,
                               const std::vector<FlowAllocation>& afterAlloc,
                               const std::string& eventTitle) const;
};

#endif // ROUTER_H

