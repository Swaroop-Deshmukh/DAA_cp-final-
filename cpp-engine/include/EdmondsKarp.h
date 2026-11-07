#ifndef EDMONDS_KARP_H
#define EDMONDS_KARP_H

#include "Graph.h"
#include "DecisionTrace.h"
#include <vector>
#include <string>

/**
 * @brief Represents a bottleneck edge crossing the minimum cut (S_cut -> T_cut).
 */
struct CutEdge {
    int fromNodeId;
    int toNodeId;
    std::string fromNodeName;
    std::string toNodeName;
    double capacity;
    double flow;
    std::string label;
    bool isOriginRoute;     // True if this edge connects to/from the Origin server
};

/**
 * @brief Represents the Minimum Cut partitioning of the residual network.
 * 
 * According to the Max-Flow Min-Cut Theorem:
 * The maximum value of an (s, t)-flow is strictly equal to the minimum capacity
 * of an (s, t)-cut.
 */
struct MinCutResult {
    double minCutCapacity;              // Sum of capacities of all forward edges crossing from S_cut to T_cut
    double edgeFleetCutCapacity;        // Sum of capacities of bottleneck edges excluding Origin fallback routes
    std::vector<int> sourcePartition;   // Nodes reachable from source in residual graph G_f
    std::vector<int> sinkPartition;     // Nodes unreachable from source in residual graph G_f
    std::vector<CutEdge> bottleneckEdges; // Specific saturated edges forming the bottleneck
};

/**
 * @brief High-level output of the Edmonds-Karp maximum flow algorithm.
 */
struct EdmondsKarpResult {
    double maxFlow;                     // Total maximum flow delivered from source to sink
    int iterations;                     // Number of BFS augmenting path iterations
    MinCutResult minCut;                // Corresponding minimum cut bottleneck analysis
};

/**
 * @brief Edmonds-Karp Maximum Flow and Minimum Cut Algorithm.
 * 
 * Implementation Details:
 * ------------------------
 * 1. Uses Breadth-First Search (BFS) to find the shortest augmenting path (fewest edges) from S to T.
 * 2. Augments flow along the bottleneck capacity delta.
 * 3. Guaranteed termination in O(V * E^2) time complexity.
 * 4. Independently validates the correctness of the primary Min-Cost Max-Flow algorithm.
 * 5. Identifies the minimum cut bottleneck partitions:
 *    - S_cut: Vertices reachable from S via residual edges with capacity > 0.
 *    - T_cut: Vertices unreachable from S.
 *    - Bottleneck edges: Forward edges with u in S_cut and v in T_cut (fully saturated).
 */
class EdmondsKarp {
public:
    static constexpr double EPSILON = 1e-9;

    /**
     * @brief Computes maximum flow from source to sink using BFS shortest augmenting paths.
     * @param graph Residual network (mutated in-place with flow).
     * @param source Source node ID.
     * @param sink Sink node ID.
     * @return EdmondsKarpResult with max flow, iterations, and min-cut analysis.
     */
    static EdmondsKarpResult calculateMaxFlow(
        Graph& graph, 
        int source, 
        int sink, 
        DecisionTrace* trace = nullptr,
        double targetDemand = 0.0,
        long long costScalingFactor = 100
    );

    /**
     * @brief Computes the minimum cut on an already saturated residual graph.
     * @param graph The residual graph after maximum flow calculation.
     * @param source Source node ID.
     * @param sink Sink node ID.
     * @return MinCutResult containing reachable sets and saturated cut edges.
     */
    static MinCutResult findMinimumCut(const Graph& graph, int source, int sink);

    /**
     * @brief Formats and displays the minimum cut bottleneck diagnostic report.
     * @param graph Reference to graph for node metadata.
     * @param minCut The calculated min-cut result.
     * @param excludeOrigin Set to true to view edge-only physical bottlenecks without fallback.
     */
    static void printMinCutReport(const Graph& graph, const MinCutResult& minCut, bool excludeOrigin = false);
};

#endif // EDMONDS_KARP_H

