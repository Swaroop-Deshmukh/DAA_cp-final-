# Adaptive CDN Traffic Router

A simulation-based Content Delivery Network (CDN) traffic routing and optimization engine designed for intelligent distribution of incoming user traffic across multiple edge servers.

The system models key networking and economic constraints including network latency, transit bandwidth cost, link bandwidth capacity, edge server capacity, server utilization curves, cache hit ratios, and server availability.

---

## 1. Project Overview

Modern CDNs like Cloudflare, Akamai, and Fastly need to balance two competing objectives:
1. **User Experience:** Route requests to nearby edge servers to minimize round-trip latency.
2. **Operational Cost & Stability:** Prevent server over-utilization and avoid expensive cross-region bandwidth links.

This project implements a multi-objective traffic routing optimization system powered by classical and advanced graph algorithms.

---

## 2. Technology Stack

- **Core Routing & Simulation Engine:** C++17 (high-performance graph algorithms and deterministic discrete simulation)
- **Build System:** CMake 3.15+ (tested with MSVC / GCC)
- **API & Communication Layer:** Python 3.13 + FastAPI (REST backend)
- **Data Serialization:** JSON (structured interchange between C++ engine and API)
- **Interactive UI Dashboard:** HTML5, CSS3, and modern Vanilla JavaScript

---

## 3. Project Architecture & Algorithms

### Algorithmic Foundations
1. **Routing Cost Function (Phase 3):**
   $$C = \alpha \cdot L + \beta \cdot B + (1 - H) \cdot M$$
   Where $L$ is latency, $B$ is bandwidth transit cost, $H$ is cache hit ratio, and $M$ is cache miss penalty. Discretized via integer scaling factor ($100\times$) for exact flow network arithmetic.

2. **Flow Network Construction (Phase 4):**
   Transformed into a directed residual flow graph with canonical layers:
   ```text
   [Source S]
       │ (capacity = demand, cost = 0)
       ▼
   [Client Regions]
       │ (capacity = link bandwidth, cost = calculated multi-factor cost)
       ▼
   [Edge Servers] ───────────────► [Origin Server] (High-cost fallback)
       │ (4 parallel load bands:           │
       │  costs 0, 6, 15, 60)              │ (capacity = total demand, cost = 50)
       ▼                                   ▼
   [Sink T] ◄──────────────────────────────┘
   ```

3. **Min-Cost Max-Flow using Johnson Potentials & Dijkstra (Phase 5):**
   - **Successive Shortest Path (SSP):** Iteratively finds the cheapest augmenting path from $S$ to $T$.
   - **Johnson's Potentials:** Dual node potentials $\pi(u)$ transform reduced edge costs $c^\pi(u, v) = c(u, v) + \pi(u) - \pi(v) \ge 0$, allowing Dijkstra's priority queue in $O(E \log V)$.

4. **Adaptive Routing & Failure Handling (Phase 6):**
   - **Replanning Policy:** Automated threshold-based triggers ($\Delta D \ge 10\%$, $\Delta L \ge 5.0\text{ ms}$, server status change).

5. **Supporting Algorithms (Phase 7):**
   - **Edmonds-Karp Algorithm:** Computes maximum flow via Breadth-First Search (BFS) in $O(V \cdot E^2)$, providing an independent ground-truth verification of the MCMF maximum flow value.
   - **Minimum Cut & Bottleneck Analysis:** Partitions the residual network into $S_{\text{cut}}$ and $T_{\text{cut}}$ to pinpoint saturated bottleneck transit links. Verifies the **Max-Flow Min-Cut Theorem** ($\text{Max Flow} = \text{Min Cut Capacity}$).
   - **Dynamic Programming 0/1 Knapsack Cache Optimizer:** Selects content assets to replicate at edge servers under finite cache storage (MB) to maximize hit popularity value.
   - **Holt's Linear Exponential Smoothing:** Decomposes historical traffic demand into time-varying base level $L_t$ and linear trend slope $T_t$ to forecast upcoming load spikes.

6. **Baseline Heuristic Routing Strategies (Phase 8):**
   - **Round Robin:** Uniform cyclic distribution across available healthy edge servers.
   - **Nearest Server:** Greedily assigns traffic to the lowest-latency edge server first.
   - **Greedy Cost-Based:** Selects cheapest candidate paths based on continuous composite cost function.
   - **Comparative Benchmark Suite:** Rigorously evaluates all 4 strategies across 8 real-world scenarios measuring cost, latency, server utilization, link utilization, overflow spill, and execution time.

---

## 4. Folder Structure

