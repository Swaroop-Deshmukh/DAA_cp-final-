# Viva Voce & Technical Defense Questions

**Adaptive CDN Traffic Router**  
*College-level project defense guide containing 30 in-depth questions and model answers covering graph algorithms, network optimization, heuristics, dynamic adaptation, and CDN systems engineering.*

---

## Category A: Graph Theory & Network Flow Fundamentals

### Q1: What is a flow network, and what two fundamental properties must every valid flow satisfy?
**Answer:**  
A flow network is a directed graph $G = (V, E)$ with a designated source vertex $S$ and sink vertex $T$, where each directed edge $(u, v) \in E$ has a non-negative capacity $c(u, v) \ge 0$.  
Every feasible flow $f: E \to \mathbb{R}$ must satisfy:
1. **Capacity Constraint:** For every edge, the assigned flow cannot be negative and cannot exceed available capacity: $0 \le f(u, v) \le c(u, v)$.
2. **Flow Conservation:** For every vertex other than $S$ and $T$, total incoming flow must strictly equal total outgoing flow:
   $$\sum_{u : (u, v) \in E} f(u, v) = \sum_{w : (v, w) \in E} f(v, w), \quad \forall v \in V \setminus \{S, T\}$$

---

### Q2: Why are residual reverse edges necessary in flow augmentation algorithms?
**Answer:**  
When flow $f(u, v) > 0$ is pushed along an edge, a reverse edge $(v, u)$ is created in the residual graph $G_f$ with residual capacity $r(v, u) = f(u, v)$ and cost $c(v, u) = -c(u, v)$.  
Reverse edges represent the ability to **"undo" or redirect** a previously made routing decision. Without reverse edges, a greedy path choice would permanently lock in traffic, preventing the algorithm from discovering a globally cheaper combination of paths (as demonstrated by the canonical diamond network).

---

### Q3: State the Max-Flow Min-Cut Theorem and explain how our project verifies it.
**Answer:**  
The Max-Flow Min-Cut Theorem states that in any flow network, the maximum amount of flow that can travel from source $S$ to sink $T$ is exactly equal to the minimum total capacity of edges that, if removed, disconnect $S$ from $T$:
$$\max |f| = \min_{(S_{\text{cut}}, T_{\text{cut}})} \text{cap}(S_{\text{cut}}, T_{\text{cut}})$$
In our project (Phase 7), after running the Edmonds-Karp maximum flow algorithm, we perform a BFS on the residual graph $G_f$ from $S$ considering only edges with residual capacity $r(e) > 0$. The reachable nodes form $S_{\text{cut}}$, and all edges leading from $S_{\text{cut}}$ to $T_{\text{cut}}$ are identified. The sum of capacities of these saturated cut edges was verified to equal the maximum flow bit-for-bit ($6000\text{ req/s} == 6000\text{ req/s}$ in test 7.2).

---

### Q4: What is an augmenting path, and how does Edmonds-Karp guarantee termination?
**Answer:**  
An augmenting path is a simple directed path from source $S$ to sink $T$ in the residual graph $G_f$ where every edge has positive residual capacity ($r(e) > 0$).  
The Ford-Fulkerson method does not specify path selection and can loop indefinitely or run in pseudo-polynomial time with irrational capacities. The Edmonds-Karp algorithm specifically selects the **shortest augmenting path in terms of number of edges** using Breadth-First Search (BFS). It is proven that the shortest distance from $S$ to any node in $G_f$ increases monotonically, guaranteeing termination in at most $O(V \cdot E)$ augmentations, yielding an overall time complexity of $O(V \cdot E^2)$.

---

## Category B: Min-Cost Max-Flow & Johnson Dual Potentials

