# REST API Specification & Integration Guide

**Adaptive CDN Traffic Router API (Phase 9)**  
*Complete technical documentation for the Python FastAPI backend bridge connecting to the C++17 routing engine.*

---

## 1. Quickstart & Server Execution

### Prerequisites
- Python 3.10+ (tested on Python 3.13)
- C++ Engine binary compiled at `cpp-engine/build/Release/cdn_router.exe`

### Installation
From the project root directory:

```powershell
pip install -r backend/requirements.txt
```

### Starting the Server

```powershell
# Option A: Start using Python module
python -m uvicorn backend.app:app --host 127.0.0.1 --port 8000 --reload

# Option B: Direct Python execution
python backend/app.py
```

- **Interactive Swagger UI:** [http://127.0.0.1:8000/docs](http://127.0.0.1:8000/docs)
- **ReDoc Documentation:** [http://127.0.0.1:8000/redoc](http://127.0.0.1:8000/redoc)

---

## 2. API Endpoint Directory

| Method | Endpoint | Description |
| :--- | :--- | :--- |
| `GET` | `/` | Service root welcome and status metadata |
| `GET` | `/api/health` | Health check and C++ engine binary discovery |
| `GET` | `/api/network` | Retrieves current CDN network topology, regions, servers, and links |
| `GET` | `/api/metrics` | Fleet-wide capacity, demand, and utilization summary |
| `POST` | `/api/routing/run` | Executes routing on C++ engine using specified algorithm |
| `POST` | `/api/simulation/run` | Injects dynamic network event and replans traffic |
| `POST` | `/api/scenarios/run` | Runs a preset operational test scenario |
| `GET` | `/api/benchmarks` | Comparative evaluation of all 4 algorithms across all 8 scenarios |
| `GET` | `/api/benchmarks/{scenario_id}` | Comparative evaluation of all 4 algorithms for a single scenario |

---

## 3. Endpoint Specifications

### 3.1 GET `/api/health`
Checks whether the FastAPI service is running and confirms that the compiled C++17 executable exists and is executable.

#### Example Request
```bash
curl -X GET "http://127.0.0.1:8000/api/health"
```

#### Example Response (`200 OK`)
```json
{
  "status": "healthy",
  "engine": "C++17 Adaptive CDN Router Engine (SSP Dijkstra + Johnson)",
  "executable": "C:\\MY_DATA\\DAA_cp(final)\\cpp-engine\\build\\Release\\cdn_router.exe",
  "available": true
}
```

---

### 3.2 GET `/api/network`
Returns the complete network topology including user regions, edge servers, link latency, bandwidth costs, bandwidth capacities, and cache hit ratios.

#### Example Request
```bash
curl -X GET "http://127.0.0.1:8000/api/network"
```

#### Example Response (`200 OK`)
```json
{
  "totalDemand": 12000.0,
  "totalActiveCapacity": 20500.0,
  "regions": [
    {"name": "Pune", "trafficDemand": 2500.0},
    {"name": "Mumbai", "trafficDemand": 3500.0},
    {"name": "Delhi", "trafficDemand": 2000.0},
    {"name": "Bangalore", "trafficDemand": 2500.0},
    {"name": "Hyderabad", "trafficDemand": 1500.0}
  ],
  "servers": [
    {"name": "Pune Server", "configuredCapacity": 5000.0, "availableCapacity": 5000.0, "isActive": true},
    {"name": "Mumbai Server", "configuredCapacity": 6000.0, "availableCapacity": 6000.0, "isActive": true},
    {"name": "Delhi Server", "configuredCapacity": 4500.0, "availableCapacity": 4500.0, "isActive": true},
    {"name": "Bangalore Server", "configuredCapacity": 5000.0, "availableCapacity": 5000.0, "isActive": true}
  ],
  "links": [
    {
      "regionName": "Pune",
      "serverName": "Pune Server",
      "baseLatencyMs": 5.0,
      "congestionLatencyMs": 0.0,
      "effectiveLatencyMs": 5.0,
      "bandwidthCost": 1.0,
      "bandwidthCapacity": 4000.0,
      "cacheHitRatio": 0.85
    }
  ]
}
```

---

### 3.3 GET `/api/metrics`
Returns high-level system performance indicators, fleet capacity, and strategy availability.

#### Example Response (`200 OK`)
```json
{
  "totalRegions": 5,
  "totalServers": 4,
  "totalLinks": 20,
  "totalDemand": 12000.0,
  "totalActiveCapacity": 20500.0,
  "fleetUtilization": 58.53658536585366,
  "availableStrategies": [
    "Round Robin",
    "Nearest Server",
    "Greedy Cost-Based",
    "Min-Cost Max-Flow"
  ],
  "status": "healthy"
}
```

---

### 3.4 POST `/api/routing/run`
Executes traffic routing on the C++ engine.

#### Request Body
```json
{
  "strategy": "Min-Cost Max-Flow",
  "scenario": "normal"
}
```

*Supported Strategies:* `"Min-Cost Max-Flow"`, `"Round Robin"`, `"Nearest Server"`, `"Greedy Cost-Based"`.

#### Example Response (`200 OK`)
```json
{
  "strategyName": "Min-Cost Max-Flow",
  "scenarioName": "normal",
  "totalTrafficRequested": 12000.0,
  "edgeTrafficRouted": 12000.0,
  "originTrafficRouted": 0.0,
  "totalTrafficRouted": 12000.0,
  "unservedTraffic": 0.0,
  "totalRoutingCost": 183712.0,
  "averageRoutingCostPerReq": 15.3094,
  "averageLatencyMs": 6.14583,
  "executionTimeMicroseconds": 64,
  "serverLoads": {
    "Bangalore Server": 3750.0,
    "Delhi Server": 2000.0,
    "Mumbai Server": 3500.0,
    "Pune Server": 2750.0
  },
  "serverUtilPct": {
    "Bangalore Server": 75.0,
    "Delhi Server": 44.4444,
    "Mumbai Server": 58.3333,
    "Pune Server": 55.0
  },
  "linkLoads": {
    "Pune->Pune Server": 2500.0,
    "Mumbai->Mumbai Server": 3500.0,
    "Delhi->Delhi Server": 2000.0,
    "Bangalore->Bangalore Server": 2500.0,
    "Hyderabad->Bangalore Server": 1250.0,
    "Hyderabad->Pune Server": 250.0
  },
  "linkUtilPct": {
    "Pune->Pune Server": 62.5,
    "Mumbai->Mumbai Server": 70.0,
    "Delhi->Delhi Server": 44.44,
    "Bangalore->Bangalore Server": 55.55
  },
  "regionalDemands": {
    "Pune": 2500.0,
    "Mumbai": 3500.0,
    "Delhi": 2000.0,
    "Bangalore": 2500.0,
    "Hyderabad": 1500.0
  },
  "failedServers": [],
  "congestedLinks": [],
  "allocations": [
    {
      "from": "Pune",
      "to": "Pune Server",
      "flow": 2500.0,
      "capacity": 4000.0,
      "unitCost": 9.5,
      "totalCost": 23750.0,
      "label": "Pune -> Pune Server"
    }
  ],
  "explanation": "Min-Cost Max-Flow utilizes Successive Shortest Path (SSP) with Johnson dual potentials and 4-tier convex server load bands to achieve the mathematically optimal global tradeoff between latency, bandwidth expense, cache efficiency, and server congestion."
}
```

---

### 3.5 POST `/api/simulation/run`
Applies a dynamic network disturbance and returns the re-optimized routing solution.

#### Request Body
```json
{
  "event_type": "traffic_spike",
  "target": "Pune",
  "value": 2.0,
  "strategy": "Min-Cost Max-Flow"
}
```

*Supported Event Types:*
- `"traffic_spike"`: Multiplies target region demand by `value` (e.g. `2.0` doubles demand).
- `"server_failure"`: Marks target server offline (available capacity drops to 0).
- `"server_recovery"`: Restores target server online.
- `"congestion"`: Injects transient latency delay (ms) on link between `target` (region) and `secondary_target` (server).

---

### 3.6 POST `/api/scenarios/run`
Runs one of the 8 canonical operational test scenarios.

#### Request Body
```json
{
  "scenario": "mumbai_spike",
  "strategy": "Min-Cost Max-Flow"
}
```

*Supported Scenario Keys:*
- `"normal"`: Standard baseline ($12,000\text{ req/s}$)
- `"mumbai_spike"`: Mumbai regional surge ($3,500 \to 7,000\text{ req/s}$)
- `"delhi_failure"`: Delhi Server offline
- `"delhi_recovery"`: Delhi Server restored
- `"pune_congestion"`: Pune link delayed by $+35\text{ ms}$
- `"over_capacity"`: Traffic exceeding edge capacity ($24,000\text{ req/s}$)
- `"all_servers_failed"`: Total edge blackout ($100\%$ Origin fallback)
- `"cache_improvement"`: High cache hit ratio optimization ($0.98 / 0.99$)

---

### 3.7 GET `/api/benchmarks` and `/api/benchmarks/{scenario_id}`
Retrieves comparative metrics for all 4 routing strategies across all 8 scenarios (or a specific scenario).

#### Example Request
```bash
curl -X GET "http://127.0.0.1:8000/api/benchmarks/delhi_failure"
```

#### Example Response (`200 OK`)
```json
{
  "scenario": "delhi_failure",
  "strategies": [
    {
      "strategyName": "Round Robin",
      "totalTrafficRequested": 12000.0,
      "edgeTrafficRouted": 12000.0,
      "originTrafficRouted": 0.0,
      "totalTrafficRouted": 12000.0,
      "unservedTraffic": 0.0,
      "totalRoutingCost": 402108.0,
      "averageRoutingCostPerReq": 33.509,
      "averageLatencyMs": 17.9306,
      "executionTimeMicroseconds": 18,
      "serverUtilPct": {"Bangalore Server": 80.0, "Delhi Server": 0.0, "Mumbai Server": 66.67, "Pune Server": 80.0},
      "serverLoads": {"Bangalore Server": 4000.0, "Delhi Server": 0.0, "Mumbai Server": 4000.0, "Pune Server": 4000.0}
    },
    {
      "strategyName": "Min-Cost Max-Flow",
      "totalTrafficRequested": 12000.0,
      "edgeTrafficRouted": 12000.0,
      "originTrafficRouted": 0.0,
      "totalTrafficRouted": 12000.0,
      "unservedTraffic": 0.0,
      "totalRoutingCost": 265512.0,
      "averageRoutingCostPerReq": 22.126,
      "averageLatencyMs": 10.1458,
      "executionTimeMicroseconds": 47,
      "serverUtilPct": {"Bangalore Server": 75.0, "Delhi Server": 0.0, "Mumbai Server": 75.0, "Pune Server": 75.0},
      "serverLoads": {"Bangalore Server": 3750.0, "Delhi Server": 0.0, "Mumbai Server": 4500.0, "Pune Server": 3750.0}
    }
  ]
}
```

---

## 4. Error Responses

### Invalid Strategy (`400 Bad Request`)
```json
{
  "detail": "Invalid strategy 'Quantum Router'. Supported strategies: ['Round Robin', 'Nearest Server', 'Greedy Cost', 'Min-Cost Max-Flow']"
}
```

### Invalid Scenario (`400 Bad Request`)
```json
{
  "detail": "Invalid scenario 'space_outage'. Supported scenarios: ['normal', 'mumbai_spike', 'delhi_failure', 'delhi_recovery', 'pune_congestion', 'over_capacity', 'all_servers_failed', 'cache_improvement']"
}
```

### Engine Binary Missing (`500 Internal Server Error`)
```json
{
  "detail": "C++ engine executable not found at '...\\cdn_router.exe'. Please compile with CMake first."
}
```

