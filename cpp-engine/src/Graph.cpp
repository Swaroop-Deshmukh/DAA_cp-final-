#include "../include/Graph.h"
#include <iostream>
#include <iomanip>
#include <queue>

// ==========================================
// FlowEdge Implementation
// ==========================================
FlowEdge::FlowEdge(int u, int v, double cap, long long cost, int revIdx, 
                   const std::string& edgeLabel, bool reverse)
    : from(u),
      to(v),
      capacity(cap),
      flow(0.0),
      unitCost(cost),
      reverseEdgeIndex(revIdx),
      label(edgeLabel),
      isReverse(reverse) {}

double FlowEdge::getResidualCapacity() const {
    return capacity - flow;
}

// ==========================================
// Graph Implementation
// ==========================================
Graph::Graph() : sourceId(-1), sinkId(-1), originId(-1) {}

int Graph::registerNode(const std::string& name, NodeType type) {
    int id = static_cast<int>(nodes.size());
    nodes.push_back({id, name, type});
    nodeNameToId[name] = id;
    adjList.emplace_back();
    return id;
}

void Graph::addEdge(int u, int v, double capacity, long long unitCost, const std::string& label) {
    int uIdx = static_cast<int>(adjList[u].size());
    int vIdx = static_cast<int>(adjList[v].size());

    // Forward edge (capacity = C, cost = W)
    FlowEdge forwardEdge(u, v, capacity, unitCost, vIdx, label, false);

    // Residual reverse edge (initial capacity = 0, cost = -W)
    FlowEdge reverseEdge(v, u, 0.0, -unitCost, uIdx, label + " [Residual]", true);

    adjList[u].push_back(forwardEdge);
    adjList[v].push_back(reverseEdge);
}

void Graph::buildFromNetwork(const Network& network, const CostCalculator& calculator) {
    // 1. Clear existing graph
    nodes.clear();
    nodeNameToId.clear();
    adjList.clear();

    // 2. Register stable node IDs
    // S = 0
    sourceId = registerNode("Source (S)", NodeType::SOURCE);

    // Client Region nodes
    for (const auto& region : network.getRegions()) {
        registerNode(region.name, NodeType::REGION);
    }

    // CDN Edge Server nodes
    for (const auto& server : network.getServers()) {
        registerNode(server.name, NodeType::SERVER);
    }

    // Origin Server node
    originId = registerNode("Origin Server", NodeType::ORIGIN);

    // Sink T node
    sinkId = registerNode("Sink (T)", NodeType::SINK);

    long long scale = calculator.getCostScalingFactor();

    // 3. Edges: Layer 1 -> Source to Region (Capacity = regional demand, Cost = 0)
    for (const auto& region : network.getRegions()) {
        int u = sourceId;
        int v = getNodeId(region.name);
        addEdge(u, v, region.trafficDemand, 0, "Source -> " + region.name);
    }

    // 4. Edges: Layer 2 -> Region to Server (Capacity = link bandwidth, Cost = calculated routing cost)
    for (const auto& link : network.getLinks()) {
        int u = getNodeId(link.regionName);
        int v = getNodeId(link.serverName);
        const Server* server = network.getServer(link.serverName);

        // If server is active, available link capacity is used; if server failed, available capacity is 0
        double cap = (server && server->isActive) ? link.bandwidthCapacity : 0.0;

        RouteConnectionProfile profile(link.regionName, link.serverName, 
                                       link.getEffectiveLatency(), link.bandwidthCost, 
                                       link.cacheHitRatio, calculator.getDefaultMissPenalty());
        long long cost = calculator.getScaledEdgeCost(profile);

        addEdge(u, v, cap, cost, link.regionName + " -> " + link.serverName);
    }

    // 5. Edges: Layer 3 -> Server to Sink (4 Parallel Load Bands representing convex congestion costs)
    // Server capacity is divided into 4 equal bands (25% each):
    // Band 1: 0% - 25% load  -> Cost = 0
    // Band 2: 25% - 50% load -> Cost = 6 (scaled: 6 * scale)
    // Band 3: 50% - 75% load -> Cost = 15 (scaled: 15 * scale)
    // Band 4: 75% - 100% load -> Cost = 60 (scaled: 60 * scale)
    const long long bandBaseCosts[4] = {0, 6, 15, 60};

    for (const auto& server : network.getServers()) {
        int u = getNodeId(server.name);
        int v = sinkId;

        double bandCapacity = 0.0;
        if (server.isActive && server.availableCapacity > 0.0) {
            bandCapacity = server.availableCapacity / 4.0;
        }

        for (int band = 0; band < 4; ++band) {
            long long bandCost = bandBaseCosts[band] * scale;
            std::string bandLabel = server.name + " -> Sink [Band " + std::to_string(band + 1) + "]";
            addEdge(u, v, bandCapacity, bandCost, bandLabel);
        }
    }

    // 6. Edges: Layer 4 -> Region to Origin Fallback (High-cost fallback route)
    // In case edge servers are saturated or failed, traffic can route directly to the central origin
    for (const auto& region : network.getRegions()) {
        int u = getNodeId(region.name);
        int v = originId;

        // Fallback cost profile: high latency (100 ms), transit cost (5.0 $/unit), cache hit 0% (full origin miss)
        RouteConnectionProfile fallbackProfile(region.name, "Origin Server", 100.0, 5.0, 0.0, calculator.getDefaultMissPenalty());
        long long fallbackCost = calculator.getScaledEdgeCost(fallbackProfile);

        addEdge(u, v, region.trafficDemand, fallbackCost, region.name + " -> Origin [Fallback]");
    }

    // 7. Edges: Layer 5 -> Origin to Sink
    // Origin has sufficient capacity to absorb all fallback traffic with a baseline origin processing cost
    double totalDemand = network.getTotalTrafficDemand();
    long long originCost = 50 * scale; // High origin compute/egress cost (50 units)
    addEdge(originId, sinkId, totalDemand, originCost, "Origin -> Sink");
}