### Q5: Why can we NOT run standard Dijkstra's algorithm directly on the residual graph during Min-Cost Max-Flow?
**Answer:**  
In Min-Cost Max-Flow, residual reverse edges have costs $c(v, u) = -c(u, v)$. Because forward edge costs are positive, reverse residual edges inherently carry **negative edge weights**.  
Standard Dijkstra's algorithm greedily assumes that once a vertex is popped from the priority queue with minimal distance, its shortest distance is finalized. With negative edges, this greedy property is violated because a path visiting a negative edge later could produce a smaller total distance, causing Dijkstra to return incorrect results or loop indefinitely.

---

### Q6: How do Johnson's dual potentials resolve the negative edge problem?
**Answer:**  
Johnson's algorithm assigns a real-valued potential $\pi(v)$ to each vertex $v \in V$. The reduced cost $c^\pi(u, v)$ for edge $(u, v)$ is defined as:
$$c^\pi(u, v) = c(u, v) + \pi(u) - \pi(v)$$
For any path $P = \langle v_0, v_1, \dots, v_k \rangle$, the sum of reduced costs telescopes:
$$\sum_{i=0}^{k-1} c^\pi(v_i, v_{i+1}) = \left(\sum_{i=0}^{k-1} c(v_i, v_{i+1})\right) + \pi(v_0) - \pi(v_k)$$
Since $\pi(v_0) - \pi(v_k)$ is independent of the path taken between $v_0$ and $v_k$, the path that minimizes reduced cost also minimizes original cost. By maintaining potentials such that $c^\pi(u, v) \ge 0$ for all residual edges, Dijkstra's priority queue can be safely used in $O(E \log V)$.

---