```text
DAA_cp(final)/
├── README.md
├── benchmark_results.json       (Exported Phase 8 benchmark matrix for UI)
├── docs/
│   ├── development-history.md
│   ├── phase-1-to-7-audit.md
│   ├── phase-8-comparison.md
│   ├── continuous-simulation.md
│   ├── routing-decision-inspector.md
│   ├── algorithms.md
│   ├── viva-questions.md
│   ├── architecture.md
│   └── api-documentation.md
├── cpp-engine/
│   ├── CMakeLists.txt
│   ├── main.cpp
│   ├── include/
│   │   ├── Network.h
│   │   ├── Simulator.h
│   │   ├── CostCalculator.h
│   │   ├── Graph.h
│   │   ├── Algorithms.h
│   │   ├── DecisionTrace.h      (Authentic step-by-step decision tracer)
│   │   ├── FailureHandler.h
│   │   ├── Router.h
│   │   ├── EdmondsKarp.h
│   │   ├── CacheOptimizer.h
│   │   ├── DemandForecaster.h
│   │   └── BaselineRouters.h
│   └── src/
│       ├── Network.cpp
│       ├── Simulator.cpp
│       ├── CostCalculator.cpp
│       ├── Graph.cpp
│       ├── Algorithms.cpp
│       ├── FailureHandler.cpp
│       ├── Router.cpp
│       ├── EdmondsKarp.cpp
│       ├── CacheOptimizer.cpp
│       ├── DemandForecaster.cpp
│       └── BaselineRouters.cpp
├── backend/                     (FastAPI Bridge & Continuous Engine)
│   ├── app.py
│   ├── requirements.txt
│   ├── routes/
│   │   ├── routing.py
│   │   ├── simulation.py
│   │   ├── scenarios.py
│   │   ├── benchmarks.py
│   │   └── live.py              (WebSocket, Continuous Loop & Inspector API)
│   ├── services/
│   │   ├── cpp_engine.py
│   │   ├── validation.py
│   │   └── simulation_manager.py (Autonomous Clock, Ring Buffer, Stepper)
│   ├── models/
│   │   ├── requests.py
│   │   └── responses.py
│   └── tests/
│       ├── test_api.py
│       ├── test_live.py         (Continuous simulation & WebSocket tests)
│       ├── test_inspector.py    (Snapshot freeze, stepper, and trace tests)
│       ├── test_graph_visualizer.py (Authentic graph topology & path tests)
│       └── test_candidates.py   (Candidate server comparison matrix tests)
└── frontend/                    (Enterprise NOC Operations Dashboard)
    ├── index.html
    ├── css/
    │   ├── style.css
    │   ├── components.css
    │   └── dashboard.css
    └── js/
        ├── app.js
        ├── api.js
        ├── charts.js
        ├── topology.js
        ├── routing.js
        ├── monitoring.js
        ├── comparison.js
        ├── simulation.js
        ├── live.js              (Continuous Simulation Controller & WebSocket)
        ├── inspector.js         (Routing Decision Inspector & Step-by-Step Stepper)
        ├── graph_visualizer.js  (Interactive SVG Residual Graph & Augmenting Animator)
        ├── forecast.js
        └── logs.js
```

---

## 5. Algorithmic Complexity

| Component | Algorithm | Time Complexity | Space Complexity |
| :--- | :--- | :--- | :--- |
| **Routing Optimizer** | MCMF (Dijkstra + Johnson) | $O(F \cdot E \log V)$ | $O(V + E)$ |
| **Round Robin Heuristic** | Cyclic Equal Share Heuristic | $O(R \cdot S)$ | $O(R + S)$ |
| **Nearest Server Heuristic** | Min-Latency Greedy Sorting | $O(R \cdot S \log S)$ | $O(R \cdot S)$ |
| **Greedy Cost Heuristic** | Multi-Factor Cost Greedy | $O(R \cdot S \log S)$ | $O(R \cdot S)$ |
| **Independent Max-Flow**| Edmonds-Karp (BFS) | $O(V \cdot E^2)$ | $O(V + E)$ |
| **Bottleneck Detector**| Minimum Cut (Residual BFS)| $O(V + E)$ | $O(V)$ |
| **Cache Placement** | 0/1 Knapsack Dynamic Prog.| $O(N \cdot W)$ | $O(N \cdot W)$ |
| **Demand Forecasting** | Holt's Linear Smoothing | $O(T + m)$ | $O(T + m)$ |

---

## 6. Installation and Execution Steps

