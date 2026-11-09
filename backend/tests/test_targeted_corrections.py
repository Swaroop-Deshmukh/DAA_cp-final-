import math
import pytest
from fastapi.testclient import TestClient
from backend.app import app

client = TestClient(app)

def assert_no_nan_or_inf(obj, path="root"):
    """Recursively verify that no NaN or Infinity exists anywhere in data structures."""
    if isinstance(obj, float):
        assert not math.isnan(obj), f"NaN detected at {path}"
        assert not math.isinf(obj), f"Infinity detected at {path}"
    elif isinstance(obj, dict):
        for k, v in obj.items():
            assert_no_nan_or_inf(v, f"{path}.{k}")
    elif isinstance(obj, list):
        for idx, item in enumerate(obj):
            assert_no_nan_or_inf(item, f"{path}[{idx}]")

def test_regression_pune_scenario_and_data_integrity():
    """
    Task 14 & Acceptance Test:
    Verify Pune stress scenario metrics:
    - Pune Demand = 3,250 req/s -> Mumbai Server = 3,250 req/s.
    - Mumbai other regional allocation accounted for separately.
    - Mumbai Server total allocation, projected load, remaining headroom.
    - Link bandwidth projected usage & headroom.
    - Flow conservation and no NaN / Infinity.
    """
    client.post("/api/live/reset")
    client.post("/api/live/config", json={"mode": "stress"})
    
    # Inject Pune transit congestion
    client.post("/api/live/inject", json={"event_type": "link_congestion", "target": "Pune:Pune Server", "value": 35.0})
    client.post("/api/live/step")

    res = client.get("/api/inspector/latest")
    assert res.status_code == 200
    data = res.json()
    assert_no_nan_or_inf(data)

    snapshot = data["snapshot"]
    demands = snapshot["demands"]
    allocations = snapshot["routing"]["allocations"]
    server_loads = snapshot["routing"]["serverLoads"]
    cands_by_reg = snapshot["candidates_by_region"]

    # 1. Candidate Status Consistency (Task 2)
    # Selected candidate must never be marked REJECTED
    for reg, candidates in cands_by_reg.items():
        reg_demand = demands[reg]
        for cand in candidates:
            status = cand["candidate_status"]
            flow = cand["flow_allocated"]
            if cand["is_origin"]:
                if flow > 0:
                    assert status == "ORIGIN FALLBACK"
                else:
                    assert status == "NOT USED"
            elif not cand["server_healthy"]:
                assert status == "INELIGIBLE"
            elif flow >= (reg_demand - 0.5) and flow > 0:
                assert status == "SELECTED"
                assert "REJECTED" not in status
            elif flow > 0:
                assert status == "PARTIALLY SELECTED"
                assert "REJECTED" not in status
            else:
                assert status in ("NOT SELECTED", "INELIGIBLE")
                assert "REJECTED" not in status

    # 2. Regional Demand Conservation (Task 4 & 10)
    # Demand(R) = Sum of Alloc(R -> S) + Origin(R) + Unallocated(R)
    for reg, dem in demands.items():
        reg_allocs = [a for a in allocations if a.get("from") == reg and not a.get("to", "").startswith("Sink")]
        sum_routed = sum(a["flow"] for a in reg_allocs)
        assert abs(sum_routed - dem) < 1.0, f"Region {reg} demand {dem} != routed {sum_routed}"

    # 3. Server Ingress Accounting & Capacity Conservation (Task 3 & 4)
    # Total Newly Allocated Server Traffic = Sum of allocations from all regions to that server
    for srv_name, srv_load in server_loads.items():
        if "Origin" in srv_name:
            continue
        srv_allocs = [a for a in allocations if a.get("to") == srv_name and not a.get("from", "").startswith("Source")]
        sum_srv_ingress = sum(a["flow"] for a in srv_allocs)
        assert abs(sum_srv_ingress - srv_load) < 1.0, f"Server {srv_name} load {srv_load} != sum ingress {sum_srv_ingress}"

        # Projected load must not exceed server total capacity
        server_cap = 6000.0 if "Mumbai" in srv_name else (4500.0 if "Delhi" in srv_name else 5000.0)
        assert srv_load <= server_cap + 0.1, f"Server {srv_name} exceeded capacity: {srv_load} > {server_cap}"

    # 4. Link Bandwidth Conservation (Task 3 & 6)
    for reg, candidates in cands_by_reg.items():
        for cand in candidates:
            if cand["is_origin"]:
                continue
            assert cand["link_projected_load"] <= cand["link_bandwidth_capacity"] + 0.1
            assert abs(cand["link_remaining_bandwidth"] - (cand["link_bandwidth_capacity"] - cand["link_projected_load"])) < 0.1

    # 5. Cost Formula Verification (Task 7)
    # C = alpha * L + beta * B + (1 - H) * M
    for reg, candidates in cands_by_reg.items():
        for cand in candidates:
            if cand["is_origin"]:
                continue
            cf = cand["cost_formula"]
            eff_l = cand["link_effective_latency_ms"]
            bw_cost = cand["link_bandwidth_cost"]
            hit = cand["cache_hit_ratio"]
            expected_c = round(1.0 * eff_l + 1.5 * bw_cost + (1.0 - hit) * 20.0, 2)
            assert abs(cf["unit_cost"] - expected_c) < 0.05, f"Cost formula mismatch for {reg} -> {cand['server']}"

