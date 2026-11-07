#include "../include/EdmondsKarp.h"
#include <iostream>
#include <iomanip>
#include <queue>
#include <algorithm>

EdmondsKarpResult EdmondsKarp::calculateMaxFlow(Graph& graph, int source, int sink) {
    EdmondsKarpResult result;
    result.maxFlow = 0.0;
    result.iterations = 0;

    int n = graph.getNodeCount();
    std::vector<int> parent(n);
    std::vector<int> parentEdge(n);

    // Edmonds-Karp Loop: BFS finds augmenting path with fewest hops
    while (true) {
        std::fill(parent.begin(), parent.end(), -1);
        std::fill(parentEdge.begin(), parentEdge.end(), -1);

        std::queue<int> q;
        parent[source] = source;
        q.push(source);

        while (!q.empty()) {
            int u = q.front();
            q.pop();

            if (u == sink) break;

            const auto& edges = graph.getEdgesFrom(u);
            for (int eIdx = 0; eIdx < static_cast<int>(edges.size()); ++eIdx) {
                const auto& edge = edges[eIdx];
                int v = edge.to;

                // Traverse unvisited nodes along edges with positive residual capacity
                if (edge.getResidualCapacity() > EPSILON && parent[v] == -1) {
                    parent[v] = u;
                    parentEdge[v] = eIdx;
                    q.push(v);
                }
            }
        }

        // If sink was not reached, no augmenting path exists; max flow reached!
        if (parent[sink] == -1) break;

        // Find path bottleneck capacity delta
        double delta = std::numeric_limits<double>::max();
        int curr = sink;
        while (curr != source) {
            int u = parent[curr];
            int eIdx = parentEdge[curr];
            const auto& edge = graph.getEdgesFrom(u)[eIdx];
            delta = std::min(delta, edge.getResidualCapacity());
            curr = u;
        }

        if (delta <= EPSILON) break;

        // Augment flow along the path
        curr = sink;
        while (curr != source) {
            int u = parent[curr];
            int eIdx = parentEdge[curr];

            FlowEdge& fwdEdge = graph.getEdgesFrom(u)[eIdx];
            FlowEdge& revEdge = graph.getEdgesFrom(curr)[fwdEdge.reverseEdgeIndex];

            fwdEdge.flow += delta;
            revEdge.flow -= delta;

            curr = u;
        }

        result.maxFlow += delta;
        result.iterations++;
    }

    // Compute corresponding Minimum Cut
    result.minCut = findMinimumCut(graph, source, sink);
    return result;
}

MinCutResult EdmondsKarp::findMinimumCut(const Graph& graph, int source, int /*sink*/) {
    MinCutResult cut;
    cut.minCutCapacity = 0.0;
    cut.edgeFleetCutCapacity = 0.0;

    int n = graph.getNodeCount();
    std::vector<bool> visited(n, false);
    std::queue<int> q;

    // 1. BFS to find all nodes reachable from source in the residual graph G_f
    visited[source] = true;
    q.push(source);

    while (!q.empty()) {
        int u = q.front();
        q.pop();

        for (const auto& edge : graph.getEdgesFrom(u)) {
            if (edge.getResidualCapacity() > EPSILON && !visited[edge.to]) {
                visited[edge.to] = true;
                q.push(edge.to);
            }
        }
    }

    // 2. Partition vertices into S_cut (reachable) and T_cut (unreachable)
    for (int i = 0; i < n; ++i) {
        if (visited[i]) {
            cut.sourcePartition.push_back(i);
        } else {
            cut.sinkPartition.push_back(i);
        }
    }

    // 3. Identify saturated forward edges crossing from S_cut to T_cut
    int originId = graph.getOriginId();

    for (int u : cut.sourcePartition) {
        for (const auto& edge : graph.getEdgesFrom(u)) {
            // Forward edge with capacity > 0 whose destination is in T_cut (unvisited)
            if (!edge.isReverse && edge.capacity > EPSILON && !visited[edge.to]) {
                CutEdge cutEdge;
                cutEdge.fromNodeId = u;
                cutEdge.toNodeId = edge.to;
                cutEdge.fromNodeName = graph.getNodeInfo(u).name;
                cutEdge.toNodeName = graph.getNodeInfo(edge.to).name;
                cutEdge.capacity = edge.capacity;
                cutEdge.flow = edge.flow;
                cutEdge.label = edge.label;
                cutEdge.isOriginRoute = (u == originId || edge.to == originId);

                cut.minCutCapacity += edge.capacity;
                if (!cutEdge.isOriginRoute) {
                    cut.edgeFleetCutCapacity += edge.capacity;
                }
                cut.bottleneckEdges.push_back(cutEdge);
            }
        }
    }

    return cut;
}

void EdmondsKarp::printMinCutReport(const Graph& /*graph*/, const MinCutResult& minCut, bool excludeOrigin) {
    std::cout << "\n========================================================================================\n";
    std::cout << "                 MINIMUM CUT & BOTTLENECK ANALYSIS REPORT                               \n";
    std::cout << "========================================================================================\n";
    std::cout << "  Max-Flow Min-Cut Theorem Check : Total Cut Capacity = " 
              << std::fixed << std::setprecision(0) << minCut.minCutCapacity << " req/s\n";
    std::cout << "  Source Partition (S_cut) Size  : " << minCut.sourcePartition.size() << " nodes reachable in G_f\n";
    std::cout << "  Sink Partition (T_cut) Size    : " << minCut.sinkPartition.size() << " nodes saturated/cut off\n";
    std::cout << "  Origin Fallback Included?      : " << (excludeOrigin ? "EXCLUDED (Edge-only bottlenecks)" : "INCLUDED (Full topology)") << "\n\n";

    std::cout << "--- [Saturated Bottleneck Edges Crossing the Cut (S_cut -> T_cut)] ---\n";
    std::cout << "+------------------------------------------+--------------------+--------------------+----------------+\n";
    std::cout << "| Bottleneck Link                          | Capacity (req/s)   | Flow Pushed        | Saturation (%) |\n";
    std::cout << "+------------------------------------------+--------------------+--------------------+----------------+\n";

    double displayedCutCapacity = 0.0;
    int edgeCount = 0;

    for (const auto& edge : minCut.bottleneckEdges) {
        if (excludeOrigin && edge.isOriginRoute) {
            continue;
        }

        displayedCutCapacity += edge.capacity;
        edgeCount++;

        double saturationPct = (edge.capacity > 0.0) ? (edge.flow / edge.capacity) * 100.0 : 0.0;
        std::cout << "| " << std::left << std::setw(40) << edge.label
                  << " | " << std::right << std::setw(18) << std::fixed << std::setprecision(0) << edge.capacity
                  << " | " << std::setw(18) << std::fixed << std::setprecision(0) << edge.flow
                  << " | " << std::setw(13) << std::fixed << std::setprecision(1) << saturationPct << "% |\n";
    }

    std::cout << "+------------------------------------------+--------------------+--------------------+----------------+\n";
    std::cout << "  Bottleneck Edges Displayed : " << edgeCount << "\n";
    std::cout << "  Effective Bottleneck Limit : " << std::fixed << std::setprecision(0) << displayedCutCapacity << " req/s\n";
    std::cout << "========================================================================================\n\n";
}

