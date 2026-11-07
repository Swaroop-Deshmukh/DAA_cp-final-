# Feature Documentation: Routing Decision Inspector & Demonstration Mode

## 1. Overview & Pedagogical Purpose

The **Routing Decision Inspector** is a demonstration and educational mode designed for instructors, evaluators, and system architects. It enables deep inspection into the inner workings of the Adaptive CDN Traffic Router.

While continuous simulation streams live traffic at 1x to 5x speeds, an evaluator can:
1. **Pause Simulation Clock:** Instantly freeze the network state without losing recent traffic history.
2. **Inspect Request & Network Snapshots:** Inspect incoming regional traffic demands, active edge server health, remaining capacities, link latencies, and active congestion states.
3. **Step Through Algorithm Calculations (C++ Execution Trace):** Walk forward and backward through genuine, discrete mathematical decisions made by the C++ engine (e.g. Dijkstra augmenting path searches, reduced edge costs, Johnson potential updates, bottleneck capacity saturations, or cyclic rotation assignments).
4. **Inspect Final Allocation Decisions:** Examine exact per-region flow allocations, unit costs, and economic justifications.
5. **Replay & Compare:** Re-run the exact frozen snapshot with an alternate algorithm (`Min-Cost Max-Flow`, `Round Robin`, `Nearest Server`, or `Greedy Cost`) to observe how different algorithmic paradigms make fundamentally different routing tradeoffs on the identical network snapshot.
6. **Time-Travel Across History:** Select any recent recorded simulation tick from the bounded history ring buffer to inspect how the network responded to transient spikes or server outages.

---

## 2. Architecture & Control Flow

```
                                [ Evaluator Action: PAUSE & INSPECT ]
                                                  │
                                                  ▼
                        +---------------------------------------------------+
                        |         Simulation Manager (Python)               |
                        |  - Freezes current tick & regional demands        |
                        |  - Pulls snapshot from history ring buffer        |
                        +-------------------------+-------------------------+
                                                  │
                                                  │ POST /api/inspector/snapshot/{tick}
                                                  │ (record_trace=true)
                                                  v
                        +---------------------------------------------------+
                        |             C++17 Engine Core                     |
                        |  --api inspect-trace <strategy> <demands...>      |
                        |                                                   |
                        |  For MCMF:                                        |
                        |   - Step 1: GRAPH_INIT (Johnson potentials = 0)   |
                        |   - Step 2..k: FLOW_AUGMENT (Augmenting Paths)    |
                        |   - Step 3..k: POTENTIAL_UPDATE (pi'(v) update)   |
                        |   - Final: TERMINATION (Optimal Max-Flow)         |
                        |                                                   |
                        |  For Heuristics (RR, Nearest, Greedy):            |
                        |   - CANDIDATE_EVALUATION (Ranked links/latencies) |
                        |   - ASSIGNMENT (Flow dispatched to POP)           |
                        |   - CAPACITY_EXHAUSTED (Server saturated)         |
                        |   - FALLBACK (Origin DC spillover activated)      |
                        +-------------------------+-------------------------+
                                                  │
                                                  │ Structured JSON with "trace"
                                                  v
                        +---------------------------------------------------+
                        |        Routing Decision Inspector UI              |
                        |  - Stepper Toolbar: Prev / Next / Play / Jump     |
                        |  - Trace Timeline & Variable Key-Value Inspector  |
                        |  - Allocation Table & Economic Proof Narrative    |
                        +---------------------------------------------------+
```

---

## 3. Algorithm-Specific Decision Traces

### A. Min-Cost Max-Flow (MCMF) Trace Format
For each iteration $k$ of Successive Shortest Path (SSP):
1. **Dijkstra Reduced-Cost Shortest Path:**
   - Reduced cost invariant: $c^\pi(u, v) = c(u, v) + \pi(u) - \pi(v) \ge 0$.
   - Augmenting path sequence: e.g. `Source (S) -> Hyderabad -> Bangalore Server -> Sink (T)`.
   - Path reduced distance: $\text{dist}[\text{Sink}]$.