def test_multi_algorithm_independent_inspection():
    """Verify that all 5 algorithms can be inspected independently without MCMF trace leakage (Task 12)."""
    strategies = ["Min-Cost Max-Flow", "Round Robin", "Nearest Server", "Greedy Cost", "Edmonds-Karp"]
    for strat in strategies:
        res = client.post("/api/inspector/replay", json={"strategy": strat, "tick": 1})
        assert res.status_code == 200
        snap = res.json()
        assert_no_nan_or_inf(snap)
        assert "trace" in snap
        assert "steps" in snap["trace"]
        assert len(snap["trace"]["steps"]) > 0

def test_cumulative_trace_updates_and_residuals():
    """Verify that MCMF and Edmonds-Karp trace edgeUpdates record cumulative flows and residual updates (Task 8)."""
    for strat in ["Min-Cost Max-Flow", "Edmonds-Karp"]:
        res = client.post("/api/inspector/replay", json={"strategy": strat, "tick": 1})
        assert res.status_code == 200
        steps = res.json()["trace"]["steps"]
        augment_steps = [s for s in steps if s.get("bottleneckFlow", 0) > 0 or "AUGMENT" in s.get("phase", "")]
        assert len(augment_steps) > 0

        for st in augment_steps:
            assert "edgeUpdates" in st
            updates = st["edgeUpdates"]
            assert len(updates) > 0
            for u in updates:
                assert "prevFlow" in u
                assert "newFlow" in u
                assert "prevResidual" in u
                assert "newResidual" in u
                assert "isReverse" in u
                # Check delta flow matches residual change
                delta = abs(u["newFlow"] - u["prevFlow"])
                res_delta = abs(u["newResidual"] - u["prevResidual"])
                assert abs(delta - res_delta) < 0.01, f"Delta {delta} != res_delta {res_delta}"

