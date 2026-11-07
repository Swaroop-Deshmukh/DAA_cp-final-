from fastapi import APIRouter
from backend.models.requests import ScenarioRequest
from backend.models.responses import RoutingResponse
from backend.services.simulation_manager import simulation_manager
from backend.services.validation import (
    validate_and_normalize_strategy,
    validate_and_normalize_scenario,
)

router = APIRouter(prefix="/api/scenarios", tags=["Scenarios"])


@router.post("/run", response_model=RoutingResponse)
async def execute_scenario(request: ScenarioRequest):
    """
    Evaluates a pre-configured operational scenario on the C++ engine and
    synchronizes continuous simulation manager telemetry and Inspector snapshot history.
    Supported scenarios:
    - normal: Standard baseline (12,000 req/s, 4 edge servers online)
    - mumbai_spike: Regional spike (3,500 -> 7,000 req/s)
    - delhi_failure: Delhi Server offline (0 available capacity)
    - delhi_recovery: Delhi Server back online
    - pune_congestion: Pune link congestion (+35 ms latency)
    - over_capacity: Traffic demand exceeding edge capacity (24,000 req/s)
    - all_servers_failed: Catastrophic outage (100% Origin fallback)
    - cache_improvement: High cache hit ratio optimization (0.98 / 0.99)
    """
    normalized_scenario = validate_and_normalize_scenario(request.scenario)
    normalized_strategy = validate_and_normalize_strategy(
        request.strategy or "Min-Cost Max-Flow"
    )

    result = await simulation_manager.apply_scenario(
        scenario_name=normalized_scenario, strategy=normalized_strategy
    )
    return result