2. **Bottleneck Flow Identification:**
   - Evaluates residual capacity $r(u, v) = c(u, v) - f(u, v)$ along all path edges.
   - Computes $\Delta f = \min_{(u, v) \in P} r(u, v)$.
3. **Residual Capacity Updates:**
   - Forward edge: $r(u, v) \leftarrow r(u, v) - \Delta f$.
   - Reverse edge: $r(v, u) \leftarrow r(v, u) + \Delta f$.
   - Cumulative delivered flow: $F \leftarrow F + \Delta f$.
4. **Johnson Dual Potential Update:**
   - $\pi'(v) = \pi(v) + \text{dist}[v]$ for all reachable nodes, maintaining non-negative reduced costs for the next Dijkstra priority-queue iteration.

#### Example Trace Output (MCMF):
```json
{
  "stepNumber": 8,
  "phase": "FLOW_AUGMENT",
  "action": "Augmenting Path #4 Pushed (1250 req/s)",
  "description": "Dijkstra identified cheapest augmenting path with reduced cost 0. Bottleneck flow of 1250 req/s augmented across residual edges.",
  "details": {
    "augmenting_path": "Source (S) -> Bangalore -> Bangalore Server -> Sink (T)",
    "bottleneck_capacity": "1250 req/s",
    "cumulative_routed_flow": "6250 req/s",
    "path_reduced_distance": "0",
    "residual_capacity_updates": "[Source -> Bangalore: res 2500 -> 1250, redCost=0] [Bangalore -> Bangalore Server: res 4500 -> 3250, redCost=0] [Bangalore Server -> Sink [Band 1]: res 1250 -> 0, redCost=0]"
  }
}
```

---

### B. Round Robin Trace Format
1. **`ROTATION_INIT`:** Lists active healthy servers in cyclic order and their available capacities: e.g. `[Pune Server (5000), Mumbai Server (6000), Delhi Server (4500), Bangalore Server (5000)]`.
2. **`REGION_EVALUATION`:** Computes target equal share: $\text{Share} = D_{\text{reg}} / |S_{\text{active}}|$.
3. **`ASSIGNMENT`:** Dispatches flow to each server in rotation, logging residual server and link capacities.
4. **`CAPACITY_EXHAUSTED`:** Logs when a server or link hits $0\text{ req/s}$ remaining capacity and rotates to the next available server.
5. **`FALLBACK`:** Dispatches unserved overflow to Origin datacenter at high latency ($100\text{ ms}$).

---

### C. Nearest Server Trace Format
1. **`STRATEGY_INIT`:** Initializes greedy physical latency routing.
2. **`CANDIDATE_EVALUATION`:** Evaluates all candidate links $(R \to S)$ and sorts servers by effective latency:
   $$L_{\text{eff}} = L_{\text{base}} + \Delta L_{\text{cong}}$$
   Example ranking for Mumbai: `#1 Mumbai Server (5 ms), #2 Pune Server (12 ms), #3 Bangalore Server (22 ms), #4 Delhi Server (26 ms)`.
3. **`ASSIGNMENT`:** Dispatches maximum possible traffic to the #1 ranked server.
4. **`CAPACITY_EXHAUSTED`:** When #1 server capacity fills, logs explicit shift of remaining demand to the #2 ranked server.
5. **`FALLBACK`:** Dispatches remaining traffic to Origin when all edge servers are saturated.

---

### D. Greedy Cost Trace Format
1. **`STRATEGY_INIT`:** Sets parameters $\alpha = 1.0, \beta = 1.5, M = 20.0$.
2. **`ROUTE_EVALUATION`:** Calculates multi-factor continuous cost for every link:
   $$C = \alpha \cdot L + \beta \cdot B + (1 - H) \cdot M$$
   Sorts routes by unit cost: `#1 Mumbai Server ($7.50/r), #2 Pune Server ($17.80/r), ...`