int Graph::getSourceId() const { return sourceId; }
int Graph::getSinkId() const { return sinkId; }
int Graph::getOriginId() const { return originId; }
int Graph::getNodeCount() const { return static_cast<int>(nodes.size()); }

size_t Graph::getEdgeCount() const {
    size_t count = 0;
    for (const auto& list : adjList) {
        for (const auto& edge : list) {
            if (!edge.isReverse) {
                count++;
            }
        }
    }
    return count;
}

int Graph::getNodeId(const std::string& name) const {
    auto it = nodeNameToId.find(name);
    if (it != nodeNameToId.end()) {
        return it->second;
    }
    return -1;
}

const NodeInfo& Graph::getNodeInfo(int id) const {
    return nodes.at(id);
}

const std::vector<NodeInfo>& Graph::getAllNodes() const {
    return nodes;
}

const std::vector<FlowEdge>& Graph::getEdgesFrom(int u) const {
    return adjList.at(u);
}

std::vector<FlowEdge>& Graph::getEdgesFrom(int u) {
    return adjList.at(u);
}

const std::vector<std::vector<FlowEdge>>& Graph::getAdjacencyList() const {
    return adjList;
}

std::vector<std::vector<FlowEdge>>& Graph::getAdjacencyList() {
    return adjList;
}

bool Graph::isSinkReachable() const {
    if (sourceId < 0 || sinkId < 0) return false;

    std::vector<bool> visited(nodes.size(), false);
    std::queue<int> q;

    visited[sourceId] = true;
    q.push(sourceId);

    while (!q.empty()) {
        int u = q.front();
        q.pop();

        if (u == sinkId) return true;

        for (const auto& edge : adjList[u]) {
            // Forward traverse along edges with available capacity
            if (!edge.isReverse && edge.capacity > 0.0 && !visited[edge.to]) {
                visited[edge.to] = true;
                q.push(edge.to);
            }
        }
    }

    return false;
}

