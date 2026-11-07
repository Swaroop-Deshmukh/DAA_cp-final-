from typing import Tuple
from fastapi import HTTPException

VALID_STRATEGIES = {
    "min-cost max-flow": "Min-Cost Max-Flow",
    "mcmf": "Min-Cost Max-Flow",
    "round robin": "Round Robin",
    "round_robin": "Round Robin",
    "rr": "Round Robin",
    "nearest server": "Nearest Server",
    "nearest_server": "Nearest Server",
    "ns": "Nearest Server",
    "greedy cost-based": "Greedy Cost",
    "greedy cost": "Greedy Cost",
    "greedy_cost": "Greedy Cost",
    "gc": "Greedy Cost",
    "edmonds-karp": "Edmonds-Karp",
    "edmonds_karp": "Edmonds-Karp",
    "edmonds karp": "Edmonds-Karp",
    "ek": "Edmonds-Karp",
}

VALID_SCENARIOS = {
    "normal": "normal",
    "normal traffic": "normal",
    "mumbai_spike": "mumbai_spike",
    "mumbai traffic spike": "mumbai_spike",
    "delhi_failure": "delhi_failure",
    "delhi server failure": "delhi_failure",
    "delhi_offline": "delhi_failure",
    "delhi offline": "delhi_failure",
    "delhi_recovery": "delhi_recovery",
    "delhi server recovery": "delhi_recovery",
    "pune_congestion": "pune_congestion",
    "pune congestion": "pune_congestion",
    "over_capacity": "over_capacity",
    "overcapacity": "over_capacity",
    "over-capacity traffic": "over_capacity",
    "traffic exceeding edge capacity": "over_capacity",
    "all_servers_failed": "all_servers_failed",
    "all_edge_down": "all_servers_failed",
    "all edge servers offline": "all_servers_failed",
    "cache_improvement": "cache_improvement",
    "cache_boost": "cache_improvement",
    "cache-hit improvement": "cache_improvement",
}

VALID_EVENT_TYPES = {
    "traffic_spike": "traffic_spike",
    "spike": "traffic_spike",
    "server_failure": "server_failure",
    "failure": "server_failure",
    "fail": "server_failure",
    "server_recovery": "server_recovery",
    "recovery": "server_recovery",
    "recover": "server_recovery",
    "congestion": "congestion",
}


def validate_and_normalize_strategy(strategy: str) -> str:
    key = strategy.strip().lower()
    if key not in VALID_STRATEGIES:
        raise HTTPException(
            status_code=400,
            detail=f"Invalid strategy '{strategy}'. Supported strategies: {list(set(VALID_STRATEGIES.values()))}",
        )
    return VALID_STRATEGIES[key]


def validate_and_normalize_scenario(scenario: str) -> str:
    key = scenario.strip().lower()
    if key not in VALID_SCENARIOS:
        raise HTTPException(
            status_code=400,
            detail=f"Invalid scenario '{scenario}'. Supported scenarios: {list(set(VALID_SCENARIOS.values()))}",
        )
    return VALID_SCENARIOS[key]


def validate_simulation_event(
    event_type: str, target: str, value: float
) -> Tuple[str, str, float]:
    key = event_type.strip().lower()
    if key not in VALID_EVENT_TYPES:
        raise HTTPException(
            status_code=400,
            detail=f"Invalid event type '{event_type}'. Supported: {list(set(VALID_EVENT_TYPES.values()))}",
        )
    normalized_event = VALID_EVENT_TYPES[key]

    if not target or not target.strip():
        raise HTTPException(
            status_code=400, detail="Target entity cannot be empty."
        )

    if normalized_event == "traffic_spike":
        if value <= 0.0 or value > 10.0:
            raise HTTPException(
                status_code=400,
                detail="Traffic spike multiplier must be between 0.1 and 10.0.",
            )
    elif normalized_event == "congestion":
        if value < 0.0 or value > 500.0:
            raise HTTPException(
                status_code=400,
                detail="Congestion delay must be between 0.0 and 500.0 ms.",
            )

    return normalized_event, target.strip(), value