3. **`ASSIGNMENT`:** Allocates traffic greedily to the cheapest route.
4. **`CAPACITY_EXHAUSTED`:** When cheapest route saturates, logs shift to next cheapest candidate route.

---

## 4. UI Layout & Controls

The Inspector interface is organized into five functional sections:
1. **Top Inspector Bar:**
   - **Tick Selector:** Dropdown listing recent recorded simulation cycles (`Tick #42`, `Tick #41`, ...).
   - **Algorithm Replay:** Select any algorithm and click **Replay & Trace** to re-compute calculations on the frozen snapshot.
   - **Stepper Toolbar:** `First Step`, `Prev Step (Left Arrow)`, `Step X of Y`, `Next Step (Right Arrow)`, `Final Step`, and `Play/Pause Auto-Step`.
   - **Step Progress Bar:** Displays percentage progress through the algorithm's execution steps.
2. **Left Sidebar:**
   - **Trace Steps Timeline:** Scrollable list of numbered steps. Clicking any step jumps immediately to that calculation.
   - **Ingress Demand Snapshot:** Regional incoming requests with proportion bars.
   - **Edge Server Health & Limits:** Real-time online/offline badges, capacity utilization percentages, and link congestion alerts.
3. **Active Step Detail Card:**
   - Step Number, Phase Badge (`[FLOW_AUGMENT]`, `[POTENTIAL_UPDATE]`, `[ASSIGNMENT]`, etc.), and Action Headline.
   - Narrative Description explaining mathematical rationale.
   - **Variable Details Grid:** Displays augmenting path, bottleneck capacity, residual capacity updates, and reduced edge costs.
4. **Final Allocation Decision Table:**
   - Client Region $\to$ Assigned POP $\to$ Throughput (req/s) $\to$ Unit Cost $\to$ Total Cost.
5. **KPI Outcome Cards & Decision Rationale Box:**
   - Fleet-wide KPIs (Total Routed, Edge Traffic, Origin Fallback, Unit Cost, Average Latency) and economic proof explanation.

---

## 5. Demonstration & Evaluation Instructions

### Step-by-Step Viva / Demo Walkthrough:

1. **Start Live Simulation:**
   - Navigate to the **Live Simulation** tab.
   - Click **Start Simulation** (1x speed, Normal Mode).
   - Watch traffic fluctuate across Pune, Mumbai, Delhi, Bangalore, and Hyderabad.

2. **Trigger Delhi Server Failure:**
   - Click **Delhi POP Outage** in the Stress Injections toolbar.
   - Observe the replanning banner flash amber: `TRIGGER_SERVER_FAILURE: Delhi Server transitioned to INACTIVE`.

3. **Pause & Open Inspector:**
   - Click **Pause Simulation**.
   - Click **Inspect Decision** (or select **Decision Inspector** from the sidebar).
   - The entire network snapshot is now frozen on the exact tick of the outage.

4. **Walk Through MCMF Execution:**
   - Notice the step counter: e.g. `Step 1 of 24`.
   - Click **Next Step** (or press the **Right Arrow** key) to step through augmenting iterations.
   - Observe Step 1 (`GRAPH_INIT`): Dual potentials initialized to 0.
   - Observe Step 8 (`FLOW_AUGMENT`): Dijkstra discovers the shortest augmenting path bypassing the failed Delhi server.
   - Observe the `residual_capacity_updates` showing forward capacity decreasing and reverse residual increasing.
   - Observe Step 9 (`POTENTIAL_UPDATE`): Dual potentials updated via $\pi'(v) = \pi(v) + \text{dist}[v]$ to maintain non-negative reduced costs.

5. **Compare with Round Robin Replay:**
   - In the **Algorithm Replay** dropdown, select **Round Robin**.
   - Click **Replay & Trace**.
   - Step through the Round Robin trace: observe `ROTATION_INIT` (3 active servers, Delhi excluded), followed by equal-share `ASSIGNMENT` steps across Pune, Mumbai, and Bangalore.
   - Compare the final metrics: Round Robin yields higher routing cost and higher latency compared to MCMF.

