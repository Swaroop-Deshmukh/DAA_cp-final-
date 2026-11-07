# Algorithms & Theoretical Formulations

**Adaptive CDN Traffic Router**  
*Comprehensive algorithmic documentation, mathematical formulations, pseudocode, and complexity analysis.*

---

## 1. Overview of Algorithms

The Adaptive CDN Traffic Router employs a multi-tiered algorithmic architecture combining global network optimization, heuristic routing baselines, independent flow verification, dynamic programming cache optimization, and exponential smoothing demand forecasting.

```
+---------------------------------------------------------------------------------------------------+
|                                      ADAPTIVE CDN ALGORITHM SUITE                                 |
+------------------------------------+----------------------------------+---------------------------+
| Core Global Optimization           | Baseline Heuristic Routers       | Supporting DAA Algorithms |
| - Min-Cost Max-Flow (SSP)          | - Round Robin                    | - Edmonds-Karp Max-Flow   |
| - Johnson Potentials               | - Nearest Server (Min-Latency)   | - Min-Cut Bottleneck Part |
| - Dijkstra with Fibonacci/Min-Heap | - Greedy Cost-Based              | - 0/1 Knapsack DP Cache   |
|                                    |                                  | - Holt Linear Smoothing   |
+------------------------------------+----------------------------------+---------------------------+
```

---

## 2. Min-Cost Max-Flow (MCMF) Routing Engine

### Objective Function
Given a directed flow graph $G = (V, E)$, source $S$, sink $T$, and link capacities $u(e)$ with costs $c(e)$, the algorithm solves the linear program:

$$\min \sum_{e \in E} c(e) \cdot f(e)$$

Subject to:
1. **Capacity Constraints:** $0 \le f(e) \le u(e), \quad \forall e \in E$
2. **Flow Conservation:**
   $$\sum_{w : (v, w) \in E} f(v, w) - \sum_{u : (u, v) \in E} f(u, v) = \begin{cases} |f| & v = S \\ -|f| & v = T \\ 0 & v \notin \{S, T\} \end{cases}$$
3. **Maximum Flow Maximization:** $|f| = \min(D_{\text{total}}, \text{Cut}_{\min}(S, T))$

### Successive Shortest Path (SSP) with Johnson Dual Potentials
Standard shortest-path augmentation fails when residual graphs contain negative-cost reverse edges (created when pushing flow along forward edges, $c(v, u) = -c(u, v)$). Bellman-Ford can handle negative costs but runs in $O(V \cdot E)$, making repeated augmentations prohibitively expensive ($O(F \cdot V \cdot E)$).

To solve this, we maintain **dual vertex potentials** $\pi(u)$. The reduced cost $c^\pi(u, v)$ of an edge is defined as:
$$c^\pi(u, v) = c(u, v) + \pi(u) - \pi(v)$$

By linear programming duality, if $c^\pi(u, v) \ge 0$ for all residual edges $(u, v) \in E_f$, Dijkstra's algorithm using a binary min-heap can be executed in $O(E \log V)$ to find the cheapest augmenting path.

After computing shortest path distances $d(v)$ from $S$ to all nodes using $c^\pi$, vertex potentials are updated:
$$\pi'(v) = \pi(v) + d(v)$$

This ensures that reduced costs on all forward and newly created reverse residual edges remain non-negative for all subsequent iterations.

### Pseudocode: Min-Cost Max-Flow (SSP + Johnson)

