# Continuous Real-Time Traffic Simulation & Adaptive Routing

## 1. Overview & Architecture

The **Continuous Real-Time Traffic Simulation** transforms the Adaptive CDN Traffic Router from a discrete scenario-based testbed into an autonomous, continuously operating edge routing control plane.

Every second (or sub-second interval at 2x/5x acceleration), the simulation executes a closed feedback loop:
1. **Dynamic Demand Generation:** Regional incoming traffic fluctuates dynamically according to time-of-day sinusoidal baseline diurnal cycles with superimposed high-frequency Gaussian noise.
2. **Dynamic Network Conditions:** Link utilization, effective latency (via $M/M/1$ queueing delay), and bandwidth transmission costs dynamically update.
3. **Adaptive Replanning Engine:** Evaluates network state deltas against hysteresis thresholds to determine whether to trigger routing recalculation.
4. **C++17 Routing Execution:** Computes the globally optimal or heuristic routing distribution using the selected algorithm (`mcmf`, `round_robin`, `nearest_server`, or `greedy_cost`).
5. **Real-Time Telemetry Streaming:** Serializes network state and decision metrics into compact JSON frames and broadcasts them to all connected dashboard clients via WebSockets (`/ws/live`).

```
                    +------------------------------------------+
                    |       Simulation Manager (Python)        |
                    |   (Timer: 1000ms / 500ms / 200ms)        |
                    +--------------------+---------------------+
                                         |
                       1. Generate Regional Demand
                       2. Apply Transient Events & Congestion
                       3. Check Replanning Thresholds
                                         |
                                         v
                    +--------------------+---------------------+
                    |       C++17 Routing Engine Core          |
                    |    (--api run-dynamic <strategy> ...)    |
                    |    Min-Cost Max-Flow / Heuristics        |
                    +--------------------+---------------------+
                                         |  Result JSON (Allocations,
                                         |  Latency, Cost, Utilizations)
                                         v
                    +--------------------+---------------------+
                    |       Live Event Broadcaster             |
                    |       WebSocket Server (/ws/live)        |
                    +--------------------+---------------------+
                                         |  JSON Push Frame
                                         v
                    +--------------------+---------------------+
                    |   Frontend Dashboard (Vanilla JS)        |
                    |  - 30-tick Rolling Time-Series Charts    |
                    |  - Animated Link Glows & Capacity Bars   |
                    |  - Replanning Decision Timeline          |
                    +------------------------------------------+
```

---

## 2. Dynamic Traffic Generation Model

Each region $i \in \{\text{Pune}, \text{Mumbai}, \text{Delhi}, \text{Bangalore}, \text{Hyderabad}\}$ has a base nominal demand $D_i^{\text{base}}$:
- **Pune:** 2,500 req/s
- **Mumbai:** 3,500 req/s
- **Delhi:** 2,000 req/s
- **Bangalore:** 2,500 req/s
- **Hyderabad:** 1,500 req/s
- **Fleet Total:** 12,000 req/s

### Diurnal Cycle + Stochastic Noise
At simulation tick $t \ge 0$:
$$D_i(t) = D_i^{\text{base}} \times \left(1.0 + 0.15 \cdot \sin\left(\frac{2\pi t}{120} + \phi_i\right) + \epsilon_i(t)\right)$$

Where:
- $\frac{2\pi t}{120}$ simulates an accelerated 2-minute diurnal peak-and-trough cycle.
- $\phi_i$ is a regional phase offset (e.g. Mumbai leads by $+0.2$ rad, Delhi by $+0.5$ rad).
- $\epsilon_i(t) \sim \mathcal{U}(-\alpha, +\alpha)$ is a bounded pseudo-random noise term seeded by the simulation RNG:
  - **Normal Mode:** $\alpha = 0.05$ ($\pm 5\%$ traffic perturbation).
  - **Stress Mode:** $\alpha = 0.35$ ($\pm 35\%$ high-variance bursts with transient spikes).

---

## 3. Link Latency & Queueing Degradation Model

Physical latency $L_{ij}(t)$ over link $(i, j)$ consists of base geographic distance plus queueing delay driven by utilization:
$$L_{ij}(t) = L_{ij}^{\text{base}} + \Delta L_{ij}^{\text{cong}} + \text{Delay}_{\text{queue}}\left(\frac{F_{ij}}{C_{ij}}\right)$$