### Prerequisites
- C++17 compliant compiler (MSVC 2019+, GCC 9+, or Clang 10+)
- CMake 3.15 or newer
- Python 3.10+ (for FastAPI Backend & Frontend serving)

### 1. One-Click Quick Launch (Windows)

Simply double-click `start.bat` (or `run.bat`) in the root directory, or run from PowerShell / Command Prompt:

```cmd
.\start.bat
```

This automated script will:
1. Verify Python 3.10+ installation.
2. Verify / compile the C++17 routing engine binary (`cdn_router.exe`).
3. Verify and install required backend packages.
4. Launch the FastAPI server.
5. Automatically open the **Interactive NOC Dashboard** in your default web browser (`http://127.0.0.1:8000/`).

---

### 2. Manual Build & Execution (C++ Engine Directly)

```powershell
cd cpp-engine
cmake -B build -S .
cmake --build build --config Release
.\build\Release\cdn_router.exe
```

### 2. Start Full Stack (FastAPI API + Interactive Dashboard)

```powershell
# Install backend dependencies
pip install -r backend/requirements.txt

# Run backend test suite (44 automated tests covering REST API, WebSockets, dynamic simulation, Decision Inspector, Graph Visualizer, and Candidate Evaluation Matrix)
python -m pytest backend/tests/ -v

# Launch the unified backend & NOC dashboard server
python -m uvicorn backend.app:app --host 127.0.0.1 --port 8000 --reload
```

Once running, access:
- **Interactive Operations Dashboard:** `http://127.0.0.1:8000/` or `http://127.0.0.1:8000/dashboard`
- **Swagger REST API Documentation:** `http://127.0.0.1:8000/docs`
- **Live Continuous WebSocket Stream:** `ws://127.0.0.1:8000/ws/live`

---

## 7. Interactive Dashboard Features (Phases 10, 11 & 12)

The web dashboard is an enterprise-grade dark SaaS Network Operations Center (NOC) built with semantic HTML5, CSS3, ES modules, and Chart.js:
1. **Routing Decision Inspector & Interactive Graph Visualizer:** Educational step-by-step trace mode for evaluators. Pause simulation, freeze network state, inspect regional requests and server limits, walk forward and backward through genuine C++ mathematical calculations (Dijkstra augmenting paths, reduced edge costs, Johnson dual potentials, residual capacity updates, cyclic server rotations). Includes a **live interactive SVG graph visualizer** with layered DAG layout, pan/zoom, fullscreen, toggleable reverse residual edges, candidate path states, animated flow particles along augmenting paths, node/edge click flyouts, and Dijkstra calculation HUD. Compare with alternate algorithm replays, and time-travel across recorded history ticks.
2. **Live Continuous Simulation:** Real-time autonomous simulation loop (1s / 500ms / 200ms ticks). Streams continuous telemetry over WebSockets, renders 30-tick rolling time-series charts (Regional Demands, Server Loads, Latencies), supports runtime algorithm switching, live event injections (server failure, recovery, network congestion, demand spike), and displays automated replanning triggers.
3. **Operations Overview:** Fleet KPI cards (Total Ingress Demand, Global Avg Latency, Edge Serving Efficiency, Healthy Edge POPs), destination doughnut chart, regional round-trip latency profile, and live audit feed.
4. **Network Topology Visualizer:** High-resolution interactive SVG diagram rendering Source Ingress $\to$ 5 Regions $\to$ 4 Edge POPs $\to$ Origin Central DC. Displays animated SVG flow lines, link statuses (Active, Congested, Fallback), and a click-to-inspect slide-out telemetry drawer.
5. **Traffic Routing Studio:** Interactive optimization console with strategy selection (MCMF, Round Robin, Nearest, Greedy), traffic demand inputs, solve duration timer, complete flow allocation breakdown table, and Johnson potential economic explanations.
6. **Server Telemetry Monitoring:** Live hardware cards for 4 edge POPs (Delhi, Mumbai, Bangalore, Kolkata) with 4-tier load band saturation gauges, online/offline status indicators, and capacity utilization progress bars.
7. **Algorithm Benchmark Matrix:** Rigorous comparative matrix of all 4 strategies evaluated across 8 operational scenarios, paired with comparative bar charts for routing cost and latency profiles.
8. **Simulation & Stress Lab:** 1-click test scenario triggers: Normal Baseline, Mumbai Surge (+100%), Delhi Hardware Outage, Delhi Recovery, Transit Congestion (+40ms), 24,000 req/s Overcapacity, Catastrophic All-Edge Down, and Cache Hit Improvement, with a one-click Reset button.
9. **Demand Forecasting & Cache Optimization:** Holt Linear Smoothing trend forecasting with region selector and slope metrics, plus 0/1 Knapsack Dynamic Programming cache placement with customizable storage budget (150 MB, 350 MB, 500 MB), visual memory allocation bar, and asset selection decision table.
10. **Audit Event Logs:** Comprehensive audit log with keyword search, severity filtering (ALL, INFO, WARN, ERROR, REROUTE), and live event broadcasting.

