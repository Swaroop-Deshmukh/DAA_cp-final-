import pytest
from fastapi.testclient import TestClient
from backend.app import app

client = TestClient(app)

def test_candidates_matrix_all_regions():
    """Verify that every region evaluates all 4 edge servers and Origin fallback."""
    # Reset and step once
    client.post("/api/live/reset")
    client.post("/api/live/step")

    res = client.get("/api/inspector/latest")
    assert res.status_code == 200
    snapshot = res.json()["snapshot"]

    assert "candidates_by_region" in snapshot
    cands_by_reg = snapshot["candidates_by_region"]

    expected_regions = ["Pune", "Mumbai", "Delhi", "Bangalore", "Hyderabad"]
    expected_servers = ["Pune Server", "Mumbai Server", "Delhi Server", "Bangalore Server", "Origin Server"]

    for reg in expected_regions:
        assert reg in cands_by_reg, f"Region {reg} missing from candidates matrix"
        candidates = cands_by_reg[reg]
        assert len(candidates) == 5, f"Expected 5 candidates for {reg}, got {len(candidates)}"

        cand_servers = [c["server"] for c in candidates]
        for exp_srv in expected_servers:
            assert exp_srv in cand_servers, f"Server {exp_srv} missing from {reg} candidates"

        # Check candidate fields
        for cand in candidates:
            assert "server_healthy" in cand
            assert "server_total_capacity" in cand
            assert "server_remaining_capacity" in cand
            assert "link_effective_latency_ms" in cand
            assert "link_bandwidth_capacity" in cand
            assert "link_remaining_bandwidth" in cand
            assert "cost_formula" in cand
            assert "eligibility" in cand
            assert "exclusion_reason" in cand
            assert "flow_allocated" in cand
            assert "allocation_pct" in cand
            assert "total_cost" in cand

            # Cost formula validation: C = alpha * L + beta * B + (1-H)*M
            cf = cand["cost_formula"]
            if not cand["is_origin"]:
                expected_unit = round(cf["latency_term"] + cf["bandwidth_term"] + cf["cache_miss_term"], 2)
                assert abs(cf["unit_cost"] - expected_unit) < 0.05
            else:
                assert cand["eligibility"] == "ORIGIN_FALLBACK"


def test_candidates_hardware_outage_exclusion():
    """Verify that when a server fails, it is marked EXCLUDED_OUTAGE with 0 flow."""
    client.post("/api/live/reset")
    # Fail Delhi Server
    client.post("/api/live/inject", json={"event_type": "server_failure", "target": "Delhi Server", "value": 1.0})
    client.post("/api/live/step")

    res = client.get("/api/inspector/latest")
    assert res.status_code == 200
    snapshot = res.json()["snapshot"]

    delhi_cands = snapshot["candidates_by_region"]["Delhi"]
    delhi_srv = next(c for c in delhi_cands if c["server"] == "Delhi Server")

    assert delhi_srv["server_healthy"] is False
    assert delhi_srv["eligibility"] == "EXCLUDED_OUTAGE"
    assert "outage" in delhi_srv["exclusion_reason"].lower()
    assert delhi_srv["flow_allocated"] == 0.0

    # Other healthy servers or fallback must handle Delhi's demand
    delhi_demand = snapshot["demands"]["Delhi"]
    allocated_sum = sum(c["flow_allocated"] for c in delhi_cands)
    assert abs(allocated_sum - delhi_demand) < 1.0


def test_candidates_recovery_restoration():
    """Verify that recovered server becomes ELIGIBLE again."""
    client.post("/api/live/inject", json={"event_type": "server_recovery", "target": "Delhi Server", "value": 1.0})
    client.post("/api/live/step")

    res = client.get("/api/inspector/latest")
    assert res.status_code == 200
    snapshot = res.json()["snapshot"]

    delhi_cands = snapshot["candidates_by_region"]["Delhi"]
    delhi_srv = next(c for c in delhi_cands if c["server"] == "Delhi Server")

    assert delhi_srv["server_healthy"] is True
    assert delhi_srv["eligibility"] == "ELIGIBLE"
    assert delhi_srv["flow_allocated"] > 0.0

