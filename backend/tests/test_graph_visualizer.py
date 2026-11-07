import pytest
from fastapi.testclient import TestClient
from backend.app import app

client = TestClient(app)

def test_graph_visualizer_topology_data():
    """Verify authentic C++ graph topology structure returned for the visualizer."""
    # Reset simulation and advance 1 step
    client.post("/api/live/reset")
    client.post("/api/live/step")

    res = client.get("/api/inspector/latest")
    assert res.status_code == 200
    body = res.json()

    assert "trace" in body
    trace = body["trace"]

    # 1. Verify Nodes
    assert "initialNodes" in trace
    nodes = trace["initialNodes"]
    assert len(nodes) == 12  # Source, 5 regions, 4 edge servers, Origin, Sink

    node_names = [n["name"] for n in nodes]
    expected_nodes = [
        "Source (S)",
        "Pune", "Mumbai", "Delhi", "Bangalore", "Hyderabad",
        "Pune Server", "Mumbai Server", "Delhi Server", "Bangalore Server",
        "Origin Server",
        "Sink (T)"
    ]
    for exp in expected_nodes:
        assert exp in node_names, f"Node {exp} missing from C++ graph topology"

    # Verify Node types
    source_node = next(n for n in nodes if n["name"] == "Source (S)")
    assert source_node["type"] == "SOURCE"

    sink_node = next(n for n in nodes if n["name"] == "Sink (T)")
    assert sink_node["type"] == "SINK"

    origin_node = next(n for n in nodes if n["name"] == "Origin Server")
    assert origin_node["type"] == "ORIGIN"

    pune_node = next(n for n in nodes if n["name"] == "Pune")
    assert pune_node["type"] == "REGION"

    pune_srv = next(n for n in nodes if n["name"] == "Pune Server")
    assert pune_srv["type"] == "SERVER"

    # 2. Verify Initial Edges
    assert "initialEdges" in trace
    edges = trace["initialEdges"]
    assert len(edges) > 50  # Forward + reverse residual edges

    # Ensure forward and reverse edges exist
    fwd_edges = [e for e in edges if not e.get("isReverse", False)]
    rev_edges = [e for e in edges if e.get("isReverse", False)]
    assert len(fwd_edges) > 0
    assert len(rev_edges) > 0
    assert len(fwd_edges) == len(rev_edges)

    # Initial flow should be 0 across all edges
    for e in edges:
        assert e["flow"] == 0


def test_graph_visualizer_augmenting_steps():
    """Verify flow augment steps provide valid paths and edge updates for animation."""
    client.post("/api/live/reset")
    client.post("/api/live/step")

    res = client.get("/api/inspector/latest")
    assert res.status_code == 200
    trace = res.json()["trace"]

    augment_steps = [s for s in trace["steps"] if s["phase"] == "FLOW_AUGMENT"]
    assert len(augment_steps) > 0

    for step in augment_steps:
        # Each augment step must have bottleneck flow and valid path
        assert step["bottleneckFlow"] > 0
        path = step["pathNodeNames"]
        assert len(path) >= 3
        assert path[0] == "Source (S)"
        assert path[-1] == "Sink (T)"

        # Edge updates must reflect flow increase
        updates = step["edgeUpdates"]
        assert len(updates) >= 2
        for up in updates:
            if not up["isReverse"]:
                assert up["newFlow"] > up["prevFlow"]
                assert up["newResidual"] < up["prevResidual"]
            else:
                assert up["newResidual"] > up["prevResidual"]


def test_graph_visualizer_replay_strategies_topology():
    """Verify that all baseline strategies export graph topology when replayed."""
    client.post("/api/live/reset")
    client.post("/api/live/step")

    latest = client.get("/api/inspector/latest").json()
    snapshot = latest["snapshot"]

    strategies = ["Round Robin", "Nearest Server", "Greedy Cost"]
    for strat in strategies:
        rep_res = client.post("/api/inspector/replay", json={
            "tick": snapshot["tick"],
            "strategy": strat,
            "demands": snapshot["demands"],
            "failed_servers": snapshot["failed_servers"],
            "congestion_spec": snapshot["congestion_spec"]
        })
        assert rep_res.status_code == 200
        rep_data = rep_res.json()
        assert "trace" in rep_data
        rep_trace = rep_data["trace"]
        assert len(rep_trace["initialNodes"]) == 12
        assert len(rep_trace["initialEdges"]) > 50

