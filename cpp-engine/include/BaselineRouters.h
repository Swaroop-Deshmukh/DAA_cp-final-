#ifndef BASELINE_ROUTERS_H
#define BASELINE_ROUTERS_H

#include "Network.h"
#include "CostCalculator.h"
#include "Graph.h"
#include "Algorithms.h"
#include "DecisionTrace.h"
#include <string>
#include <vector>
#include <map>
#include <chrono>

/**
 * @brief Unified performance metrics struct capturing outcomes across all routing strategies.
 */
struct StrategyResult {
    std::string strategyName;                     // Name of routing strategy
    double totalTrafficRequested = 0.0;           // Total regional demand requested (req/s)
    double edgeTrafficRouted = 0.0;               // Traffic absorbed by edge servers (req/s)
    double originTrafficRouted = 0.0;             // Overflow traffic spilled to Origin (req/s)
    double totalTrafficRouted = 0.0;              // Total successfully routed traffic (edge + origin)
    double unservedTraffic = 0.0;                 // Unserved traffic if capacity is completely exhausted
    double totalRoutingCost = 0.0;                // Total multi-factor routing cost ($ / cost units)
    double averageRoutingCostPerReq = 0.0;        // Cost per request (totalRoutingCost / totalTrafficRouted)
    double averageLatencyMs = 0.0;                // User-experienced physical network latency (ms)
    std::map<std::string, double> serverLoads;    // Allocated traffic per edge server (req/s)
    std::map<std::string, double> serverUtilPct;  // Server utilization % relative to configured capacity
    std::map<std::string, double> linkLoads;      // Allocated traffic per link "Region->Server" (req/s)
    std::map<std::string, double> linkUtilPct;    // Link utilization % relative to link bandwidth capacity
    long long executionTimeMicroseconds = 0;      // Runtime of algorithm in microseconds (us)
    std::vector<FlowAllocation> allocations;      // Individual route flow assignments
    DecisionTrace trace;                          // Full step-by-step decision trace for inspection
};

/**
 * @brief Benchmark suite implementing and evaluating baseline heuristics alongside MCMF.
 * 
 * Implemented Strategies:
 * 1. Round Robin: Cyclically balances regional traffic across healthy edge servers within capacity bounds.
 * 2. Nearest Server: Greedily routes traffic to lowest-latency servers, spilling over as servers fill.
 * 3. Greedy Cost: Greedily routes traffic to cheapest candidate servers based on Phase 3 cost calculation.
 * 4. Min-Cost Max-Flow: Global optimization engine balancing latency, bandwidth, cache miss, and load bands.
 */
class BaselineRouters {
public:
    static constexpr double EPSILON = 1e-6;

    /**
     * @brief Executes Round Robin routing.
     */
    static StrategyResult runRoundRobin(const Network& network, const CostCalculator& calculator, bool recordTrace = false);

    /**
     * @brief Executes Nearest Server (lowest latency first) routing.
     */
    static StrategyResult runNearestServer(const Network& network, const CostCalculator& calculator, bool recordTrace = false);

    /**
     * @brief Executes Greedy Cost (cheapest initial cost first) routing.
     */
    static StrategyResult runGreedyCost(const Network& network, const CostCalculator& calculator, bool recordTrace = false);

    /**
     * @brief Executes Min-Cost Max-Flow optimization routing.
     */
    static StrategyResult runMCMF(const Network& network, const CostCalculator& calculator, bool recordTrace = false);

    /**
     * @brief Executes all 4 strategies on the network and displays a formatted comparative benchmark matrix.
     */
    static std::vector<StrategyResult> compareAll(const Network& network, 
                                                 const CostCalculator& calculator, 
                                                 const std::string& scenarioTitle,
                                                 bool printTable = true);

    /**
     * @brief Formats and prints the comparative benchmark table.
     */
    static void printComparisonTable(const std::vector<StrategyResult>& results, 
                                     const std::string& scenarioTitle);

    /**
     * @brief Generates structured JSON representation of comparison results for frontend dashboard consumption.
     */
    static std::string exportResultsJson(const std::string& scenarioTitle, 
                                         const std::vector<StrategyResult>& results);

    /**
     * @brief Generates JSON serialization for a single strategy result.
     */
    static std::string exportSingleResultJson(const Network& network, 
                                             const StrategyResult& result, 
                                             const std::string& scenarioName, 
                                             const std::string& explanation = "");

    /**
     * @brief Generates JSON serialization of network topology and status.
     */
    static std::string exportNetworkJson(const Network& network);
};

#endif // BASELINE_ROUTERS_H

