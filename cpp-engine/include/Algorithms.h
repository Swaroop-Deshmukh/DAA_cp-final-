#ifndef ALGORITHMS_H
#define ALGORITHMS_H

#include "Graph.h"
#include "DecisionTrace.h"
#include <vector>
#include <string>
#include <limits>

/**
 * @brief Represents the high-level result of running Min-Cost Max-Flow.
 */
struct MCMFResult {
    double maxFlow;                 // Total traffic flow delivered from Source to Sink (req/s)
    double totalContinuousCost;     // Total routing cost in original continuous units ($ / cost units)
    long long totalScaledCost;      // Total cost in scaled integer flow units
    int iterations;                 // Total number of augmenting paths pushed
    bool isOptimal;                 // True if algorithm terminated with optimal max-flow solution
    std::string terminationReason;  // Human-readable termination status
};

/**
 * @brief Represents flow allocated to a specific edge for reporting and dashboard display.
 */
struct FlowAllocation {
    std::string fromNode;           // Originating node name
    std::string toNode;             // Destination node name
    double flow;                    // Allocated traffic in req/s
    double capacity;                // Edge capacity limit in req/s
    double unitCostContinuous;      // Unit cost in original continuous units
    double totalCostContinuous;     // flow * unitCostContinuous
    std::string label;              // Edge description
};

/**
 * @brief Implementation of Min-Cost Max-Flow using Successive Shortest Path
 * with Dijkstra and Johnson Potentials.
 * 
 * Mathematical Formulation:
 * -------------------------
 * In residual graphs, reverse edges have negative costs: cost(v, u) = -cost(u, v).
 * Ordinary Dijkstra's algorithm fails with negative edge weights.
 * 
 * Johnson Potentials assign each vertex v a dual potential pi(v) such that
 * the reduced edge cost:
 *     reducedCost(u, v) = cost(u, v) + pi(u) - pi(v)
 * is strictly non-negative (>= 0) for all residual edges with available capacity.
 * 
 * This enables Dijkstra's algorithm with a min-priority queue to find the minimum-cost
 * augmenting path in O(E log V) per iteration, while preserving exact shortest path optimality.
 */
class MinCostMaxFlow {
public:
    static constexpr long long INF_COST = std::numeric_limits<long long>::max() / 4;
    static constexpr double EPSILON = 1e-9;

    /**
     * @brief Initializes node potentials pi(v).
     * For graphs where all initial forward edge costs are non-negative, pi is set to 0.
     * For general graphs, Bellman-Ford/SPFA can be used.
     * @param graph The residual flow network.
     * @param source Source node ID.
     * @param potentials Vector of size V to store node potentials.
     * @return true if potentials initialized successfully, false if negative cycle detected.
     */
    static bool initializePotentials(const Graph& graph, int source, std::vector<long long>& potentials);

    /**
     * @brief Finds the cheapest augmenting path from source to sink using Dijkstra on reduced costs.
     * @param graph The residual flow network.
     * @param source Source node ID.
     * @param sink Sink node ID.
     * @param potentials Current vertex potentials pi.
     * @param dist Output vector of shortest reduced distances from source.
     * @param parentNode Output vector tracking previous node on the shortest path.
     * @param parentEdgeIdx Output vector tracking edge index in adjList[parentNode].
     * @return true if an augmenting path with residual capacity exists, false if sink is unreachable.
     */
    static bool findCheapestPath(const Graph& graph, int source, int sink,
                                const std::vector<long long>& potentials,
                                std::vector<long long>& dist,
                                std::vector<int>& parentNode,
                                std::vector<int>& parentEdgeIdx,
                                std::vector<DijkstraRelaxationDto>* relaxations = nullptr);

    /**
     * @brief Updates node potentials using the shortest distances found by Dijkstra.
     * pi'(v) = pi(v) + dist(v) for all reachable nodes.
     * @param potentials Potentials vector to update in-place.
     * @param dist Distance vector computed by Dijkstra.
     * @param shifts Optional vector to capture structured potential transitions.
     * @param graph Optional pointer to graph for node metadata resolution.
     */
    static void updatePotentials(std::vector<long long>& potentials, const std::vector<long long>& dist,
                                 std::vector<PotentialShiftDto>* shifts = nullptr,
                                 const Graph* graph = nullptr);

    /**
     * @brief Augments flow along the shortest path found by Dijkstra.
     * Finds bottleneck residual capacity delta, updates forward and reverse flows.
     * @param graph Flow graph to mutate in-place.
     * @param source Source node ID.
     * @param sink Sink node ID.
     * @param parentNode Parent node pointers along shortest path.
     * @param parentEdgeIdx Edge index pointers along shortest path.
     * @param maxAllowedFlow Optional cap on flow to push (for target flow satisfaction).
     * @return Amount of traffic flow pushed (delta in req/s).
     */
    static double sendTraffic(Graph& graph, int source, int sink,
                             const std::vector<int>& parentNode,
                             const std::vector<int>& parentEdgeIdx,
                             double maxAllowedFlow = -1.0);

    /**
     * @brief Executes the complete Successive Shortest Path Min-Cost Max-Flow algorithm.
     * @param graph Flow network (mutated in-place with optimal flow assignments).
     * @param source Source node ID.
     * @param sink Sink node ID.
     * @param targetFlow Optional maximum flow limit; if -1, pushes maximum feasible flow.
     * @param costScalingFactor The scaling factor used during graph construction (e.g. 100).
     * @return MCMFResult containing total flow, total cost, iterations, and status.
     */
    static MCMFResult calculateMinCostMaxFlow(Graph& graph, int source, int sink,
                                            double targetFlow = -1.0,
                                            long long costScalingFactor = 100,
                                            DecisionTrace* trace = nullptr);

    /**
     * @brief Extracts non-zero forward flow assignments across the network for explainability.
     */
    static std::vector<FlowAllocation> extractFlowAllocations(const Graph& graph,
                                                            long long costScalingFactor = 100);

    /**
     * @brief Formats and prints a detailed terminal report of routing allocations and costs.
     */
    static void printFlowSummary(const Graph& graph, const MCMFResult& result,
                                 long long costScalingFactor = 100);
};

#endif // ALGORITHMS_H

