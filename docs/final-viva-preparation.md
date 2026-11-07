# ADAPTIVE CDN TRAFFIC ROUTER — COMPREHENSIVE VIVA PREPARATION GUIDE
## Design and Analysis of Algorithms (DAA) & Systems Defense Reference

This guide contains in-depth algorithmic derivations, mathematical proofs, time and space complexity analyses, and expected examiner questions for the final project defense.

---

## 1. Algorithmic Deep Dives

### A. Min-Cost Max-Flow (MCMF) Formulation

#### 1. Optimization Problem
Given a directed graph $G = (V, E)$ with source $s$, sink $t$, edge capacities $u(e) \ge 0$, and unit routing costs $c(e) \in \mathbb{R}$:
$$\text{Minimize } \sum_{e \in E} c(e) \cdot f(e)$$
Subject to:
1. **Capacity Constraint:** $0 \le f(u, v) \le u(u, v) \quad \forall (u, v) \in E$
2. **Skew Symmetry:** $f(u, v) = -f(v, u) \quad \forall (u, v) \in V \times V$
3. **Flow Conservation:** $\sum_{v \in V} f(u, v) = 0 \quad \forall u \in V \setminus \{s, t\}$

#### 2. Successive Shortest Path (SSP) Algorithm
- **Concept:** Iteratively finds an augmenting path from $s$ to $t$ with the minimum cumulative unit cost in the residual graph $G_f$.
- **Residual Graph $G_f$:**
  - For forward edge $e = (u, v)$ with flow $f(e) < u(e)$, residual capacity is $r(e) = u(e) - f(e)$ with cost $c(e)$.
  - For reverse edge $e_{\text{rev}} = (v, u)$ with flow $f(e) > 0$, residual capacity is $r(e_{\text{rev}}) = f(e)$ with negative cost $-c(e)$.
- **Why Ordinary Dijkstra Fails:** Reverse edges have negative costs $-c(e)$. Ordinary Dijkstra assumes non-negative edge weights; greedy settlement fails when negative edges exist.

---

### B. Johnson Vertex Potentials & Reduced Costs

#### 1. Reduced Cost Definition
Assign a potential $\pi(u) \in \mathbb{R}$ to each vertex $u \in V$. The **reduced cost** $c^\pi(u, v)$ of edge $(u, v)$ is:
$$c^\pi(u, v) = c(u, v) + \pi(u) - \pi(v)$$

#### 2. Preservation of Shortest Paths
Let $P$ be any path from vertex $s$ to vertex $t$ consisting of edges $(v_0, v_1), (v_1, v_2), \dots, (v_{k-1}, v_k)$ where $v_0 = s$ and $v_k = t$:
$$\sum_{i=0}^{k-1} c^\pi(v_i, v_{i+1}) = \sum_{i=0}^{k-1} [c(v_i, v_{i+1}) + \pi(v_i) - \pi(v_{i+1})]$$
Expanding the telescoping sum:
$$\sum_{e \in P} c^\pi(e) = \sum_{e \in P} c(e) + \pi(s) - \pi(t)$$
Since $\pi(s) - \pi(t)$ is a constant independent of the chosen path $P$, any path that minimizes reduced cost $c^\pi$ **also minimizes actual cost $c$**.

#### 3. Potential Update Invariant
After running Dijkstra using reduced costs $c^\pi$, let $d(u)$ be the shortest distance from $s$ to $u$. By the triangle inequality:
$$d(v) \le d(u) + c^\pi(u, v) = d(u) + c(u, v) + \pi(u) - \pi(v)$$
Rearranging terms:
$$c(u, v) + [\pi(u) + d(u)] - [\pi(v) + d(v)] \ge 0$$
Therefore, updating vertex potentials as:
$$\pi_{\text{new}}(u) = \pi_{\text{old}}(u) + d(u)$$
guarantees that for every edge $(u, v)$ in the updated residual network, **$c^{\pi_{\text{new}}}(u, v) \ge 0$**, allowing Dijkstra's priority queue to be used repeatedly.

---

### C. Convex Server Load Bands (Piecewise Linearization)