6. **Resume Simulation:**
   - Click **Back to Live** or **Resume Simulation** to continue continuous live streaming.

---

## 6. Interactive Graph Execution Visualizer

Embedded directly inside the Decision Inspector is a high-performance, interactive SVG graph visualizer that renders the authentic C++ flow network topology and demonstrates real-time algorithmic execution.

### Graph Architecture & Layered DAG Layout
The visualizer models the exact C++ residual graph structure across 4 distinct hierarchical layers in a 1000x560 coordinate space:

1. **Layer 0 (Virtual Super-Source S):** Node 0 (`Source (S)`), virtual ingress injection point.
2. **Layer 1 (Client Regions):** Nodes 1–5 (`Pune`, `Mumbai`, `Delhi`, `Bangalore`, `Hyderabad`), displaying real-time regional request demands and ingress throughput.
3. **Layer 2 (Edge Servers & Origin Fallback):** Nodes 6–9 (`Pune Server`, `Mumbai Server`, `Delhi Server`, `Bangalore Server`) and Node 10 (`Origin Server`), displaying active server capacity and incoming flow loads.
4. **Layer 3 (Virtual Super-Sink T):** Node 11 (`Sink (T)`), terminal absorption endpoint receiving traffic via 4 convex parallel load bands per server.

### Visual Elements & Interactive Capabilities
- **Directed Forward Edges:** Rendered as smooth Bézier curves with arrows indicating flow direction. Stroke width and color dynamically scale with throughput utilization.
- **Reverse Residual Edges ($c_f(v, u)$):** Displayed as distinct dashed purple arcs. Can be toggled on/off to demonstrate how backward residual capacity enables Dijkstra to cancel/redirect previously pushed flow to guarantee global cost minimality.
- **Augmenting Path Highlighting:** When an augmenting path is found, the exact sequence $S \to \text{Region} \to \text{Server} \to T$ illuminates with an electric emerald glow (`#00f5a0`).
- **Flow Particle Animation:** Animated flow particles traverse the directed edges in real-time, demonstrating bottleneck flow volume $\Delta f = \min_{e \in P} c_f(e)$ traveling across the network.
- **Node & Edge Flyout Inspection:** Clicking any node or edge displays an interactive inspection flyout detailing flow balance ($\sum f_{in} - \sum f_{out} = 0$), maximum capacity, residual capacity, unit cost $c(u,v)$, and Johnson dual reduced cost $c^\pi(u,v) = c(u,v) + \pi(u) - \pi(v)$.
- **Dijkstra Calculation HUD:** A floating heads-up display presents the exact path sequence, bottleneck flow $\Delta f$, and optimization action in sync with the current step.
- **Interactive Controls:**
  - **Zoom & Pan:** Smooth zoom (+ / − / mouse wheel) and pan drag.
  - **Fit to Screen & Reset:** Auto-centers and scales the graph to the canvas container.
  - **Edge Labels Toggle:** Show/hide numeric flow, capacity, and residual annotations.
  - **Residual Edges Toggle:** Show/hide reverse backward residual edges.
  - **Candidate Paths Toggle:** Show/hide evaluated candidate routes.
  - **Fullscreen Mode:** Expands the visualizer for high-resolution projection during presentations.

---

## 7. Region Candidate Server Investigator & Decision Synthesis

To give evaluators complete, transparent visibility into why a specific edge server was chosen for any region (e.g., Pune), the Inspector features an interactive **Candidate Server Investigator** and **Decision Synthesis Panel**.