```text
Algorithm MinCostMaxFlow(G, S, T, targetFlow):
    Initialize flow f(e) = 0 for all e in E
    Initialize potential pi(v) = 0 for all v in V
    totalCost = 0, totalFlow = 0

    while totalFlow < targetFlow:
        // Run Dijkstra with reduced costs c^pi(u, v)
        dist[v] = infinity for all v in V
        parent[v] = null
        dist[S] = 0
        PQ.push((0, S))

        while not PQ.empty():
            (d, u) = PQ.pop()
            if d > dist[u]: continue

            for each residual edge e = (u, v) from u with residual capacity r(e) > 0:
                c_pi = e.cost + pi[u] - pi[v]
                if dist[u] + c_pi < dist[v]:
                    dist[v] = dist[u] + c_pi
                    parent[v] = e
                    PQ.push((dist[v], v))

        // If sink is unreachable in residual graph, max flow reached
        if dist[T] == infinity:
            break

        // Update Johnson potentials
        for v in V:
            if dist[v] < infinity:
                pi[v] = pi[v] + dist[v]

        // Find bottleneck residual capacity along augmenting path
        delta = targetFlow - totalFlow
        curr = T
        while curr != S:
            e = parent[curr]
            delta = min(delta, e.capacity - e.flow)
            curr = e.from

        // Augment flow along path and reverse edges
        curr = T
        while curr != S:
            e = parent[curr]
            e.flow += delta
            reverse(e).flow -= delta
            totalCost += delta * e.cost
            curr = e.from

        totalFlow += delta

    return (totalFlow, totalCost)
```

**Complexity:**
- **Time Complexity:** $O(F \cdot E \log V)$ where $F$ is the number of augmentation steps.
- **Space Complexity:** $O(V + E)$ for adjacency lists, potentials, distance arrays, and parent pointers.

---

## 3. Baseline Routing Strategies

### A. Round Robin Routing
- **Mechanism:** Rotates client regional requests cyclically across available, active edge servers.
- **Primary Pass:** Divides regional demand into equal portions $D_{\text{share}} = D(r) / N_{\text{active}}$. Allocates up to $\min(D_{\text{share}}, C_{\text{server\_rem}}, C_{\text{link\_rem}})$.
- **Secondary Pass:** Greedily fills remaining server/link capacities in round-robin order.
- **Fallback:** Spills excess unroutable traffic to Origin fallback.
- **Strengths:** Simple, balanced server utilization across edge nodes.
- **Weaknesses:** Completely ignores physical distance/latency and transit bandwidth cost, resulting in $2.5\times - 3\times$ higher average latency and cost.
- **Time Complexity:** $O(R \cdot S)$ where $R$ is region count and $S$ is server count.

### B. Nearest Server Routing (Min-Latency First)
- **Mechanism:** Sorts candidate links from each region by effective physical latency $L_{\text{eff}} = L_{\text{base}} + L_{\text{congestion}}$ in ascending order.
- **Allocation:** Greedily allocates maximum admissible flow to the closest active server until server capacity or link bandwidth is saturated, then moves to the next closest server.
- **Fallback:** Spills over to Origin fallback when all connected edge servers are saturated.
- **Strengths:** Minimizes immediate physical latency for user requests.
- **Weaknesses:** Myopic; ignores transit bandwidth cost ($B$) and server load bands. Heavily over-saturates nearest nodes while leaving distant nodes idle.
- **Time Complexity:** $O(R \cdot S \log S)$ due to sorting candidate links per region.

### C. Greedy Cost-Based Routing
- **Mechanism:** Evaluates composite link routing cost $C = \alpha L + \beta B + (1-H)M$ for all candidate links.
- **Allocation:** Sorts available routes from lowest cost to highest cost. Greedily pushes flow into the cheapest available connection until capacity limits are reached.
- **Strengths:** Fast, achieves lower cost than Round Robin on uncongested links.
- **Weaknesses:** Cannot anticipate server load escalation bands ($0 \to 6 \to 15 \to 60$). Suffers from early-region resource starvation because it lacks reverse-edge rerouting capabilities.
- **Time Complexity:** $O(R \cdot S \log S)$.

---

## 4. Supporting DAA Algorithms

