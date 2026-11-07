from fastapi import APIRouter
from backend.models.requests import RoutingRequest
from backend.models.responses import RoutingResponse
from backend.services.cpp_engine import cpp_engine
from backend.services.validation import (
    validate_and_normalize_strategy,
    validate_and_normalize_scenario,
)

router = APIRouter(prefix="/api/routing", tags=["Routing"])


@router.post("/run", response_model=RoutingResponse)
def execute_routing(request: RoutingRequest):
    """
    Executes traffic routing on the C++ engine using the specified routing strategy.
    Supports Min-Cost Max-Flow, Round Robin, Nearest Server, and Greedy Cost-Based.
    """
    normalized_strategy = validate_and_normalize_strategy(request.strategy)
    normalized_scenario = validate_and_normalize_scenario(request.scenario or "normal")

    raw_result = cpp_engine.run_routing(
        strategy=normalized_strategy, scenario=normalized_scenario
    )
    return raw_result