void Graph::printGraphSummary() const {
    std::cout << "\n========================================================================================\n";
    std::cout << "                      CDN DIRECTED FLOW GRAPH SUMMARY                                   \n";
    std::cout << "========================================================================================\n";
    std::cout << "  Total Nodes in Graph : " << getNodeCount() << " (Source S, Regions, Servers, Origin, Sink T)\n";
    std::cout << "  Total Forward Edges  : " << getEdgeCount() 
              << " (Plus equal number of residual backward edges)\n\n";

    std::cout << "--- [Layer 1: Source S -> User Regions] ---\n";
    std::cout << "+--------------------------------------+--------------------+----------------+\n";
    std::cout << "| Connection                           | Capacity (req/s)   | Cost (Units)   |\n";
    std::cout << "+--------------------------------------+--------------------+----------------+\n";
    for (const auto& edge : adjList[sourceId]) {
        if (!edge.isReverse) {
            std::cout << "| " << std::left << std::setw(36) << edge.label
                      << " | " << std::right << std::setw(18) << std::fixed << std::setprecision(0) << edge.capacity
                      << " | " << std::setw(14) << edge.unitCost << " |\n";
        }
    }
    std::cout << "+--------------------------------------+--------------------+----------------+\n";

    std::cout << "\n--- [Layer 2: User Regions -> Edge Servers (Sample Connections)] ---\n";
    std::cout << "+--------------------------------------+--------------------+----------------+\n";
    std::cout << "| Connection                           | Capacity (req/s)   | Cost (Units)   |\n";
    std::cout << "+--------------------------------------+--------------------+----------------+\n";
    for (const auto& node : nodes) {
        if (node.type == NodeType::REGION) {
            for (const auto& edge : adjList[node.id]) {
                if (!edge.isReverse && nodes[edge.to].type == NodeType::SERVER) {
                    std::cout << "| " << std::left << std::setw(36) << edge.label
                              << " | " << std::right << std::setw(18) << std::fixed << std::setprecision(0) << edge.capacity
                              << " | " << std::setw(14) << edge.unitCost << " |\n";
                }
            }
        }
    }
    std::cout << "+--------------------------------------+--------------------+----------------+\n";

    std::cout << "\n--- [Layer 3: Edge Servers -> Sink T (4 Parallel Load Bands)] ---\n";
    std::cout << "+--------------------------------------+--------------------+----------------+\n";
    std::cout << "| Server Band                          | Capacity (req/s)   | Cost (Units)   |\n";
    std::cout << "+--------------------------------------+--------------------+----------------+\n";
    for (const auto& node : nodes) {
        if (node.type == NodeType::SERVER) {
            for (const auto& edge : adjList[node.id]) {
                if (!edge.isReverse && edge.to == sinkId) {
                    std::cout << "| " << std::left << std::setw(36) << edge.label
                              << " | " << std::right << std::setw(18) << std::fixed << std::setprecision(0) << edge.capacity
                              << " | " << std::setw(14) << edge.unitCost << " |\n";
                }
            }
        }
    }
    std::cout << "+--------------------------------------+--------------------+----------------+\n";

    std::cout << "\n--- [Layer 4 & 5: High-Cost Origin Fallback Routes] ---\n";
    std::cout << "+--------------------------------------+--------------------+----------------+\n";
    std::cout << "| Fallback Connection                  | Capacity (req/s)   | Cost (Units)   |\n";
    std::cout << "+--------------------------------------+--------------------+----------------+\n";
    for (const auto& node : nodes) {
        if (node.type == NodeType::REGION) {
            for (const auto& edge : adjList[node.id]) {
                if (!edge.isReverse && edge.to == originId) {
                    std::cout << "| " << std::left << std::setw(36) << edge.label
                              << " | " << std::right << std::setw(18) << std::fixed << std::setprecision(0) << edge.capacity
                              << " | " << std::setw(14) << edge.unitCost << " |\n";
                }
            }
        }
    }
    for (const auto& edge : adjList[originId]) {
        if (!edge.isReverse && edge.to == sinkId) {
            std::cout << "| " << std::left << std::setw(36) << edge.label
                      << " | " << std::right << std::setw(18) << std::fixed << std::setprecision(0) << edge.capacity
                      << " | " << std::setw(14) << edge.unitCost << " |\n";
        }
    }
    std::cout << "+--------------------------------------+--------------------+----------------+\n";
    std::cout << "========================================================================================\n\n";
}