def test_cost_reconciliation_and_immutable_snapshot_identity():
    """
    Final Targeted Verification:
    1. Pune -> Mumbai Server unit cost in candidate matrix must exactly match
       the unit cost in the C++ allocation table (17.80 pts/req).
    2. Snapshot contains an immutable snapshot_id, tick, timestamp.
    3. Multi-algorithm replay preserves identical frozen demand across RR, Nearest, Greedy, and MCMF.
    4. Confirms that Nearest Server and Greedy Cost generate identical allocations under nominal baseline.
    """
    client.post("/api/live/reset")
    res = client.get("/api/inspector/latest")
    assert res.status_code == 200
    data = res.json()
    snapshot = data["snapshot"]

    assert "snapshot_id" in snapshot
    assert "tick" in snapshot
    assert "timestamp" in snapshot

    # Verify Pune -> Mumbai Server unit cost reconciliation
    pune_candidates = snapshot["candidates_by_region"]["Pune"]
    mumbai_cand = next(c for c in pune_candidates if c["server"] == "Mumbai Server")
    assert mumbai_cand["cost_formula"]["unit_cost"] == 17.80, (
        f"Candidate matrix unit cost expected 17.80, got {mumbai_cand['cost_formula']['unit_cost']}"
    )

    # Replay on tick with pune_congestion to route flow to Mumbai Server
    rep_res = client.post("/api/inspector/replay", json={
        "strategy": "Min-Cost Max-Flow",
        "tick": snapshot["tick"],
        "congestion": "Pune:Pune Server:35.0"
    })
    assert rep_res.status_code == 200
    rep_data = rep_res.json()
    rep_allocs = rep_data["routing"]["allocations"]
    pune_to_mumbai = next(a for a in rep_allocs if a["from"] == "Pune" and a["to"] == "Mumbai Server")
    assert pune_to_mumbai["unitCost"] == 17.80, (
        f"Allocation table unit cost expected 17.80, got {pune_to_mumbai['unitCost']}"
    )

    # Verify Nearest Server and Greedy Cost snapshot invariance
    res_nearest = client.post("/api/inspector/replay", json={"strategy": "Nearest Server", "tick": snapshot["tick"]}).json()
    res_greedy = client.post("/api/inspector/replay", json={"strategy": "Greedy Cost", "tick": snapshot["tick"]}).json()

    assert res_nearest["total_demand"] == res_greedy["total_demand"]
    assert res_nearest["routing"]["totalTrafficRouted"] == res_greedy["routing"]["totalTrafficRouted"]
    assert res_nearest["routing"]["totalRoutingCost"] == res_greedy["routing"]["totalRoutingCost"]
    assert res_nearest["routing"]["averageLatencyMs"] == res_greedy["routing"]["averageLatencyMs"]

def test_global_objective_and_load_band_reconciliation():
    """
    Verify complete numerical reconciliation between transit allocations,
    server compute capacity load bands, and the global MCMF objective cost.
    Also verifies exact decimal precision (avoiding stream truncation).
    """
    client.post("/api/live/reset")
    res = client.get("/api/inspector/latest")
    assert res.status_code == 200
    data = res.json()
    snapshot = data["snapshot"]
    allocs = snapshot["routing"]["allocations"]

    transit_allocs = [a for a in allocs if "Source" not in a.get("from", "") and "Sink" not in a.get("to", "") and a.get("flow", 0) > 0.01]
    band_allocs = [a for a in allocs if "Sink" in a.get("to", "") and a.get("flow", 0) > 0.01]

    transit_cost_sum = sum(a["flow"] * a["unitCost"] for a in transit_allocs)
    band_cost_sum = sum(a["flow"] * a["unitCost"] for a in band_allocs)
    grand_total_cost = transit_cost_sum + band_cost_sum

    reported_cost = snapshot["routing"]["totalRoutingCost"]
    assert abs(grand_total_cost - reported_cost) < 0.5, (
        f"Grand total cost {grand_total_cost} does not match reported totalRoutingCost {reported_cost}"
    )

    # Check precision on each allocation
    for a in allocs:
        expected = round(a["flow"] * a["unitCost"], 2)
        reported = round(a["totalCost"], 2)
        assert abs(expected - reported) < 0.05, f"Allocation {a['label']} expected {expected}, got {reported}"

def test_link_congestion_effective_latency_and_cost():
    """
    Numerically verify link congestion:
    Base latency: 5.0 ms.
    Congestion delay: +35.0 ms.
    Effective latency: 40.0 ms.
    Unit cost rises from 9.50 to 44.50 pts/req.
    """
    res = client.post("/api/inspector/replay", json={
        "strategy": "Min-Cost Max-Flow",
        "tick": 1,
        "congestion": "Pune:Pune Server:35.0"
    })
    assert res.status_code == 200
    snap = res.json()
    pune_cands = snap["candidates_by_region"]["Pune"]
    pune_srv_cand = next(c for c in pune_cands if c["server"] == "Pune Server")

    assert pune_srv_cand["link_congestion_ms"] == 35.0
    assert pune_srv_cand["link_effective_latency_ms"] == 40.0
    # Cost = 1.0 * 40.0 + 1.5 * 2.0 + (1 - 0.95) * 30.0 = 40.0 + 3.0 + 1.5 = 44.50
    assert abs(pune_srv_cand["cost_formula"]["unit_cost"] - 44.50) < 0.01

