"""
Continuous Real-Time Simulation Manager
Generates realistic dynamic CDN traffic, coordinates network state changes,
applies intelligent replanning policies, executes C++ routing algorithms,
and streams telemetry to connected WebSocket clients.
"""

import asyncio
import collections
import math
import random
import time
from typing import Dict, Any, List, Optional, Set
from fastapi import WebSocket
from backend.services.cpp_engine import cpp_engine


def parse_congested_links(congested_links: List[str]) -> str:
    """Parses C++ congested link strings (e.g. 'Pune->Pune Server (+35.000000ms)') into spec format."""
    if not congested_links:
        return "none"
    for item in congested_links:
        if "->" in item and "(+" in item:
            parts = item.split("->")
            reg = parts[0].strip()
            rest = parts[1]
            server_part = rest.split("(+")[0].strip()
            delay_part = rest.split("(+")[1].replace("ms)", "").strip()
            try:
                return f"{reg}:{server_part}:{float(delay_part):.1f}"
            except Exception:
                return f"{reg}:{server_part}:35.0"
    return "none"


# Canonical CDN Fleet & Interconnect Link Specifications (Single Source of Truth)
SERVERS_SPEC = {
    "Pune Server": {"capacity": 5000.0, "is_origin": False},
    "Mumbai Server": {"capacity": 6000.0, "is_origin": False},
    "Delhi Server": {"capacity": 4500.0, "is_origin": False},
    "Bangalore Server": {"capacity": 5000.0, "is_origin": False},
    "Origin Server": {"capacity": 999999.0, "is_origin": True},
}

LINKS_SPEC = {
    ("Pune", "Pune Server"): {"base_latency": 5.0, "cost": 1.0, "bw_capacity": 4000.0, "hit_ratio": 0.85},
    ("Pune", "Mumbai Server"): {"base_latency": 12.0, "cost": 1.2, "bw_capacity": 4000.0, "hit_ratio": 0.80},
    ("Pune", "Delhi Server"): {"base_latency": 32.0, "cost": 2.5, "bw_capacity": 3000.0, "hit_ratio": 0.70},
    ("Pune", "Bangalore Server"): {"base_latency": 22.0, "cost": 1.8, "bw_capacity": 3500.0, "hit_ratio": 0.75},
    ("Pune", "Origin Server"): {"base_latency": 100.0, "cost": 5.0, "bw_capacity": 99999.0, "hit_ratio": 0.0},

    ("Mumbai", "Pune Server"): {"base_latency": 12.0, "cost": 1.2, "bw_capacity": 4000.0, "hit_ratio": 0.80},
    ("Mumbai", "Mumbai Server"): {"base_latency": 4.0, "cost": 1.0, "bw_capacity": 5000.0, "hit_ratio": 0.90},
    ("Mumbai", "Delhi Server"): {"base_latency": 28.0, "cost": 2.2, "bw_capacity": 3500.0, "hit_ratio": 0.70},
    ("Mumbai", "Bangalore Server"): {"base_latency": 24.0, "cost": 1.9, "bw_capacity": 3500.0, "hit_ratio": 0.75},
    ("Mumbai", "Origin Server"): {"base_latency": 100.0, "cost": 5.0, "bw_capacity": 99999.0, "hit_ratio": 0.0},

    ("Delhi", "Pune Server"): {"base_latency": 32.0, "cost": 2.5, "bw_capacity": 3000.0, "hit_ratio": 0.70},
    ("Delhi", "Mumbai Server"): {"base_latency": 28.0, "cost": 2.2, "bw_capacity": 3500.0, "hit_ratio": 0.75},
    ("Delhi", "Delhi Server"): {"base_latency": 6.0, "cost": 1.0, "bw_capacity": 4500.0, "hit_ratio": 0.85},
    ("Delhi", "Bangalore Server"): {"base_latency": 42.0, "cost": 3.0, "bw_capacity": 2500.0, "hit_ratio": 0.65},
    ("Delhi", "Origin Server"): {"base_latency": 100.0, "cost": 5.0, "bw_capacity": 99999.0, "hit_ratio": 0.0},

    ("Bangalore", "Pune Server"): {"base_latency": 22.0, "cost": 1.8, "bw_capacity": 3500.0, "hit_ratio": 0.75},
    ("Bangalore", "Mumbai Server"): {"base_latency": 24.0, "cost": 1.9, "bw_capacity": 3500.0, "hit_ratio": 0.75},
    ("Bangalore", "Delhi Server"): {"base_latency": 42.0, "cost": 3.0, "bw_capacity": 2500.0, "hit_ratio": 0.65},
    ("Bangalore", "Bangalore Server"): {"base_latency": 5.0, "cost": 1.0, "bw_capacity": 4500.0, "hit_ratio": 0.88},
    ("Bangalore", "Origin Server"): {"base_latency": 100.0, "cost": 5.0, "bw_capacity": 99999.0, "hit_ratio": 0.0},

    ("Hyderabad", "Pune Server"): {"base_latency": 16.0, "cost": 1.4, "bw_capacity": 3000.0, "hit_ratio": 0.80},
    ("Hyderabad", "Mumbai Server"): {"base_latency": 20.0, "cost": 1.6, "bw_capacity": 3000.0, "hit_ratio": 0.75},
    ("Hyderabad", "Delhi Server"): {"base_latency": 35.0, "cost": 2.6, "bw_capacity": 2500.0, "hit_ratio": 0.65},
    ("Hyderabad", "Bangalore Server"): {"base_latency": 15.0, "cost": 1.3, "bw_capacity": 3500.0, "hit_ratio": 0.82},
    ("Hyderabad", "Origin Server"): {"base_latency": 100.0, "cost": 5.0, "bw_capacity": 99999.0, "hit_ratio": 0.0},
}