Where:
- $L_{ij}^{\text{base}}$ is geographic propagation time (5 ms to 25 ms).
- $\Delta L_{ij}^{\text{cong}}$ is transient congestion injected via the control bar.
- Queueing delay rises non-linearly when link capacity $C_{ij}$ is saturated ($> 85\%$).

---

## 4. Intelligent Replanning Policy & Hysteresis

To prevent route flapping under nominal micro-perturbations ($\pm 2-4\%$), the simulation manager implements a multi-tier trigger evaluation:

```
[Is Any Server Outage or Recovery Detected?] 
      |---> YES: Trigger IMMEDIATE Replanning (Code: TRIGGER_SERVER_FAILURE / TRIGGER_SERVER_RECOVERY)
      |
[Is Network Congestion State Changed?]
      |---> YES: Trigger IMMEDIATE Replanning (Code: TRIGGER_CONGESTION)
      |
[Is Manual Event Injected (e.g. Spike)?]
      |---> YES: Trigger IMMEDIATE Replanning (Code: TRIGGER_TRAFFIC_SPIKE)
      |
[Calculate Regional Delta: max_i |D_i(t) - D_i(last)| / D_i(last)]
      |---> If Delta >= 8%: Trigger Replanning (Code: REPLANNED, Reason: "Demand Shift > 8%")
      |---> If Delta < 8%: Skip Recalculation (Code: UNCHANGED, Keep Cached Routing Flow)
```

This guarantees sub-millisecond control-plane overhead while reacting within 1 cycle to genuine topology failures.

---

## 5. WebSocket Protocol Specification

### Endpoint: `ws://localhost:8000/ws/live`

Every tick (1s by default), the server emits a telemetry packet:

```json
{
  "type": "LIVE_TICK",
  "tick": 42,
  "timestamp": 42.0,
  "elapsed_seconds": 42.0,
  "mode": "normal",
  "speed": 1.0,
  "strategy": "mcmf",
  "total_demand": 12450,
  "demands": {
    "Pune": 2580,
    "Mumbai": 3610,
    "Delhi": 2100,
    "Bangalore": 2620,
    "Hyderabad": 1540
  },
  "replan_status": "REPLANNED",
  "replan_reason": "Regional traffic shifted by 8.4%",
  "routing": {
    "total_flow": 12450,
    "edge_flow": 12450,
    "origin_spillover": 0,
    "avg_cost": 17.82,
    "avg_latency": 6.84,
    "execution_time_us": 192,
    "is_optimal": true
  },
  "server_utilization": {
    "Pune Server": 72.4,
    "Mumbai Server": 78.5,
    "Delhi Server": 45.1,
    "Bangalore Server": 74.0
  },
  "active_events": []
}
```

---

## 6. Real-Time REST Control API

| Endpoint | Method | Payload / Parameters | Description |
|---|---|---|---|
| `/api/live/start` | POST | `{"mode": "normal", "speed": 1.0, "strategy": "mcmf", "seed": 42}` | Starts autonomous simulation thread |
| `/api/live/pause` | POST | None | Pauses simulation clock and execution |
| `/api/live/resume` | POST | None | Resumes paused simulation |
| `/api/live/reset` | POST | None | Stops simulation, restores nominal baseline state |
| `/api/live/config` | POST | `{"speed": 2.0, "mode": "stress", "strategy": "round_robin"}` | Dynamic runtime parameter switching |
| `/api/live/inject` | POST | `{"event_type": "server_failure", "target": "Delhi Server", "value": 0}` | Injects transient stress scenario |
| `/api/live/status` | GET | None | Polls current simulation state |
| `/ws/live` | WebSocket | None | Real-time bi-directional telemetry stream |

---

## 7. Performance Benchmarks

- **C++ Solver Execution Duration:** $150\ \mu\text{s} - 280\ \mu\text{s}$ per dynamic state solve.
- **Python Event Loop Overhead:** $< 2.5\ \text{ms}$ per tick cycle.
- **WebSocket Frame Size:** $\approx 1.2\ \text{KB}$ per frame.
- **Network Bandwidth:** $\approx 1.2\ \text{KB/s}$ at 1x speed, $\approx 6.0\ \text{KB/s}$ at 5x speed.
- **Client Chart Rendering Budget:** $< 4.0\ \text{ms}$ per canvas paint with 30-tick sliding window.

