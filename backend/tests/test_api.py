import pytest
from fastapi.testclient import TestClient
from backend.app import app

client = TestClient(app)


def test_root_welcome():
    response = client.get("/")
    assert response.status_code == 200
    if "text/html" in response.headers.get("content-type", ""):
        assert "Adaptive CDN" in response.text
    else:
        data = response.json()
        assert data["status"] == "online"
        assert data["service"] == "Adaptive CDN Traffic Router API"


def test_dashboard_ui():
    response = client.get("/dashboard")
    assert response.status_code == 200
    assert "Adaptive CDN" in response.text


def test_static_assets_serving():
    css_res = client.get("/css/style.css")
    assert css_res.status_code == 200
    assert "--bg-primary" in css_res.text

    js_res = client.get("/js/app.js")
    assert js_res.status_code == 200
    assert "App" in js_res.text


def test_health_check():
    response = client.get("/api/health")
    assert response.status_code == 200
    data = response.json()
    assert data["status"] == "healthy"
    assert data["available"] is True
    assert "cdn_router" in data["executable"]


def test_network_topology():
    response = client.get("/api/network")
    assert response.status_code == 200
    data = response.json()
    assert data["totalDemand"] == 12000.0
    assert data["totalActiveCapacity"] == 20500.0
    assert len(data["regions"]) == 5
    assert len(data["servers"]) == 4
    assert len(data["links"]) == 20


def test_metrics_overview():
    response = client.get("/api/metrics")
    assert response.status_code == 200
    data = response.json()
    assert data["status"] == "healthy"
    assert data["totalRegions"] == 5
    assert data["totalServers"] == 4
    assert data["totalLinks"] == 20
    assert data["totalDemand"] == 12000.0
    assert data["totalActiveCapacity"] == 20500.0


def test_routing_mcmf():
    response = client.post(
        "/api/routing/run",
        json={"strategy": "Min-Cost Max-Flow", "scenario": "normal"},
    )
    assert response.status_code == 200
    data = response.json()
    assert data["strategyName"] == "Min-Cost Max-Flow"
    assert data["totalTrafficRequested"] == 12000.0
    assert data["totalTrafficRouted"] == 12000.0
    assert data["edgeTrafficRouted"] == 12000.0
    assert data["originTrafficRouted"] == 0.0
    assert data["unservedTraffic"] == 0.0
    assert data["totalRoutingCost"] > 0
    assert 6.0 <= data["averageLatencyMs"] <= 6.5
    assert len(data["allocations"]) > 0


def test_routing_all_strategies():
    strategies = [
        "Round Robin",
        "Nearest Server",
        "Greedy Cost-Based",
        "Min-Cost Max-Flow",
    ]
    for s in strategies:
        response = client.post(
            "/api/routing/run", json={"strategy": s, "scenario": "normal"}
        )
        assert response.status_code == 200
        data = response.json()
        assert data["totalTrafficRouted"] == 12000.0
        assert data["edgeTrafficRouted"] == 12000.0


def test_routing_invalid_strategy():
    response = client.post(
        "/api/routing/run", json={"strategy": "Quantum Neural Network Router"}
    )
    assert response.status_code == 400
    assert "Invalid strategy" in response.json()["detail"]


def test_scenario_mumbai_spike():
    response = client.post(
        "/api/scenarios/run",
        json={"scenario": "mumbai_spike", "strategy": "Min-Cost Max-Flow"},
    )
    assert response.status_code == 200
    data = response.json()
    assert data["totalTrafficRequested"] == 15500.0
    assert data["totalTrafficRouted"] == 15500.0
    assert data["regionalDemands"]["Mumbai"] == 7000.0


def test_scenario_delhi_failure():
    response = client.post(
        "/api/scenarios/run",
        json={"scenario": "delhi_failure", "strategy": "Min-Cost Max-Flow"},
    )
    assert response.status_code == 200
    data = response.json()
    assert data["serverLoads"]["Delhi Server"] == 0.0
    assert "Delhi Server" in data["failedServers"]