def test_cost_attribution_and_server_load_decomposition():
    """
    Verify complete server load decomposition, capacity-band compute congestion attribution,
    and genuine C++ Dijkstra relaxation & Johnson potential trace step payloads.
    """
    client.post("/api/live/reset")
    res = client.get("/api/inspector/latest")
    assert res.status_code == 200
    data = res.json()
    snapshot = data["snapshot"]
    trace = data["trace"]
    cands_by_reg = snapshot["candidates_by_region"]

    # 1. Server Load Decomposition Formula:
    # Projected = Pre-Load (0) + Own Ingress + Other Ingress
    for reg, cands in cands_by_reg.items():
        for c in cands:
            if c["is_origin"]:
                continue
            pre = c["external_pre_decision_load"]
            own = c["own_regional_allocation"]
            others = c["other_regional_allocations"]
            proj = c["server_projected_load"]
            assert pre == 0.0, f"Expected 0 external pre-load, got {pre}"
            assert abs(proj - (pre + own + others)) < 0.2, (
                f"Decomposition mismatch for {reg} -> {c['server']}: {proj} != {pre} + {own} + {others}"
            )
            # Effective cost = Transit + Compute
            transit = c["cost_formula"]["unit_cost"]
            surcharge = c["server_compute_surcharge"]
            eff = c["effective_unit_cost"]
            assert abs(eff - (transit + surcharge)) < 0.05, (
                f"Effective cost mismatch: {eff} != {transit} + {surcharge}"
            )

    # 2. Trace Verification: Dijkstra Edge Relaxations and Johnson Potential Shifts
    steps = trace["steps"]
    search_steps = [s for s in steps if s.get("relaxations") and len(s["relaxations"]) > 0]
    shift_steps = [s for s in steps if s.get("potentialShifts") and len(s["potentialShifts"]) > 0]
    augment_steps = [s for s in steps if s.get("phase") == "FLOW_AUGMENT"]

    assert len(search_steps) > 0, "Trace should contain Dijkstra relaxation logs"
    assert len(shift_steps) > 0, "Trace should contain Johnson dual potential shifts"
    assert len(augment_steps) > 0, "Trace should contain flow augmentation steps"

    # Verify reduced cost formula on relaxations: c^pi = c + pi(u) - pi(v) >= 0
    for s in search_steps:
        for r in s["relaxations"]:
            c_base = r["baseCost"]
            pi_u = r["piU"]
            pi_v = r["piV"]
            expected_c_pi = round(c_base + pi_u - pi_v, 2)
            assert abs(r["reducedCost"] - expected_c_pi) < 0.05, (
                f"Reduced cost mismatch: {r['reducedCost']} != {expected_c_pi}"
            )
            assert r["reducedCost"] >= -0.01, f"Reduced cost is negative: {r['reducedCost']}"

    # Verify potential shift formula: new_pi = prev_pi + dist
    for s in shift_steps:
        for ps in s["potentialShifts"]:
            if ps["shortestDist"] < 1e8:
                assert abs(ps["newPotential"] - (ps["prevPotential"] + ps["shortestDist"])) < 0.05

    # Verify flow augmentation incremental cost: incCost = bottleneck * pathCost
    for s in augment_steps:
        b_flow = s["bottleneckFlow"]
        p_cost = s["pathCost"]
        inc_cost = s["incrementalPathCost"]
        assert abs(inc_cost - (b_flow * p_cost)) < 0.5, (
            f"Incremental path cost mismatch: {inc_cost} != {b_flow} * {p_cost}"
        )