#### 1. Motivation
In queuing theory ($M/M/1$), server delay and queuing penalties grow non-linearly (convexly) as utilization approaches 100%:
$$\text{Delay} \propto \frac{1}{\mu - \lambda}$$
A single edge with fixed cost cannot model this behavior in standard network flow.

#### 2. Graph Formulation
Each edge server $S_i$ with capacity $C_i$ is connected to the sink $T$ via **4 parallel edges (bands)**:
- **Band 1 (0–50% Capacity):** Capacity $= 0.50 \cdot C_i$, Cost $= 0\text{ pts}$
- **Band 2 (50–75% Capacity):** Capacity $= 0.25 \cdot C_i$, Cost $= 6\text{ pts}$
- **Band 3 (75–90% Capacity):** Capacity $= 0.15 \cdot C_i$, Cost $= 15\text{ pts}$
- **Band 4 (90–100% Capacity):** Capacity $= 0.10 \cdot C_i$, Cost $= 60\text{ pts}$

Because costs strictly increase ($0 < 6 < 15 < 60$), the successive shortest path algorithm naturally saturates Band 1 first, then Band 2, Band 3, and finally Band 4, automatically balancing server utilization without non-linear solver overhead.

---

### D. Edmonds-Karp & Minimum Cut Verification

#### 1. Edmonds-Karp Algorithm
- Uses Breadth-First Search (BFS) to find the augmenting path with the **fewest edges** from $s$ to $t$.
- **Time Complexity:** $O(V \cdot E^2)$ because each augmenting path takes $O(E)$ and the total number of augmentations is at most $O(V \cdot E)$.
- **Purpose in Project:** Serves as an independent mathematical validation of the maximum flow computed by MCMF.

#### 2. Max-Flow Min-Cut Theorem
After maximum flow is achieved, run BFS in the residual graph $G_f$ from $s$:
- Set $S_{\text{cut}} = \{u \in V \mid u \text{ is reachable from } s \text{ in } G_f\}$.
- Set $T_{\text{cut}} = V \setminus S_{\text{cut}}$.
- The cut capacity is:
  $$C(S_{\text{cut}}, T_{\text{cut}}) = \sum_{u \in S_{\text{cut}}, v \in T_{\text{cut}}, (u, v) \in E} u(u, v) = \text{Max Flow}$$
- Saturated edges crossing from $S_{\text{cut}}$ to $T_{\text{cut}}$ identify the exact physical bottlenecks of the CDN fleet.

---

### E. Cache Placement using 0/1 Knapsack DP

#### 1. Problem Formulation
Given cache budget $W\text{ MB}$ and $N$ content items, each item $i$ has size $w_i\text{ MB}$ and popularity value $v_i$.
$$\text{Maximize } \sum_{i=1}^N v_i \cdot x_i \quad \text{subject to } \sum_{i=1}^N w_i \cdot x_i \le W, \quad x_i \in \{0, 1\}$$

#### 2. DP Recurrence Relation
Let $DP[i][w]$ be the maximum popularity value achievable using a subset of items $\{1, \dots, i\}$ with storage capacity $w$:
$$DP[i][w] = \begin{cases}
DP[i-1][w] & \text{if } w_i > w \\
\max(DP[i-1][w], DP[i-1][w - w_i] + v_i) & \text{if } w_i \le w
\end{cases}$$
- **Base Case:** $DP[0][w] = 0$ for all $0 \le w \le W$.
- **Complexity:** $O(N \cdot W)$ time and $O(N \cdot W)$ space (reducible to $O(W)$ 1D array).
- **Classification:** Pseudo-polynomial time algorithm (polynomial in the numeric value of $W$, exponential in input length $\log W$).

---

### F. Holt's Linear Exponential Smoothing

#### 1. Equations
Used for double exponential smoothing when traffic exhibits a local linear trend:
1. **Level Update ($L_t$):**
   $$L_t = \alpha Y_t + (1 - \alpha)(L_{t-1} + T_{t-1})$$
2. **Trend Update ($T_t$):**
   $$T_t = \beta(L_t - L_{t-1}) + (1 - \beta)T_{t-1}$$