### A. Candidate Evaluation Matrix
When an ingress region (Pune, Mumbai, Delhi, Bangalore, or Hyderabad) is selected via the region pills:
- The backend evaluates all **4 Edge POP candidates** (`Pune Server`, `Mumbai Server`, `Delhi Server`, `Bangalore Server`) and the **Origin Fallback** ($5 \times 5 = 25$ candidate pairings across the network).
- For each candidate route, the matrix displays:
  1. **Server Health & Physical Latency:** Online/offline status, base propagation delay, and active congestion penalty ($L_{\text{eff}} = L_{\text{base}} + \Delta L_{\text{cong}}$).
  2. **Link Bandwidth Capacity:** Allocated flow, link limit, and remaining bandwidth headroom.
  3. **Server Processing Limits:** Total configured capacity, current fleet load, and utilization percentage.
  4. **Edge Cache Hit Ratio ($H$):** Percentage of requests served directly from the local cache tier.
  5. **Multi-Factor Unit Cost Formulation:**
     $$C = \alpha \cdot L + \beta \cdot B + (1 - H) \cdot M$$
     with $\alpha = 1.0$, $\beta = 1.5$, $M = 20.0$.
  6. **Eligibility & Constraint Status:** `ELIGIBLE`, `EXCLUDED_OUTAGE` (server hardware/link failure), `SATURATED` (server capacity exhaustion), or `ORIGIN_FALLBACK` (emergency fallback tier).
  7. **Allocated Flow & Share:** Exact throughput (req/s) and percentage share assigned by the C++ engine.

### B. Deep-Dive Decision Synthesis
Directly beneath the matrix, a three-column synthesis bridges rigorous theoretical algorithm analysis and production CDN network engineering:

1. **CDN Operational & Infrastructure Rationale (Real System Flow):**
   - **Ingress Demand & Proximity Routing:** Inbound regional volume and selected primary POP.
   - **Proximity SLA:** Millisecond round-trip time (RTT) comparison against alternate cities, demonstrating minimization of TCP handshakes.
   - **Link Capacity & Headroom:** Bandwidth utilization percentage and buffer headroom to absorb traffic spikes without packet drops.
   - **Edge Caching & Origin Offload:** Cache hit percentage eliminating long-haul backhaul egress to Origin.
   - **Fleet Load & Compute Health:** Server utilization safety band, preventing tail-latency degradation.

2. **Mathematical Optimization Proof (Algorithmic Rigor):**
   - **Objective Function:** Linear programming formulation:
     $$\min \sum_{(u,v) \in E} c(u,v) \cdot f(u,v) \quad \text{s.t.} \quad 0 \le f(u,v) \le u(u,v), \; \sum f_{\text{in}} - \sum f_{\text{out}} = b(v)$$
   - **Cost Differential:** Exact unit cost comparisons ($c_{\text{opt}}$ vs $c_{\text{alt}}$) and penalty differentials ($\Delta c$).
   - **Johnson Dual Potentials:** Maintenance of non-negative reduced costs $c^\pi(u,v) = c(u,v) + \pi(u) - \pi(v) \ge 0$, enabling Dijkstra shortest-path augmentations in $O(E \log V)$.
   - **Bottleneck Flow Push:** Calculation of $\Delta f = \min_{e \in P} c_f(e)$ along the augmenting residual path.

3. **Alternative Route Differential Analysis:**
   - Card-by-card breakdown for every evaluated alternative node:
     - Exact latency delta ($\Delta L = +X\text{ ms}$).
     - Unit cost premium ($\Delta C = +\$Y/\text{req}$).
     - Precise operational or constraint reason for non-selection (e.g. outage exclusion, capacity exhaustion, or sub-optimal economic cost).

---

## 8. Technical Guarantees & Verification

- **Single Source of Truth:** All graph topology nodes, edges, candidate evaluations, trace calculations, path searches, and residual updates are computed inside the compiled C++17 engine.
- **Non-Interference Invariant:** Verification tests prove that running routing with trace recording produces **bit-for-bit identical flow allocations, costs, and latencies** as runs without trace recording.
- **Memory Safety:** History is bounded to a maximum of 100 snapshots using an in-memory ring buffer (`collections.deque(maxlen=100)`), preventing unbounded memory growth.
- **Automated Test Coverage:** Verified via 44 automated tests in `backend/tests/` (including candidate matrix, inspector, and graph visualizer tests) and 90/90 C++ technical audit tests passing (100% pass rate).

