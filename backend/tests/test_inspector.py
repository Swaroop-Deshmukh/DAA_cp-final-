import pytest
from fastapi.testclient import TestClient
from backend.app import app

client = TestClient(app)

def test_inspector_step_and_history():
    # 1. Reset simulation first
    client.post("/api/live/reset")
    
    # 2. Advance exactly 1 step
    res_step1 = client.post("/api/live/step")
    assert res_step1.status_code == 200
    data_step1 = res_step1.json()
    assert data_step1["status"] == "paused"
    assert data_step1["step"] == 1

    # 3. Advance 2 more steps
    client.post("/api/live/step")
    res_step3 = client.post("/api/live/step")
    assert res_step3.status_code == 200
    assert res_step3.json()["step"] == 3

    # 4. Check history endpoint
    res_hist = client.get("/api/inspector/history")
    assert res_hist.status_code == 200
    history = res_hist.json()
    assert len(history) == 3
    assert history[0]["tick"] == 1
    assert history[-1]["tick"] == 3
    assert "total_demand" in history[0]
    assert "total_cost" in history[0]
    assert "average_latency" in history[0]


def test_inspector_latest_snapshot_mcmf_trace():
    # Fetch latest snapshot and authentic C++ trace
    res = client.get("/api/inspector/latest")
    assert res.status_code == 200
    body = res.json()
    
    assert "snapshot" in body
    assert "trace" in body
    assert "routing" in body

    snapshot = body["snapshot"]
    trace = body["trace"]

    assert snapshot["tick"] == 3
    assert trace["strategy"] == "Min-Cost Max-Flow"
    assert trace["totalSteps"] > 5

    phases = [s["phase"] for s in trace["steps"]]
    assert "GRAPH_INIT" in phases
    assert "FLOW_AUGMENT" in phases
    assert "POTENTIAL_UPDATE" in phases
    assert "TERMINATION" in phases

    # Check that residual capacities and paths are present in details
    augment_step = next(s for s in trace["steps"] if s["phase"] == "FLOW_AUGMENT")
    assert "augmenting_path" in augment_step["details"]
    assert "bottleneck_capacity" in augment_step["details"]
    assert "residual_capacity_updates" in augment_step["details"]


def test_inspector_historical_tick_snapshot():
    # Fetch snapshot for tick 2 specifically
    res = client.get("/api/inspector/snapshot/2")
    assert res.status_code == 200
    body = res.json()
    assert body["snapshot"]["tick"] == 2
    assert body["trace"]["totalSteps"] > 0


def test_inspector_replay_round_robin():
    # Replay snapshot with Round Robin
    res = client.post(
        "/api/inspector/replay",
        json={"tick": 2, "strategy": "Round Robin"}
    )
    assert res.status_code == 200
    body = res.json()
    assert body["strategy"] == "Round Robin"
    assert "trace" in body
    trace = body["trace"]
    phases = [s["phase"] for s in trace["steps"]]
    assert "ROTATION_INIT" in phases
    assert "REGION_EVALUATION" in phases
    assert "ASSIGNMENT" in phases
    assert "TERMINATION" in phases


def test_inspector_replay_nearest_server():
    # Replay snapshot with Nearest Server
    res = client.post(
        "/api/inspector/replay",
        json={"tick": 2, "strategy": "Nearest Server"}
    )
    assert res.status_code == 200
    body = res.json()
    assert body["strategy"] == "Nearest Server"
    trace = body["trace"]
    phases = [s["phase"] for s in trace["steps"]]
    assert "STRATEGY_INIT" in phases
    assert "CANDIDATE_EVALUATION" in phases
    assert "ASSIGNMENT" in phases


def test_inspector_replay_greedy_cost():
    # Replay snapshot with Greedy Cost
    res = client.post(
        "/api/inspector/replay",
        json={"tick": 2, "strategy": "Greedy Cost"}
    )
    assert res.status_code == 200
    body = res.json()
    assert body["strategy"] == "Greedy Cost"
    trace = body["trace"]
    phases = [s["phase"] for s in trace["steps"]]
    assert "STRATEGY_INIT" in phases
    assert "ROUTE_EVALUATION" in phases
    assert "ASSIGNMENT" in phases


def test_trace_preserves_algorithm_metrics():
    # Verify that routing output is bit-for-bit identical with and without trace recording
    from backend.services.cpp_engine import cpp_engine
    demands = {"Pune": 2500.0, "Mumbai": 3500.0, "Delhi": 2000.0, "Bangalore": 2500.0, "Hyderabad": 1500.0}
    
    no_trace = cpp_engine.run_dynamic("Min-Cost Max-Flow", demands, record_trace=False)
    with_trace = cpp_engine.run_dynamic("Min-Cost Max-Flow", demands, record_trace=True)

    assert no_trace["totalTrafficRouted"] == with_trace["totalTrafficRouted"]
    assert no_trace["totalRoutingCost"] == with_trace["totalRoutingCost"]
    assert no_trace["averageLatencyMs"] == with_trace["averageLatencyMs"]
    assert no_trace["serverLoads"] == with_trace["serverLoads"]
    assert "trace" in with_trace

