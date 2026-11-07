from fastapi import APIRouter
from backend.models.requests import SimulationRequest
from backend.models.responses import RoutingResponse
from backend.services.simulation_manager import simulation_manager
from backend.services.validation import (
    validate_and_normalize_strategy,
    validate_simulation_event,
)

router = APIRouter(prefix="/api/simulation", tags=["Simulation"])


@router.post("/run", response_model=RoutingResponse)
async def execute_simulation(request: SimulationRequest):
    """
    Injects a dynamic simulation event (traffic spike, server failure, server recovery, link congestion)
    into the C++ engine and synchronizes continuous simulation telemetry and Inspector snapshot history.
    """
    event_type, target, value = validate_simulation_event(
        request.event_type, request.target, request.value
    )
    strategy = validate_and_normalize_strategy(request.strategy or "Min-Cost Max-Flow")
    secondary_target = request.secondary_target or "Pune Server"

    result = await simulation_manager.inject_and_run(
        event_type=event_type,
        target=target,
        value=value,
        strategy=strategy,
        secondary_target=secondary_target,
    )
    return result