### A. Edmonds-Karp Maximum Flow (BFS)
- **Purpose:** Independent, ground-truth algorithmic verification of maximum feasible network throughput.
- **Formulation:** Computes shortest augmenting path in terms of edge count using Breadth-First Search (BFS) in the residual graph $G_f$.
- **Termination:** Guaranteed in at most $O(V \cdot E)$ augmentations because shortest-path distances monotonically increase.
- **Time Complexity:** $O(V \cdot E^2)$. Space Complexity: $O(V + E)$.

### B. Minimum Cut & Bottleneck Analysis
- **Max-Flow Min-Cut Theorem:** The maximum value of an $(S, T)$-flow equals the minimum capacity of an $(S, T)$-cut.
- **Identification:**
  1. Compute maximum flow using Edmonds-Karp.
  2. Perform BFS/DFS from $S$ in residual graph $G_f$ using only edges with $c_f(u, v) > 0$.
  3. Reachable nodes form $S_{\text{cut}}$; unreachable nodes form $T_{\text{cut}}$.
  4. Cut edges are forward edges with $u \in S_{\text{cut}}, v \in T_{\text{cut}}$ where $f(u, v) = u(u, v)$ (fully saturated).
- **Edge-Fleet Separation:** Saturated links are categorized into edge transit bottlenecks and Origin fallback lines.

### C. 0/1 Knapsack Dynamic Programming (Cache Optimization)
- **Purpose:** Selects web assets for local edge cache storage $W$ (MB) to maximize hit popularity value.
- **Recurrence Relation:**
  $$dp[i][w] = \begin{cases} dp[i-1][w] & \text{if } \text{size}_i > w \\ \max(dp[i-1][w], dp[i-1][w - \text{size}_i] + \text{val}_i) & \text{if } \text{size}_i \le w \end{cases}$$
- **Backtracking:** Optimal asset combination reconstructed in $O(N)$ by tracing table decisions from $dp[N][W]$.
- **Time Complexity:** $O(N \cdot W)$. Space Complexity: $O(N \cdot W)$.

### D. Holt Linear Exponential Smoothing (Demand Forecasting)
- **Purpose:** Predicts future regional traffic demand from historical time series by separating base level $L_t$ and linear trend slope $T_t$.
- **Formulas:**
  $$\begin{aligned}
  L_t &= \alpha y_t + (1 - \alpha)(L_{t-1} + T_{t-1}) \\
  T_t &= \beta (L_t - L_{t-1}) + (1 - \beta) T_{t-1} \\
  \hat{y}_{t+m} &= L_t + m \cdot T_t
  \end{aligned}$$
- **Time Complexity:** $O(T + m)$. Space Complexity: $O(T + m)$.

---

## 5. Algorithmic Complexity Comparison

| Algorithm | Category | Time Complexity | Space Complexity | Optimality Guarantee |
| :--- | :--- | :--- | :--- | :--- |
| **Min-Cost Max-Flow** | Global Optimization | $O(F \cdot E \log V)$ | $O(V + E)$ | **Globally Optimal (Cost & Flow)** |
| **Round Robin** | Heuristic Baseline | $O(R \cdot S)$ | $O(R + S)$ | Sub-optimal (Uniform Load Heuristic) |
| **Nearest Server** | Heuristic Baseline | $O(R \cdot S \log S)$ | $O(R \cdot S)$ | Myopic (Latency-Only Heuristic) |
| **Greedy Cost** | Heuristic Baseline | $O(R \cdot S \log S)$ | $O(R \cdot S)$ | Myopic (Static Cost Heuristic) |
| **Edmonds-Karp** | Independent Verification | $O(V \cdot E^2)$ | $O(V + E)$ | **Globally Optimal (Max Flow)** |
| **Minimum Cut** | Bottleneck Detection | $O(V + E)$ | $O(V)$ | **Exact Minimum Cut** |
| **0/1 Knapsack DP** | Cache Placement | $O(N \cdot W)$ | $O(N \cdot W)$ | **Globally Optimal (Integer DP)** |
| **Holt Forecasting**| Trend Prediction | $O(T + m)$ | $O(T + m)$ | Analytical Smoothing |

