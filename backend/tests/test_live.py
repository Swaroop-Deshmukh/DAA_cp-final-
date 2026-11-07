import pytest
from fastapi.testclient import TestClient
from backend.app import app

client = TestClient(app)


def test_live_status_initial():
    response = client.get("/api/live/status")
    assert response.status_code == 200
    data = response.json()
    assert data["status"] in ("stopped", "running", "paused")
    assert "strategy" in data
    assert "speed" in data


def test_live_start_and_status():
    start_payload = {
        "mode": "normal",
        "speed": 5.0,  # Fast speed for test execution
        "strategy": "Min-Cost Max-Flow",
        "seed": 999
    }
    response = client.post("/api/live/start", json=start_payload)
    assert response.status_code == 200
    data = response.json()
    assert data["status"] == "running"
    assert data["mode"] == "normal"
    assert data["strategy"] == "Min-Cost Max-Flow"

    # Status check
    st_res = client.get("/api/live/status")
    assert st_res.status_code == 200
    assert st_res.json()["status"] == "running"


def test_live_config_update():
    cfg_payload = {
        "mode": "stress",
        "speed": 2.0,
        "strategy": "Nearest Server"
    }
    response = client.post("/api/live/config", json=cfg_payload)
    assert response.status_code == 200
    data = response.json()
    assert data["mode"] == "stress"
    assert data["speed"] == 2.0
    assert data["strategy"] == "Nearest Server"


def test_live_inject_event():
    inject_payload = {
        "event_type": "traffic_spike",
        "target": "Mumbai",
        "value": 2.0
    }
    response = client.post("/api/live/inject", json=inject_payload)
    assert response.status_code == 200
    data = response.json()
    assert data["status"] == "injected"
    assert data["target"] == "Mumbai"


def test_live_pause_resume():
    p_res = client.post("/api/live/pause")
    assert p_res.status_code == 200
    assert p_res.json()["status"] == "paused"

    r_res = client.post("/api/live/resume")
    assert r_res.status_code == 200
    assert r_res.json()["status"] == "running"


def test_live_reset():
    response = client.post("/api/live/reset")
    assert response.status_code == 200
    data = response.json()
    assert data["status"] == "stopped"
    assert data["step"] == 0
    assert data["sim_time_sec"] == 0


def test_live_websocket_connection():
    with client.websocket_connect("/ws/live") as websocket:
        init_msg = websocket.receive_json()
        assert init_msg["type"] in ("INIT_STATE", "RESET_STATE", "LIVE_TICK")
        assert "data" in init_msg

        # Send control action via websocket
        websocket.send_json({"action": "pause"})


def test_live_stress_mode_rerouting():
    # 1. Update mode to stress with MCMF strategy
    res = client.post("/api/live/config", json={"mode": "stress", "strategy": "Min-Cost Max-Flow"})
    assert res.status_code == 200
    assert res.json()["mode"] == "stress"

    # 2. Latest inspector snapshot should reflect stress routing for Pune
    insp_res = client.get("/api/inspector/latest")
    assert insp_res.status_code == 200
    insp_data = insp_res.json()
    snapshot = insp_data["snapshot"]
    assert snapshot["mode"] == "stress"
    assert "candidates_by_region" in snapshot
    pune_cands = snapshot["candidates_by_region"]["Pune"]
    
    # Pune Server has link congestion penalty, Mumbai Server should be chosen
    mumbai_cand = next(c for c in pune_cands if c["server"] == "Mumbai Server")
    assert mumbai_cand["flow_allocated"] > 0
    pune_cand = next(c for c in pune_cands if c["server"] == "Pune Server")
    assert pune_cand["cost_formula"]["unit_cost"] > mumbai_cand["cost_formula"]["unit_cost"]


def test_scenario_synchronization_with_inspector():
    # Trigger scenario
    sc_res = client.post("/api/scenarios/run", json={"scenario": "pune_congestion"})
    assert sc_res.status_code == 200
    
    # Verify inspector has synchronized this exact state
    insp_res = client.get("/api/inspector/latest")
    assert insp_res.status_code == 200
    insp_data = insp_res.json()
    snapshot = insp_data["snapshot"]
    assert "candidates_by_region" in snapshot
    pune_cands = snapshot["candidates_by_region"]["Pune"]
    mumbai_cand = next(c for c in pune_cands if c["server"] == "Mumbai Server")
    assert mumbai_cand["flow_allocated"] == 2500.0


