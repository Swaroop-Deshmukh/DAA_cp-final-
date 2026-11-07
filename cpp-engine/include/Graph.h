#ifndef GRAPH_H
#define GRAPH_H

#include "Network.h"
#include "CostCalculator.h"
#include <string>
#include <vector>
#include <map>
#include <iostream>

/**
 * @brief Categorization of nodes within the CDN flow network.
 */
enum class NodeType {
    SOURCE,         // Virtual Super-Source S
    REGION,         // Client traffic demand zone
    SERVER,         // CDN Edge Server
    ORIGIN,         // Central Origin Server (Fallback)
    SINK            // Virtual Super-Sink T
};

/**
 * @brief Represents a directed edge in the residual flow network.
 * 
 * In a flow network for Min-Cost Max-Flow:
 * - Every forward edge (u -> v) has capacity C and unit cost W.
 * - Every reverse edge (v -> u) has initial capacity 0 and unit cost -W.
 * - reverseEdgeIndex provides O(1) direct indexing into the target node's adjacency list.
 */
struct FlowEdge {
    int from;                   // Source node ID
    int to;                     // Target node ID
    double capacity;            // Maximum flow capacity in req/s
    double flow;                // Currently allocated flow in req/s
    long long unitCost;         // Scaled integer cost per unit flow
    int reverseEdgeIndex;       // Index of the paired residual edge in adjList[to]
    std::string label;          // Descriptive edge label (e.g., "Pune -> Pune Server")
    bool isReverse;             // True if this is a residual backward edge

    FlowEdge(int u, int v, double cap, long long cost, int revIdx, 
             const std::string& edgeLabel = "", bool reverse = false);

    // Residual capacity available along this edge
    double getResidualCapacity() const;
};

/**
 * @brief Metadata for a node in the flow network.
 */
struct NodeInfo {
    int id;                     // Unique integer node ID
    std::string name;           // Human-readable node name
    NodeType type;              // Semantic category of the node
};

/**
 * @brief Directed Flow Network graph for CDN traffic routing.
 * 
 * Converts the physical CDN network into a multi-layer directed flow graph:
 *   [Source S]
 *       | (capacity = demand, cost = 0)
 *       v
 *   [Client Regions]
 *       | (capacity = link bandwidth, cost = calculated multi-factor cost)
 *       v
 *   [Edge Servers]  ---> [Origin Fallback]
 *       | (4 parallel load bands with convex costs 0, 6, 15, 60)
 *       v
 *   [Sink T]
 */
class Graph {
private:
    int sourceId;                                       // Super-Source S ID
    int sinkId;                                         // Super-Sink T ID
    int originId;                                       // Origin Server ID
    std::vector<NodeInfo> nodes;                        // Metadata indexed by node ID
    std::map<std::string, int> nodeNameToId;            // Fast lookup from name to ID
    std::vector<std::vector<FlowEdge>> adjList;         // Adjacency list: adjList[u] = list of edges out of u

    // Helper to register nodes and assign stable IDs
    int registerNode(const std::string& name, NodeType type);

public:
    Graph();

    /**
     * @brief Builds the complete CDN flow graph from Network and CostCalculator configurations.
     * @param network The CDN topology containing regions, servers, and links.
     * @param calculator The cost calculator for multi-factor edge weight evaluation.
     */
    void buildFromNetwork(const Network& network, const CostCalculator& calculator);

    /**
     * @brief Adds a directed forward edge and its corresponding residual backward edge.
     * @param u Source node ID.
     * @param v Destination node ID.
     * @param capacity Maximum capacity in req/s.
     * @param unitCost Scaled integer cost per unit flow.
     * @param label Human-readable description.
     */
    void addEdge(int u, int v, double capacity, long long unitCost, const std::string& label = "");

    // Node accessors
    int getSourceId() const;
    int getSinkId() const;
    int getOriginId() const;
    int getNodeCount() const;
    size_t getEdgeCount() const;

    int getNodeId(const std::string& name) const;
    const NodeInfo& getNodeInfo(int id) const;
    const std::vector<NodeInfo>& getAllNodes() const;

    // Adjacency list accessors
    const std::vector<FlowEdge>& getEdgesFrom(int u) const;
    std::vector<FlowEdge>& getEdgesFrom(int u);
    const std::vector<std::vector<FlowEdge>>& getAdjacencyList() const;
    std::vector<std::vector<FlowEdge>>& getAdjacencyList();

    /**
     * @brief Prints a formatted, human-readable summary of the entire flow graph.
     */
    void printGraphSummary() const;

    /**
     * @brief Verifies whether the sink T is reachable from source S via edges with positive capacity.
     * Uses Breadth-First Search (BFS).
     * @return true if a valid augmenting path can exist, false otherwise.
     */
    bool isSinkReachable() const;
};

#endif // GRAPH_H