class ContinuousSimulationManager:
    def __init__(self):
        # Simulation lifecycle
        self.status: str = "stopped"  # "stopped", "running", "paused"
        self.mode: str = "normal"      # "normal" or "stress"
        self.speed: float = 1.0        # 1.0 = 1s, 2.0 = 0.5s, 5.0 = 0.2s
        self.strategy: str = "Min-Cost Max-Flow"
        self.seed: int = 42
        self.step: int = 0
        self.sim_time_sec: int = 0

        # Background task & locking
        self._task: Optional[asyncio.Task] = None
        self._lock = asyncio.Lock()
        self._active_connections: Set[WebSocket] = set()

        # Bounded decision history for Inspection & Time-Travel (Max 100 snapshots)
        self.history: collections.deque = collections.deque(maxlen=100)

        # Base regional nominal demands
        self.base_demands = {
            "Pune": 2500.0,
            "Mumbai": 3500.0,
            "Delhi": 2000.0,
            "Bangalore": 2500.0,
            "Hyderabad": 1500.0,
        }

        # Network condition state
        self.rng = random.Random(self.seed)
        self.current_demands: Dict[str, float] = dict(self.base_demands)
        self.failed_servers: List[str] = []
        self.congestion_spec: str = "none"
        self.cache_hit_ratio: float = 0.85

        # Replanning memory & hysteresis
        self.last_demands: Dict[str, float] = dict(self.base_demands)
        self.last_failed_servers: List[str] = []
        self.last_congestion: str = "none"
        self.last_routing_result: Optional[Dict[str, Any]] = None
        self.last_decision_reason: str = "INITIAL_PLAN"
        self.previous_cost: float = 0.0

    async def connect_client(self, websocket: WebSocket):
        await websocket.accept()
        self._active_connections.add(websocket)
        # Send current status snapshot immediately upon connection
        snapshot = self.get_telemetry_snapshot()
        await websocket.send_json({"type": "INIT_STATE", "data": snapshot})

    def disconnect_client(self, websocket: WebSocket):
        self._active_connections.discard(websocket)

    async def broadcast(self, message: Dict[str, Any]):
        dead_connections = set()
        for ws in self._active_connections:
            try:
                await ws.send_json(message)
            except Exception:
                dead_connections.add(ws)
        self._active_connections.difference_update(dead_connections)

    async def start(self, mode: str = "normal", speed: float = 1.0, strategy: str = "Min-Cost Max-Flow", seed: int = 42):
        async with self._lock:
            if self.status == "running":
                return self.get_status()

            self.mode = mode
            self.speed = max(0.5, min(10.0, speed))
            self.strategy = strategy
            self.seed = seed
            self.rng = random.Random(self.seed)
            self.status = "running"

            if self._task is None or self._task.done():
                self._task = asyncio.create_task(self._simulation_loop())

        return self.get_status()

    async def pause(self):
        async with self._lock:
            if self.status == "running":
                self.status = "paused"
        await self.broadcast({"type": "STATUS_CHANGE", "status": self.status})
        return self.get_status()

    async def resume(self):
        async with self._lock:
            if self.status == "paused":
                self.status = "running"
                if self._task is None or self._task.done():
                    self._task = asyncio.create_task(self._simulation_loop())
        await self.broadcast({"type": "STATUS_CHANGE", "status": self.status})
        return self.get_status()

    async def reset(self):
        async with self._lock:
            self.status = "stopped"
            if self._task and not self._task.done():
                self._task.cancel()
                self._task = None

            self.step = 0
            self.sim_time_sec = 0
            self.mode = "normal"
            self.strategy = "Min-Cost Max-Flow"
            self.rng = random.Random(self.seed)
            self.current_demands = dict(self.base_demands)
            self.failed_servers = []
            self.congestion_spec = "none"
            self.last_demands = dict(self.base_demands)
            self.last_failed_servers = []
            self.last_congestion = "none"
            self.last_routing_result = None
            self.last_decision_reason = "RESET"
            self.previous_cost = 0.0
            self.history.clear()

        snapshot = self.get_telemetry_snapshot()
        await self.broadcast({"type": "RESET_STATE", "data": snapshot})
        return self.get_status()

    async def update_config(self, mode: Optional[str] = None, speed: Optional[float] = None, strategy: Optional[str] = None):
        replan_needed = False
        async with self._lock:
            if mode:
                self.mode = mode
                if mode == "stress":
                    # Force stress cycle to begin at phase 0 immediately
                    self.step = 0
                elif mode == "normal":
                    self.failed_servers.clear()
                    self.congestion_spec = "none"
                    for reg, base in self.base_demands.items():
                        self.current_demands[reg] = float(base)
                replan_needed = True
            if speed:
                self.speed = max(0.5, min(10.0, speed))
            if strategy:
                self.strategy = strategy
                replan_needed = True

        if replan_needed:
            await self._step()

        await self.broadcast({"type": "CONFIG_CHANGE", "data": self.get_status()})
        return self.get_status()

    async def inject_event(self, event_type: str, target: str, value: float = 1.0):
        async with self._lock:
            if event_type in ("server_failure", "fail"):
                if target not in self.failed_servers:
                    self.failed_servers.append(target)
            elif event_type in ("server_recovery", "recover"):
                if target in self.failed_servers:
                    self.failed_servers.remove(target)
            elif event_type in ("traffic_spike", "spike"):
                if target in self.current_demands:
                    self.current_demands[target] *= value
            elif event_type == "congestion":
                self.congestion_spec = f"{target}:Pune Server:{value}"

        await self._step()
        return {"status": "injected", "event": event_type, "target": target, "value": value}

    async def apply_scenario(self, scenario_name: str, strategy: Optional[str] = None) -> Dict[str, Any]:
        """
        Executes a pre-configured operational scenario via C++, synchronizes continuous simulation state,
        records full decision trace into history, and broadcasts live telemetry.
        """
        strat = strategy or self.strategy
        routing_result = await asyncio.to_thread(
            cpp_engine.run_scenario,
            scenario_name,
            strat
        )

        async with self._lock:
            self.strategy = strat
            self.step += 1
            self.sim_time_sec += 1

            demands = routing_result.get("regionalDemands", {})
            if demands:
                self.current_demands = {k: float(v) for k, v in demands.items()}
            self.failed_servers = list(routing_result.get("failedServers", []))

            congested = routing_result.get("congestedLinks", [])
            self.congestion_spec = parse_congested_links(congested)
            if scenario_name == "pune_congestion" and self.congestion_spec == "none":
                self.congestion_spec = "Pune:Pune Server:35.0"

            if scenario_name == "cache_improvement":
                self.cache_hit_ratio = 0.98
            else:
                self.cache_hit_ratio = None

            decision = f"SCENARIO_{scenario_name.upper()}"
            self.last_decision_reason = decision

            current_cost = routing_result.get("totalRoutingCost", 0.0)
            self.previous_cost = current_cost
            self.last_demands = dict(self.current_demands)
            self.last_failed_servers = list(self.failed_servers)
            self.last_congestion = self.congestion_spec
            self.last_routing_result = routing_result

            candidates = self.compute_candidates_matrix(
                self.current_demands,
                self.failed_servers,
                self.congestion_spec,
                routing_result
            )

            snapshot = {
                "tick": self.step,
                "sim_time_sec": self.sim_time_sec,
                "timestamp": time.time(),
                "strategy": self.strategy,
                "mode": self.mode,
                "demands": dict(self.current_demands),
                "total_demand": sum(self.current_demands.values()),
                "failed_servers": list(self.failed_servers),
                "congestion_spec": self.congestion_spec,
                "cache_hit_ratio": self.cache_hit_ratio,
                "decision_reason": decision,
                "previous_allocation": [],
                "new_allocation": routing_result.get("allocations", []),
                "routing": routing_result,
                "candidates_by_region": candidates,
                "serverLoads": routing_result.get("serverLoads", {}),
                "serverUtilPct": routing_result.get("serverUtilPct", {}),
                "linkLoads": routing_result.get("linkLoads", {}),
                "linkUtilPct": routing_result.get("linkUtilPct", {}),
                "origin_spillover": routing_result.get("originTrafficRouted", 0.0),
                "total_cost": routing_result.get("totalRoutingCost", 0.0),
                "average_latency": routing_result.get("averageLatencyMs", 0.0),
                "average_cost_per_req": routing_result.get("averageRoutingCostPerReq", 0.0),
            }
            self.history.append(snapshot)

        tick_payload = {
            "type": "LIVE_TICK",
            "data": {
                "step": self.step,
                "sim_time_sec": self.sim_time_sec,
                "status": self.status,
                "mode": self.mode,
                "strategy": self.strategy,
                "demands": dict(self.current_demands),
                "total_demand": sum(self.current_demands.values()),
                "total_routed": routing_result.get("totalTrafficRouted", 0.0),
                "origin_spillover": routing_result.get("originTrafficRouted", 0.0),
                "unserved_demand": routing_result.get("unservedTraffic", 0.0),
                "total_cost": routing_result.get("totalRoutingCost", 0.0),
                "cost_delta": 0.0,
                "average_latency": routing_result.get("averageLatencyMs", 0.0),
                "solve_time_us": routing_result.get("executionTimeMicroseconds", 0),
                "decision_reason": decision,
                "allocations": routing_result.get("allocations", []),
                "server_loads": routing_result.get("serverLoads", {}),
                "server_util_pct": routing_result.get("serverUtilPct", {}),
                "link_loads": routing_result.get("linkLoads", {}),
                "link_util_pct": routing_result.get("linkUtilPct", {}),
                "failed_servers": list(self.failed_servers),
                "congestion_spec": self.congestion_spec,
            }
        }
        await self.broadcast(tick_payload)
        return routing_result

    async def inject_and_run(
        self,
        event_type: str,
        target: str,
        value: float,
        strategy: str,
        secondary_target: str = "Pune Server"
    ) -> Dict[str, Any]:
        """
        Executes a dynamic simulation event on C++, updates continuous simulation state & history,
        and broadcasts live telemetry to clients.
        """
        routing_result = await asyncio.to_thread(
            cpp_engine.run_simulation,
            event_type,
            target,
            value,
            strategy,
            secondary_target
        )

        async with self._lock:
            self.strategy = strategy
            self.step += 1
            self.sim_time_sec += 1

            demands = routing_result.get("regionalDemands", {})
            if demands:
                self.current_demands = {k: float(v) for k, v in demands.items()}
            self.failed_servers = list(routing_result.get("failedServers", []))
            congested = routing_result.get("congestedLinks", [])
            self.congestion_spec = parse_congested_links(congested)

            decision = f"INJECT_{event_type.upper()}_{target.upper()}"
            self.last_decision_reason = decision

            current_cost = routing_result.get("totalRoutingCost", 0.0)
            self.previous_cost = current_cost
            self.last_demands = dict(self.current_demands)
            self.last_failed_servers = list(self.failed_servers)
            self.last_congestion = self.congestion_spec
            self.last_routing_result = routing_result

            candidates = self.compute_candidates_matrix(
                self.current_demands,
                self.failed_servers,
                self.congestion_spec,
                routing_result
            )

            snapshot = {
                "tick": self.step,
                "sim_time_sec": self.sim_time_sec,
                "timestamp": time.time(),
                "strategy": self.strategy,
                "mode": self.mode,
                "demands": dict(self.current_demands),
                "total_demand": sum(self.current_demands.values()),
                "failed_servers": list(self.failed_servers),
                "congestion_spec": self.congestion_spec,
                "cache_hit_ratio": self.cache_hit_ratio,
                "decision_reason": decision,
                "previous_allocation": [],
                "new_allocation": routing_result.get("allocations", []),
                "routing": routing_result,
                "candidates_by_region": candidates,
                "serverLoads": routing_result.get("serverLoads", {}),
                "serverUtilPct": routing_result.get("serverUtilPct", {}),
                "linkLoads": routing_result.get("linkLoads", {}),
                "linkUtilPct": routing_result.get("linkUtilPct", {}),
                "origin_spillover": routing_result.get("originTrafficRouted", 0.0),
                "total_cost": routing_result.get("totalRoutingCost", 0.0),
                "average_latency": routing_result.get("averageLatencyMs", 0.0),
                "average_cost_per_req": routing_result.get("averageRoutingCostPerReq", 0.0),
            }
            self.history.append(snapshot)

        tick_payload = {
            "type": "LIVE_TICK",
            "data": {
                "step": self.step,
                "sim_time_sec": self.sim_time_sec,
                "status": self.status,
                "mode": self.mode,
                "strategy": self.strategy,
                "demands": dict(self.current_demands),
                "total_demand": sum(self.current_demands.values()),
                "total_routed": routing_result.get("totalTrafficRouted", 0.0),
                "origin_spillover": routing_result.get("originTrafficRouted", 0.0),
                "unserved_demand": routing_result.get("unservedTraffic", 0.0),
                "total_cost": routing_result.get("totalRoutingCost", 0.0),
                "cost_delta": 0.0,
                "average_latency": routing_result.get("averageLatencyMs", 0.0),
                "solve_time_us": routing_result.get("executionTimeMicroseconds", 0),
                "decision_reason": decision,
                "allocations": routing_result.get("allocations", []),
                "server_loads": routing_result.get("serverLoads", {}),
                "server_util_pct": routing_result.get("serverUtilPct", {}),
                "link_loads": routing_result.get("linkLoads", {}),
                "link_util_pct": routing_result.get("linkUtilPct", {}),
                "failed_servers": list(self.failed_servers),
                "congestion_spec": self.congestion_spec,
            }
        }
        await self.broadcast(tick_payload)
        return routing_result

    def compute_candidates_matrix(
        self,
        demands: Dict[str, float],
        failed_servers: List[str],
        congestion_spec: str,
        routing_result: Dict[str, Any]
    ) -> Dict[str, List[Dict[str, Any]]]:
        """
        Evaluates every configured destination (4 Edge Servers + Origin Fallback)
        for every configured region (Pune, Mumbai, Delhi, Bangalore, Hyderabad).
        Produces comprehensive network & server metrics, hard constraint eligibility,
        mathematical cost formula breakdowns, and final flow allocations.
        """
        result = {}
        allocations = routing_result.get("allocations", []) if routing_result else []
        server_loads = routing_result.get("serverLoads", {}) if routing_result else {}
        server_util_pct = routing_result.get("serverUtilPct", {}) if routing_result else {}

        # Parse congestion spec if present (format: "region:server:delayMs")
        cong_map = {}
        if congestion_spec and congestion_spec != "none":
            parts = congestion_spec.split(":")
            if len(parts) == 3:
                cong_map[(parts[0], parts[1])] = float(parts[2])

        server_order = ["Pune Server", "Mumbai Server", "Delhi Server", "Bangalore Server", "Origin Server"]

        for region in ["Pune", "Mumbai", "Delhi", "Bangalore", "Hyderabad"]:
            reg_demand = demands.get(region, self.base_demands.get(region, 0.0))
            candidate_list = []

            for server in server_order:
                srv_info = SERVERS_SPEC[server]
                is_origin = srv_info["is_origin"]
                link_key = (region, server)
                link_info = LINKS_SPEC.get(link_key, {
                    "base_latency": 100.0,
                    "cost": 5.0,
                    "bw_capacity": 99999.0,
                    "hit_ratio": 0.0
                })

                # Server metrics
                server_healthy = (server not in failed_servers)
                server_total_cap = srv_info["capacity"]
                server_load = server_loads.get(server, 0.0)
                server_util = server_util_pct.get(server, 0.0)
                if not server_healthy:
                    server_remaining = 0.0
                elif is_origin:
                    server_remaining = 999999.0
                else:
                    server_remaining = max(0.0, server_total_cap - server_load)

                # Link metrics
                cong_delay = cong_map.get(link_key, 0.0)
                eff_latency = link_info["base_latency"] + cong_delay
                bw_cap = link_info["bw_capacity"]

                # Find allocated flow from this region to this server
                flow_allocated = 0.0
                for a in allocations:
                    if a.get("from") == region and a.get("to") == server:
                        flow_allocated += a.get("flow", 0.0)

                remaining_bw = max(0.0, bw_cap - flow_allocated)
                link_util = (flow_allocated / bw_cap * 100.0) if bw_cap > 0 else 0.0

                # Multi-factor cost formula: C = alpha * L + beta * B + (1 - H) * M
                alpha = 1.0
                beta = 1.5
                miss_penalty = 20.0
                hit_ratio = link_info["hit_ratio"]

                latency_term = round(alpha * eff_latency, 2)
                bandwidth_term = round(beta * link_info["cost"], 2)
                cache_miss_term = round((1.0 - hit_ratio) * miss_penalty, 2)

                if is_origin:
                    unit_cost = 177.50  # 100ms * 1.0 + 5.0 * 1.5 + 20.0 miss + 50 compute penalty
                    scaled_cost = 17750
                    formula_str = "C_origin = 1.0*(100.0) + 1.5*(5.0) + 20.0 [miss] + 50.0 [compute] = 177.50"
                else:
                    unit_cost = round(latency_term + bandwidth_term + cache_miss_term, 2)
                    scaled_cost = int(round(unit_cost * 100))
                    formula_str = f"C = {alpha}*({eff_latency:.1f}) + {beta}*({link_info['cost']:.1f}) + (1-{hit_ratio:.2f})*20 = {unit_cost:.2f}"

                # Hard Constraints & Eligibility
                if not server_healthy:
                    eligibility = "EXCLUDED_OUTAGE"
                    exclusion_reason = "Server offline due to hardware outage"
                elif not is_origin and server_remaining <= 0.001:
                    eligibility = "EXCLUDED_CAPACITY"
                    exclusion_reason = "Server processing capacity completely saturated"
                elif remaining_bw <= 0.001:
                    eligibility = "EXCLUDED_BANDWIDTH"
                    exclusion_reason = "Link transit bandwidth capacity exhausted"
                elif is_origin:
                    eligibility = "ORIGIN_FALLBACK"
                    exclusion_reason = "Origin fallback (activated when edge capacity is exhausted)"
                else:
                    eligibility = "ELIGIBLE"
                    exclusion_reason = "Candidate route eligible (healthy link and available server capacity)"

                alloc_pct = round((flow_allocated / reg_demand * 100.0), 1) if reg_demand > 0 else 0.0
                total_cost = round(flow_allocated * unit_cost, 2)

                candidate_record = {
                    "region": region,
                    "server": server,
                    "is_origin": is_origin,
                    "server_healthy": server_healthy,
                    "server_total_capacity": server_total_cap,
                    "server_existing_load": round(server_load, 1),
                    "server_remaining_capacity": round(server_remaining, 1),
                    "server_utilization_pct": round(server_util, 1),
                    "link_base_latency_ms": link_info["base_latency"],
                    "link_congestion_ms": cong_delay,
                    "link_effective_latency_ms": eff_latency,
                    "link_bandwidth_cost": link_info["cost"],
                    "link_bandwidth_capacity": bw_cap,
                    "link_allocated_flow": round(flow_allocated, 1),
                    "link_remaining_bandwidth": round(remaining_bw, 1),
                    "link_utilization_pct": round(link_util, 1),
                    "cache_hit_ratio": hit_ratio,
                    "cost_formula": {
                        "alpha": alpha,
                        "beta": beta,
                        "miss_penalty": miss_penalty,
                        "effective_latency": eff_latency,
                        "latency_term": latency_term,
                        "bandwidth_rate": link_info["cost"],
                        "bandwidth_term": bandwidth_term,
                        "cache_hit_ratio": hit_ratio,
                        "cache_miss_term": cache_miss_term,
                        "unit_cost": unit_cost,
                        "scaled_cost": scaled_cost,
                        "formula_string": formula_str
                    },
                    "eligibility": eligibility,
                    "exclusion_reason": exclusion_reason,
                    "flow_allocated": round(flow_allocated, 1),
                    "allocation_pct": alloc_pct,
                    "total_cost": total_cost,
                }
                candidate_list.append(candidate_record)

            result[region] = candidate_list

        return result

    def _generate_traffic(self):
        """Generates realistic regional traffic with diurnal waves and mode-specific fluctuations."""
        self.step += 1
        self.sim_time_sec += 1

        # Diurnal wave (smooth 120-second period)
        wave = 0.12 * math.sin(self.sim_time_sec / 120.0 * 2.0 * math.pi)

        if self.mode == "normal":
            # Normal Mode: Small realistic variations (+/- 4% to 8%)
            for reg, base in self.base_demands.items():
                noise = self.rng.uniform(-0.06, 0.06)
                trend = wave
                new_demand = base * (1.0 + trend + noise)
                # Occasional minor blip every ~30 steps
                if self.step % 30 == 0 and reg == "Mumbai":
                    new_demand *= 1.15
                self.current_demands[reg] = round(max(500.0, new_demand))
            
            # Normal mode: Preserves any manually injected events (outages/congestion)
            pass

        else:
            # Stress Mode: Active stress conditions, high volatility (+/- 15% to 30%), periodic outages and congestion
            for reg, base in self.base_demands.items():
                noise = self.rng.uniform(-0.15, 0.30)
                new_demand = base * (1.0 + wave + noise)
                self.current_demands[reg] = round(max(500.0, new_demand))

            phase = self.step % 80

            # Phase 1 (Step 0-19): Pune link transit congestion (+35 ms latency penalty)
            # Pune effective latency jumps to 40.0ms ($44.50/req). Mumbai Server is 12.0ms ($17.80/req).
            # Demonstrates dynamic cost rerouting from Pune Server to Mumbai Server!
            if 0 <= phase < 20:
                self.congestion_spec = "Pune:Pune Server:35.0"
                self.current_demands["Pune"] = round(self.base_demands["Pune"] * 1.3)  # ~3,250 req/s
                if "Delhi Server" in self.failed_servers:
                    self.failed_servers.remove("Delhi Server")

            # Phase 2 (Step 20-39): Massive Regional Demand Surge on Pune (5,500 req/s)
            # Exceeds Pune link bandwidth (4,000 req/s). Demonstrates multi-POP split allocation (Pune + Mumbai)!
            elif 20 <= phase < 40:
                self.congestion_spec = "none"
                self.current_demands["Pune"] = 5500.0
                if "Delhi Server" in self.failed_servers:
                    self.failed_servers.remove("Delhi Server")

            # Phase 3 (Step 40-59): Delhi Server Hardware Outage
            # Demonstrates automatic failover to neighboring POPs (Mumbai & Bangalore)!
            elif 40 <= phase < 60:
                self.congestion_spec = "none"
                if "Delhi Server" not in self.failed_servers:
                    self.failed_servers.append("Delhi Server")
                self.current_demands["Delhi"] = round(self.base_demands["Delhi"] * 1.4)  # ~2,800 req/s

            # Phase 4 (Step 60-79): Global Fleet Overcapacity & Origin DC Spillover
            # Total demand exceeds total edge capacity (20,500 req/s). Demonstrates tier-2 fallback!
            else:
                self.congestion_spec = "none"
                if "Delhi Server" in self.failed_servers:
                    self.failed_servers.remove("Delhi Server")
                for reg in self.current_demands:
                    self.current_demands[reg] = round(self.base_demands[reg] * 1.9)  # ~22,800 req/s total

    def _evaluate_replanning_policy(self) -> str:
        """
        Intelligent Replanning Decision Policy:
        Avoids unnecessary route churning for minor fluctuations while reacting
        instantly to hardware outages, link congestion, or demand surges.
        """
        # 1. Immediate trigger: Server failure transition
        for s in self.failed_servers:
            if s not in self.last_failed_servers:
                return "TRIGGER_SERVER_FAILURE"

        # 2. Immediate trigger: Server recovery transition
        for s in self.last_failed_servers:
            if s not in self.failed_servers:
                return "TRIGGER_SERVER_RECOVERY"

        # 3. Congestion change trigger
        if self.congestion_spec != self.last_congestion:
            return "TRIGGER_CONGESTION"

        # 4. Regional demand threshold trigger (Delta >= 8% on any region)
        for reg, dem in self.current_demands.items():
            last_dem = self.last_demands.get(reg, dem)
            if last_dem > 0:
                delta_pct = abs(dem - last_dem) / last_dem
                if delta_pct >= 0.08:
                    return "TRIGGER_TRAFFIC_SPIKE"

        # 5. Total demand threshold trigger (Delta >= 6%)
        total_curr = sum(self.current_demands.values())
        total_last = sum(self.last_demands.values())
        if total_last > 0 and (abs(total_curr - total_last) / total_last) >= 0.06:
            return "TRIGGER_TRAFFIC_SPIKE"

        # 6. Check if previous allocation is missing
        if self.last_routing_result is None:
            return "INITIAL_PLAN"

        # Fluctuations within tolerance -> keep routing stable
        return "UNCHANGED"

    async def _step(self):
        """Executes one continuous simulation cycle and broadcasts to clients."""
        # 1. Generate new incoming traffic
        self._generate_traffic()

        # 2. Check intelligent replanning policy
        decision = self._evaluate_replanning_policy()
        self.last_decision_reason = decision

        # 3. Execute C++ routing engine if replan needed or periodically for exact metrics
        # Always run C++ to ensure exact optimal allocation numbers and execution timing
        routing_result = await asyncio.to_thread(
            cpp_engine.run_dynamic,
            self.strategy,
            self.current_demands,
            self.failed_servers,
            self.congestion_spec,
            self.cache_hit_ratio
        )

        current_cost = routing_result.get("totalRoutingCost", 0.0)
        cost_delta = current_cost - self.previous_cost if self.previous_cost > 0 else 0.0
        self.previous_cost = current_cost

        # Update last known state if replanned
        if decision != "UNCHANGED":
            self.last_demands = dict(self.current_demands)
            self.last_failed_servers = list(self.failed_servers)
            self.last_congestion = self.congestion_spec

        self.last_routing_result = routing_result

        # 4. Compute comprehensive candidates matrix for all 5 regions
        candidates = self.compute_candidates_matrix(
            self.current_demands,
            self.failed_servers,
            self.congestion_spec,
            routing_result
        )

        # 5. Record snapshot into bounded history for Inspection and Replay
        snapshot = {
            "tick": self.step,
            "sim_time_sec": self.sim_time_sec,
            "timestamp": time.time(),
            "strategy": self.strategy,
            "mode": self.mode,
            "demands": dict(self.current_demands),
            "total_demand": sum(self.current_demands.values()),
            "failed_servers": list(self.failed_servers),
            "congestion_spec": self.congestion_spec,
            "cache_hit_ratio": self.cache_hit_ratio,
            "decision_reason": decision,
            "previous_allocation": (self.last_demands if decision == "UNCHANGED" else []),
            "new_allocation": routing_result.get("allocations", []),
            "routing": routing_result,
            "candidates_by_region": candidates,
            "serverLoads": routing_result.get("serverLoads", {}),
            "serverUtilPct": routing_result.get("serverUtilPct", {}),
            "linkLoads": routing_result.get("linkLoads", {}),
            "linkUtilPct": routing_result.get("linkUtilPct", {}),
            "origin_spillover": routing_result.get("originTrafficRouted", 0.0),
            "total_cost": routing_result.get("totalRoutingCost", 0.0),
            "average_latency": routing_result.get("averageLatencyMs", 0.0),
            "average_cost_per_req": routing_result.get("averageRoutingCostPerReq", 0.0),
        }
        self.history.append(snapshot)

        # 6. Construct live telemetry tick payload
        tick_payload = {
            "type": "LIVE_TICK",
            "data": {
                "step": self.step,
                "sim_time_sec": self.sim_time_sec,
                "status": self.status,
                "mode": self.mode,
                "speed": self.speed,
                "strategy": self.strategy,
                "decision_reason": decision,
                "demands": self.current_demands,
                "total_demand": sum(self.current_demands.values()),
                "failed_servers": self.failed_servers,
                "congestion_spec": self.congestion_spec,
                "previous_cost": self.previous_cost,
                "cost_delta": cost_delta,
                "routing": routing_result,
            }
        }

        # 6. Broadcast to all active WebSocket connections
        await self.broadcast(tick_payload)

    async def step_forward(self) -> Dict[str, Any]:
        """
        Advances the simulation by exactly one tick and pauses.
        Allows fine-grained step-by-step examination of live dynamic CDN behaviors.
        """
        async with self._lock:
            # If running, pause background loop
            if self.status == "running":
                self.status = "paused"
                if self._task and not self._task.done():
                    self._task.cancel()
                    self._task = None
            else:
                self.status = "paused"

            await self._step()

        await self.broadcast({"type": "STATUS_CHANGE", "status": self.status})
        return self.get_status()

    def get_history(self) -> List[Dict[str, Any]]:
        """Returns summarized recent decision history."""
        return [
            {
                "tick": s["tick"],
                "sim_time_sec": s["sim_time_sec"],
                "strategy": s["strategy"],
                "mode": s["mode"],
                "total_demand": s["total_demand"],
                "total_cost": s["total_cost"],
                "average_latency": s["average_latency"],
                "origin_spillover": s["origin_spillover"],
                "decision_reason": s["decision_reason"],
                "failed_servers": s["failed_servers"],
                "has_congestion": (s["congestion_spec"] != "none"),
            }
            for s in self.history
        ]

    async def get_execution_trace(self, tick: Optional[int] = None) -> Dict[str, Any]:
        """
        Retrieves full network snapshot and authentic C++ execution trace.
        Guarantees bit-for-bit deterministic replay from frozen snapshot.
        """
        snapshot = None
        if tick is not None:
            for s in self.history:
                if s["tick"] == tick:
                    snapshot = s
                    break
        elif self.history:
            snapshot = self.history[-1]

        if snapshot is None:
            # Construct snapshot from current state
            snapshot = {
                "tick": self.step,
                "sim_time_sec": self.sim_time_sec,
                "timestamp": time.time(),
                "strategy": self.strategy,
                "mode": self.mode,
                "demands": dict(self.current_demands),
                "total_demand": sum(self.current_demands.values()),
                "failed_servers": list(self.failed_servers),
                "congestion_spec": self.congestion_spec,
                "cache_hit_ratio": self.cache_hit_ratio,
                "decision_reason": self.last_decision_reason,
                "previous_allocation": [],
                "new_allocation": (self.last_routing_result.get("allocations", []) if self.last_routing_result else []),
                "routing": self.last_routing_result or {},
                "serverLoads": (self.last_routing_result.get("serverLoads", {}) if self.last_routing_result else {}),
                "serverUtilPct": (self.last_routing_result.get("serverUtilPct", {}) if self.last_routing_result else {}),
                "linkLoads": (self.last_routing_result.get("linkLoads", {}) if self.last_routing_result else {}),
                "linkUtilPct": (self.last_routing_result.get("linkUtilPct", {}) if self.last_routing_result else {}),
                "origin_spillover": (self.last_routing_result.get("originTrafficRouted", 0.0) if self.last_routing_result else 0.0),
                "total_cost": (self.last_routing_result.get("totalRoutingCost", 0.0) if self.last_routing_result else 0.0),
                "average_latency": (self.last_routing_result.get("averageLatencyMs", 0.0) if self.last_routing_result else 0.0),
                "average_cost_per_req": (self.last_routing_result.get("averageRoutingCostPerReq", 0.0) if self.last_routing_result else 0.0),
            }

        # Invoke C++ engine with record_trace=True to extract full genuine calculation trace
        trace_result = await asyncio.to_thread(
            cpp_engine.run_dynamic,
            snapshot["strategy"],
            snapshot["demands"],
            snapshot["failed_servers"],
            snapshot["congestion_spec"],
            snapshot["cache_hit_ratio"],
            record_trace=True,
        )

        # Ensure snapshot contains full candidates matrix for this trace
        snapshot["candidates_by_region"] = self.compute_candidates_matrix(
            snapshot["demands"],
            snapshot["failed_servers"],
            snapshot["congestion_spec"],
            trace_result
        )

        trace_data = trace_result.get("trace", {
            "strategy": snapshot["strategy"],
            "totalSteps": 0,
            "steps": [],
            "finalExplanation": trace_result.get("explanation", ""),
        })

        return {
            "snapshot": snapshot,
            "trace": trace_data,
            "routing": trace_result,
        }

    async def replay_snapshot(
        self,
        strategy: str,
        tick: Optional[int] = None,
        custom_demands: Optional[Dict[str, float]] = None,
        custom_failed: Optional[List[str]] = None,
        custom_congestion: Optional[str] = None,
    ) -> Dict[str, Any]:
        """
        Replays a snapshot or custom state with an alternate routing strategy,
        generating full comparative execution trace.
        """
        demands = custom_demands or self.current_demands
        failed = custom_failed if custom_failed is not None else self.failed_servers
        congestion = custom_congestion if custom_congestion is not None else self.congestion_spec

        if tick is not None:
            for s in self.history:
                if s["tick"] == tick:
                    demands = custom_demands or s["demands"]
                    failed = custom_failed if custom_failed is not None else s["failed_servers"]
                    congestion = custom_congestion if custom_congestion is not None else s["congestion_spec"]
                    break

        trace_result = await asyncio.to_thread(
            cpp_engine.run_dynamic,
            strategy,
            demands,
            failed,
            congestion,
            self.cache_hit_ratio,
            record_trace=True,
        )

        candidates = self.compute_candidates_matrix(
            demands,
            failed,
            congestion,
            trace_result
        )

        return {
            "tick": tick if tick is not None else self.step,
            "strategy": strategy,
            "demands": demands,
            "total_demand": sum(demands.values()),
            "failed_servers": failed,
            "congestion": congestion,
            "routing": trace_result,
            "trace": trace_result.get("trace", {}),
            "candidates_by_region": candidates,
        }

    async def _simulation_loop(self):
        """Asynchronous continuous background loop."""
        while self.status == "running":
            cycle_start = time.monotonic()
            try:
                await self._step()
            except Exception as e:
                print(f"[Simulation Error] Step {self.step}: {e}")

            # Sleep calculated dynamically based on speed multiplier
            cycle_duration = time.monotonic() - cycle_start
            sleep_time = max(0.01, (1.0 / self.speed) - cycle_duration)
            await asyncio.sleep(sleep_time)

    def get_status(self) -> Dict[str, Any]:
        return {
            "status": self.status,
            "mode": self.mode,
            "speed": self.speed,
            "strategy": self.strategy,
            "seed": self.seed,
            "step": self.step,
            "sim_time_sec": self.sim_time_sec,
            "failed_servers": self.failed_servers,
            "congestion": self.congestion_spec,
            "connected_clients": len(self._active_connections),
        }

    def get_telemetry_snapshot(self) -> Dict[str, Any]:
        return {
            "status": self.get_status(),
            "demands": self.current_demands,
            "last_decision": self.last_decision_reason,
            "last_routing": self.last_routing_result,
        }


# Global singleton instance
simulation_manager = ContinuousSimulationManager()