---

## 8. Implementation Roadmap

- [x] **Phase 1: Project Setup, Core Data Structures & CDN Network**
- [x] **Phase 2: Network Traffic Simulator & Dynamic Events**
- [x] **Phase 3: Routing Cost Calculator & Multi-Factor Evaluation**
- [x] **Phase 4: CDN Network Directed Flow Graph Construction**
- [x] **Phase 5: Core Optimization Engine — Min-Cost Max-Flow (Johnson Potentials + Dijkstra)**
- [x] **Phase 6: Adaptive Routing Coordinator & Failure Handling**
- [x] **Phase 7: Supporting DAA Algorithms (Edmonds-Karp, Min-Cut, DP Cache, Holt Smoothing)**
- [x] **Phase 8: Baseline Routing Strategies & Comparative Analysis**
- [x] **Phase 9: Python FastAPI Backend Bridge**
- [x] **Phase 10: Professional Adaptive CDN Management Dashboard**
- [x] **Phase 11: Continuous Real-Time Traffic Simulation & Autonomous Routing Loop**
- [x] **Phase 12: Routing Decision Inspector & Step-by-Step Demonstration Mode**


---

## 9. API Usage Examples

### Run Min-Cost Max-Flow Routing (cURL)
```bash
curl -X POST "http://127.0.0.1:8000/api/routing/run" \
     -H "Content-Type: application/json" \
     -d '{"strategy": "Min-Cost Max-Flow", "scenario": "normal"}'
```

### Trigger a Dynamic Outage Simulation (Python)
```python
import httpx

response = httpx.post(
    "http://127.0.0.1:8000/api/simulation/run",
    json={
        "event_type": "server_failure",
        "target": "Delhi Server",
        "value": 1.0,
        "strategy": "Min-Cost Max-Flow"
    }
)
print("Updated Fleet Load:", response.json()["serverLoads"])
```

### Retrieve 8-Scenario Comparative Benchmarks
```bash
curl -s "http://127.0.0.1:8000/api/benchmarks/mumbai_spike" | jq .
```

---

## 10. Step-by-Step Demo Walkthrough

1. **Launch the Stack:**
   ```powershell
   python -m uvicorn backend.app:app --host 127.0.0.1 --port 8000 --reload
   ```
2. **Open Browser:** Navigate to `http://127.0.0.1:8000/`.
3. **Inspect Fleet State:** Observe nominal 12,000 req/s demand, $0$ origin spill, and 100% healthy edge POPs on the **Overview** page.
4. **View Network Topology:** Switch to **Network Topology** tab to visualize animated cyan flow lines connecting 5 regions to 4 edge POPs. Click on *Mumbai Server* to view the 4-tier load band schedule in the slide-out inspector drawer.
5. **Run Routing Comparison:** Switch to **Traffic Routing**, select *Round Robin* vs *Min-Cost Max-Flow*, and observe how MCMF achieves lower cost and prevents overload.
6. **Inject Failure in Simulation Lab:** Click **Simulation Lab**, trigger *Delhi POP Outage*, and watch the system automatically reroute North traffic to Mumbai and Bangalore with zero dropped packets.
7. **Stress Test Overcapacity:** Trigger *24k Overcapacity Spike* to demonstrate graceful Origin DC fallback activation when total demand exceeds the 20,500 req/s edge fleet limit.
8. **Explore Cache Knapsack & Forecasting:** Check **Forecast & Cache** to inspect Holt linear smoothing curves and 0/1 Knapsack asset storage allocation.

---

## 11. Known Limitations & Architectural Scope

1. **Discrete Event Simulation vs. Kernel Sockets:** Network packets and transit flows are modeled as deterministic continuous-flow rates (req/s) rather than raw Linux kernel TCP/eBPF socket buffers.
2. **Static POP Topology:** The 5 user regions and 4 Tier-1 edge servers represent a fixed regional topology rather than dynamic BGP Anycast routing tables.
3. **Synchronous Subprocess Invocation:** The FastAPI bridge executes the C++ engine via secure process isolation with sub-millisecond execution times ($20 - 90\ \mu\text{s}$), suitable for high-performance orchestration and web telemetry.