def test_scenario_delhi_recovery():
    response = client.post(
        "/api/scenarios/run",
        json={"scenario": "delhi_recovery", "strategy": "Min-Cost Max-Flow"},
    )
    assert response.status_code == 200
    data = response.json()
    assert data["serverLoads"]["Delhi Server"] > 0.0


def test_scenario_pune_congestion():
    response = client.post(
        "/api/scenarios/run",
        json={"scenario": "pune_congestion", "strategy": "Min-Cost Max-Flow"},
    )
    assert response.status_code == 200
    data = response.json()
    assert data["linkLoads"]["Pune->Pune Server"] == 0.0
    assert len(data["congestedLinks"]) > 0


def test_scenario_over_capacity():
    response = client.post(
        "/api/scenarios/run",
        json={"scenario": "over_capacity", "strategy": "Min-Cost Max-Flow"},
    )
    assert response.status_code == 200
    data = response.json()
    assert data["totalTrafficRequested"] == 24000.0
    assert data["edgeTrafficRouted"] == 20500.0
    assert data["originTrafficRouted"] == 3500.0
    assert data["unservedTraffic"] == 0.0


def test_scenario_all_servers_failed():
    response = client.post(
        "/api/scenarios/run",
        json={"scenario": "all_servers_failed", "strategy": "Round Robin"},
    )
    assert response.status_code == 200
    data = response.json()
    assert data["edgeTrafficRouted"] == 0.0
    assert data["originTrafficRouted"] == 12000.0
    assert data["averageLatencyMs"] == 100.0
    assert len(data["failedServers"]) == 4


def test_scenario_invalid():
    response = client.post(
        "/api/scenarios/run", json={"scenario": "unknown_catastrophe"}
    )
    assert response.status_code == 400
    assert "Invalid scenario" in response.json()["detail"]


def test_simulation_traffic_spike():
    response = client.post(
        "/api/simulation/run",
        json={
            "event_type": "traffic_spike",
            "target": "Pune",
            "value": 2.0,
            "strategy": "Min-Cost Max-Flow",
        },
    )
    assert response.status_code == 200
    data = response.json()
    assert data["regionalDemands"]["Pune"] == 5000.0
    assert data["totalTrafficRequested"] == 14500.0


def test_simulation_server_failure():
    response = client.post(
        "/api/simulation/run",
        json={
            "event_type": "server_failure",
            "target": "Mumbai Server",
            "value": 0.0,
            "strategy": "Min-Cost Max-Flow",
        },
    )
    assert response.status_code == 200
    data = response.json()
    assert data["serverLoads"]["Mumbai Server"] == 0.0


def test_simulation_invalid_event():
    response = client.post(
        "/api/simulation/run",
        json={"event_type": "solar_flare", "target": "Pune", "value": 1.0},
    )
    assert response.status_code == 400
    assert "Invalid event type" in response.json()["detail"]


def test_benchmarks_all():
    response = client.get("/api/benchmarks")
    assert response.status_code == 200
    data = response.json()
    assert len(data) == 8
    for sc in data:
        assert "scenario" in sc
        assert len(sc["strategies"]) == 4


def test_benchmarks_single_scenario():
    response = client.get("/api/benchmarks/mumbai_spike")
    assert response.status_code == 200
    data = response.json()
    assert "mumbai_spike" in data["scenario"].lower() or "mumbai" in data["scenario"].lower()
    assert len(data["strategies"]) == 4


def test_benchmarks_invalid_scenario():
    response = client.get("/api/benchmarks/nonexistent_scenario")
    assert response.status_code == 400


def test_cache_optimization():
    response = client.get("/api/cache?budget=350")
    assert response.status_code == 200
    data = response.json()
    assert data["cacheBudgetMB"] == 350
    assert data["totalSizeUsedMB"] <= 350
    assert data["selectedCount"] > 0
    assert len(data["selectedItems"]) > 0


def test_demand_forecasting():
    response = client.get("/api/forecast?region=Pune")
    assert response.status_code == 200
    data = response.json()
    assert data["region"] == "Pune"
    assert len(data["historicalDemand"]) == 5
    assert len(data["forecastSteps"]) == 5
    assert data["finalTrend"] > 0