3. **$m$-Step Ahead Forecast ($\hat{Y}_{t+m}$):**
   $$\hat{Y}_{t+m} = L_t + m \cdot T_t$$
- Parameters: Level smoothing factor $\alpha = 0.4$, trend smoothing factor $\beta = 0.2$.

---

## 2. Algorithm Complexity Summary Matrix

| Algorithm / Component | Module | Time Complexity | Space Complexity | Optimality Guarantee |
| :--- | :--- | :---: | :---: | :--- |
| **MCMF (Dijkstra + Johnson)** | `Algorithms.cpp` | $O(F \cdot E \log V)$ | $O(V + E)$ | **Global Minimum Cost** |
| **Edmonds-Karp** | `EdmondsKarp.cpp` | $O(V \cdot E^2)$ | $O(V + E)$ | **Global Maximum Flow** |
| **Min-Cut Bottleneck** | `EdmondsKarp.cpp` | $O(V + E)$ | $O(V)$ | **Exact Cut S-T Partition** |
| **0/1 Knapsack DP** | `CacheOptimizer.cpp`| $O(N \cdot W)$ | $O(N \cdot W)$ | **Optimal Cache Subset** |
| **Holt Forecasting** | `DemandForecaster.cpp`| $O(T + m)$ | $O(T + m)$ | **Optimal Linear Trend** |
| **Round Robin Heuristic** | `BaselineRouters.cpp`| $O(R \cdot S)$ | $O(R + S)$ | Heuristic Fair-Share |
| **Nearest Server Heuristic**| `BaselineRouters.cpp`| $O(R \cdot S \log S)$| $O(R \cdot S)$| Greedy Min-Latency |
| **Greedy Cost Heuristic** | `BaselineRouters.cpp`| $O(R \cdot S \log S)$| $O(R \cdot S)$| Greedy Local Cost |

---

## 3. High-Probability Examiner Viva Questions

### Q1: What makes Johnson's potentials superior to Bellman-Ford for successive shortest paths?
> **Answer:** *"Bellman-Ford takes $O(V \cdot E)$ time per augmenting path. In our CDN graph with $F$ units of flow, using Bellman-Ford repeatedly would require $O(F \cdot V \cdot E)$ time. By establishing vertex potentials once and updating them via Dijkstra's shortest distances in $O(E \log V)$, every subsequent augmentation runs in $O(E \log V)$. Over thousands of flow units, Johnson potentials reduce runtime by orders of magnitude (from milliseconds to microseconds)."*

### Q2: What is the economic rationale behind the Multi-Factor Cost Function?
> **Answer:** *"The cost formula $C = \alpha L + \beta B + (1-H)M$ balances latency (user experience), transit bandwidth fee (ISP transit cost), and cache miss penalty (origin compute load). If cache hit ratio $H$ is high, $(1-H)M \to 0$, reducing routing cost. Integer scaling by $100\times$ converts continuous floating-point dollars and milliseconds into exact integers, avoiding floating-point precision drift during Dijkstra augmentations."*

### Q3: How do you prevent negative cost cycles from breaking the graph?
> **Answer:** *"Reduced costs satisfy $c^\pi(u, v) \ge 0$ for all edges in the residual graph at every step. Because every edge cost is non-negative, the directed residual graph contains zero negative-cost cycles throughout execution, satisfying the necessary and sufficient condition for optimality."*

### Q4: What happens when the network demand exceeds total edge capacity?
> **Answer:** *"The graph incorporates directed fallback edges from user regions to the Origin Server with capacity equal to regional demand and cost 50 pts (higher than any edge band cost). When edge server capacities are fully exhausted, residual flow is forced across the origin fallback edges. Flow conservation holds, and unserved traffic remains 0 while alerting operators to origin spill."*

### Q5: How is process isolation maintained between FastAPI and C++?
> **Answer:** *"We execute `cdn_router.exe` using Python's `subprocess.run(shell=False)` with an argument array and a 10.0-second timeout. Input arguments are pre-validated against strict whitelists before execution, preventing shell injection. Communication occurs via standard output JSON streams with robust boundary detection."*