### Q7: How are vertex potentials updated after each Dijkstra augmentation?
**Answer:**  
After running Dijkstra using reduced costs $c^\pi(u, v)$ to find shortest distances $d(v)$ from $S$ to all nodes, potentials are updated as:
$$\pi'(v) = \pi(v) + d(v)$$
By the triangle inequality of shortest paths, for every edge $(u, v) \in E_f$:
$$d(v) \le d(u) + c^\pi(u, v) = d(u) + c(u, v) + \pi(u) - \pi(v)$$
Rearranging yields:
$$c(u, v) + (\pi(u) + d(u)) - (\pi(v) + d(v)) \ge 0 \implies c^{\pi'}(u, v) \ge 0$$
Thus, reduced costs remain strictly non-negative for the next iteration.

---

### Q8: Why does the project scale floating-point continuous costs to integers before invoking MCMF?
**Answer:**  
Standard network flow and shortest-path algorithms with potentials require exact arithmetic. Floating-point roundoff errors (e.g., $1e-16$) can cause reduced costs $c^\pi(u, v)$ to evaluate to tiny negative numbers like $-10^{-15}$, breaking Dijkstra's non-negativity invariant.  
By multiplying all continuous costs by an integer scaling factor ($100\times$) and casting to `long long`, arithmetic is exact, priority queue comparisons are deterministic, and precision down to two decimal places (cents) is preserved.

---

## Category C: Routing Model & Cost Function

### Q9: Explain the multi-factor routing cost formula used in Phase 3.
**Answer:**  
The composite routing cost is given by:
$$C = \alpha \cdot L + \beta \cdot B + (1 - H) \cdot M$$
Where:
- $L$ is effective physical latency (base fiber propagation + congestion delays in ms).
- $B$ is bandwidth transit cost per unit ($/Gbps).
- $H \in [0.0, 1.0]$ is the cache hit ratio.
- $M$ is the cache miss penalty incurred when requesting content from the origin ($).
- $\alpha$ and $\beta$ are configurable weighting coefficients balancing latency versus transit cost.

---

### Q10: How does the network flow model prevent a factor like server congestion from being counted twice?
**Answer:**  
In our design, network transit latency and link bandwidth costs are attributed strictly to **Region-to-Server edges**.  
Server utilization, on the other hand, is represented exclusively on **Server-to-Sink edges** using four parallel convex load bands ($0, 6, 15, 60$). By isolating link-level transit attributes to Layer 2 edges and server-level compute saturation to Layer 3 edges, no parameter is double-counted.

---

### Q11: How do parallel convex load bands model server queuing delays?
**Answer:**  
Server queueing delays follow an $M/M/1$ or $M/M/c$ queueing curve, where delay escalates non-linearly as utilization approaches $100\%$.  
We model this piecewise-linearly using four parallel edges between Server and Sink, each with capacity equal to $25\%$ of server capacity:
- Band 1 ($0\% - 25\%$ utilization): Cost = $0$
- Band 2 ($25\% - 50\%$ utilization): Cost = $6$
- Band 3 ($50\% - 75\%$ utilization): Cost = $15$
- Band 4 ($75\% - 100\%$ utilization): Cost = $60$  
Because MCMF always exhausts lower-cost edges before higher-cost edges, it naturally prefers servers operating below $75\%$ capacity, preventing severe server overload.

---

### Q12: How is the central Origin Server modeled, and why does it have a high cost?
**Answer:**  
The Origin Server is modeled as a fallback node with edges from all client regions. Each region-to-origin edge has high latency ($100\text{ ms}$), transit cost ($5.0$), $0\%$ cache hit ratio, and an origin compute penalty ($50.0$), yielding a total unit cost of $\$177.50/\text{req}$.  
Because its capacity equals total network demand, the flow network is mathematically guaranteed to always possess a feasible flow, preventing unroutable demand drops during total edge fleet failures.

---

## Category D: Baseline Strategies & Benchmark Comparisons

### Q13: Contrast Min-Cost Max-Flow with the Nearest Server (Min-Latency) routing heuristic.
**Answer:**  
- **Nearest Server:** Greedily assigns each region's demand to the server with the lowest physical latency. While it achieves the absolute lowest physical latency ($6.12\text{ ms}$ vs $6.15\text{ ms}$), it completely ignores server load bands and transit costs. In Scenario 2 (Mumbai spike), it overloads Pune Server to $90\%$ utilization (Band 4 penalty), resulting in a $\$350,175$ cost.
- **Min-Cost Max-Flow:** Global optimization balances latency against load bands and transit costs. In Scenario 2, MCMF balances Pune, Delhi, and Mumbai at $\sim 75\%$ utilization, saving $9.4\%$ in total cost with only a fraction of a millisecond difference in latency.

---

### Q14: Why does Round Robin perform poorly in geographic CDN environments?
**Answer:**  
Round Robin divides traffic uniformly without regard to geographic proximity or fiber distance. In our benchmarks, Round Robin resulted in an average latency of $20.69\text{ ms}$ ($3.4\times$ higher than Nearest Server) and an average cost of $\$32.78/\text{req}$ ($2.1\times$ higher than MCMF) because it frequently routes Pune traffic to Delhi or Bangalore when a local Pune Server is idle.

---

### Q15: What is "myopic decision-making" in Greedy Cost routing, and how does MCMF overcome it?
**Answer:**  
Greedy Cost routes the first region's traffic to the currently cheapest route without considering that a subsequent region might need that exact capacity even more urgently. Once capacity is assigned, a greedy algorithm cannot revoke its decision.  
MCMF overcomes this through **residual reverse edges**: if a later region's flow can achieve a massive cost reduction by using a server currently occupied by an earlier region, the earlier region's traffic is pushed along reverse edges to an alternative path, achieving the true global optimum.

---

### Q16: In Scenario 5 (Pune transit link congestion), why did Nearest Server and Greedy Cost abandon Pune Server while MCMF continued to use it?
**Answer:**  
When the Pune $\to$ Pune Server link was delayed by $+35\text{ ms}$, both Nearest Server and Greedy Cost observed that Pune $\to$ Mumbai Server was now cheaper for Pune users, routing all Pune traffic to Mumbai Server. This left Pune Server with $0\%$ utilization and pushed Mumbai Server to $100\%$ capacity.  
MCMF rerouted Pune users to Mumbai Server, but simultaneously backfilled Pune Server with overflow from Mumbai and Hyderabad users whose links to Pune Server were uncongested. MCMF kept both servers balanced at $50\%$ and $75\%$ load, saving $29.3\%$ in routing costs.

---

## Category E: Dynamic Adaptation & Failure Handling

### Q17: What triggers replanning in the Router coordinator, and why do we use thresholds?
**Answer:**  
The replanning policy (`ReplanningPolicy`) monitors network state changes and triggers MCMF re-computation when:
1. Any server changes health state (Online $\to$ Offline or Offline $\to$ Online).
2. Regional demand shifts by more than $\Delta D \ge 10\%$.
3. Link latency shifts by more than $\Delta L \ge 5.0\text{ ms}$.  
Thresholds prevent **route flapping** (oscillation), where micro-variations in traffic or noise cause continuous expensive graph re-computations and unstable routing assignments.

---

### Q18: What happens when an edge server fails during active routing?
**Answer:**  
1. The server's `isActive` flag is set to `false`, and its `availableCapacity` is set to `0` for routing.
2. The `FailureHandler` notifies the `Router`.
3. The flow graph sets the capacity of all edges leading from the failed server to the Sink to `0`.
4. MCMF replans traffic, instantly redistributing the failed node's traffic across surviving edge servers and Origin fallback with $0\text{ req/s}$ assigned to the failed server.

---

## Category F: Supporting DAA Algorithms

### Q19: Explain the Dynamic Programming formulation for 0/1 Knapsack Cache Optimization.
**Answer:**  
Given $N$ content assets with size $s_i$ and popularity value $v_i$, and edge cache budget $W$:
$$dp[i][w] = \begin{cases} dp[i-1][w] & \text{if } s_i > w \\ \max(dp[i-1][w], dp[i-1][w - s_i] + v_i) & \text{if } s_i \le w \end{cases}$$
The table is populated in row-major order in $O(N \cdot W)$ time. The optimal asset set is reconstructed in $O(N)$ by checking whether $dp[i][w] \neq dp[i-1][w]$. In Phase 7 test 7.7, the DP solution matched brute-force search bit-for-bit.

---

### Q20: How does Holt Linear Exponential Smoothing predict future CDN traffic demand?
**Answer:**  
Holt's two-parameter linear exponential smoothing decomposes non-stationary demand into base level $L_t$ and trend slope $T_t$:
$$\begin{aligned}
L_t &= \alpha y_t + (1 - \alpha)(L_{t-1} + T_{t-1}) \\
T_t &= \beta (L_t - L_{t-1}) + (1 - \beta) T_{t-1}
\end{aligned}$$
Future demand $m$ steps ahead is forecasted as:
$$\hat{y}_{t+m} = L_t + m \cdot T_t$$
Where $\alpha \in [0, 1]$ controls level adaptability and $\beta \in [0, 1]$ controls trend responsiveness.

---

## Category G: Real-World CDN System Engineering

### Q21: How does this project's routing model compare to real CDNs like Cloudflare and Akamai?
**Answer:**  
- **Cloudflare** primarily uses **BGP Anycast**, where all edge data centers announce the same IP prefix, and Internet routers route packets to the topologically closest data center. While fast, Anycast cannot consider server capacity or cache state, leading to hot spots during local surges.
- **Akamai** uses **Centralized DNS / SDN Mapping**, similar to our architecture. A centralized optimization cluster continuously collects server telemetry, solves global optimization problems, and returns tailored DNS IP resolutions that balance latency against transit costs and server loads.

---

### Q22: What are the primary bottlenecks of this architecture at massive scale (e.g., 10,000 servers)?
**Answer:**  
While MCMF runs in $30 - 45\ \mu\text{s}$ for our 5-region, 4-server network, its complexity scales with $O(F \cdot E \log V)$. For 10,000 servers:
1. Solving a global graph synchronously on every packet would be too slow.
2. In production, networks are partitioned hierarchically into autonomous regional clusters, using coarse routing between continents and fine-grained MCMF within metro regions.

