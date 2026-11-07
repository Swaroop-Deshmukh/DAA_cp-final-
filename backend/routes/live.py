"""
Live Real-Time Simulation REST & WebSocket Router
Manages interactive continuous simulation controls and telemetry streaming.
"""

from fastapi import APIRouter, WebSocket, WebSocketDisconnect
from backend.models.requests import (
    LiveStartRequest,
    LiveConfigRequest,
    LiveInjectRequest,
    InspectorReplayRequest,
)
from backend.services.simulation_manager import simulation_manager
from backend.services.validation import validate_and_normalize_strategy

router = APIRouter(tags=["Live Simulation"])


@router.post("/api/live/start")
async def start_live_simulation(request: LiveStartRequest):
    """Starts the real-time continuous traffic simulation loop."""
    strategy = validate_and_normalize_strategy(request.strategy)
    return await simulation_manager.start(
        mode=request.mode,
        speed=request.speed,
        strategy=strategy,
        seed=request.seed,
    )


@router.post("/api/live/pause")
async def pause_live_simulation():
    """Pauses the running simulation loop."""
    return await simulation_manager.pause()


@router.post("/api/live/resume")
async def resume_live_simulation():
    """Resumes the paused simulation loop."""
    return await simulation_manager.resume()


@router.post("/api/live/reset")
async def reset_live_simulation():
    """Stops and resets the simulation state to initial baseline conditions."""
    return await simulation_manager.reset()


@router.post("/api/live/config")
async def update_live_config(request: LiveConfigRequest):
    """Dynamically updates simulation configuration (mode, speed, algorithm) without restarting."""
    strat = validate_and_normalize_strategy(request.strategy) if request.strategy else None
    return await simulation_manager.update_config(
        mode=request.mode,
        speed=request.speed,
        strategy=strat,
    )


@router.post("/api/live/inject")
async def inject_live_event(request: LiveInjectRequest):
    """Manually injects a real-time event (failure, recovery, spike, congestion) into the live loop."""
    return await simulation_manager.inject_event(
        event_type=request.event_type,
        target=request.target,
        value=request.value,
    )


@router.post("/api/live/step")
async def step_live_simulation():
    """Advances continuous simulation clock by exactly one tick and pauses."""
    return await simulation_manager.step_forward()


@router.get("/api/inspector/history")
def get_inspector_history():
    """Returns bounded history of recent routing decisions for timeline replay."""
    return simulation_manager.get_history()


@router.get("/api/inspector/snapshot/{tick}")
async def get_inspector_snapshot(tick: int):
    """Retrieves full snapshot and C++ step-by-step decision trace for a specific tick."""
    return await simulation_manager.get_execution_trace(tick)


@router.get("/api/inspector/latest")
async def get_inspector_latest():
    """Retrieves full snapshot and C++ decision trace for the latest available simulation tick."""
    return await simulation_manager.get_execution_trace(None)


@router.post("/api/inspector/replay")
async def replay_inspector_snapshot(request: InspectorReplayRequest):
    """Replays a historical or custom network snapshot with an alternate routing strategy."""
    strat = validate_and_normalize_strategy(request.strategy)
    return await simulation_manager.replay_snapshot(
        strategy=strat,
        tick=request.tick,
        custom_demands=request.demands,
        custom_failed=request.failed_servers,
        custom_congestion=request.congestion,
    )


@router.get("/api/live/status")
def get_live_status():
    """Returns current status and telemetry of the continuous simulation manager."""
    return simulation_manager.get_status()


@router.websocket("/ws/live")
async def live_websocket_endpoint(websocket: WebSocket):
    """
    Continuous real-time telemetry streaming channel.
    Streams tick packets containing regional demands, server loads, C++ routing allocations,
    latency, cost, and replanning decisions.
    """
    await simulation_manager.connect_client(websocket)
    try:
        while True:
            # Keep connection open and receive any incoming client control commands
            data = await websocket.receive_json()
            cmd = data.get("action")
            if cmd == "start":
                await simulation_manager.start(
                    mode=data.get("mode", "normal"),
                    speed=data.get("speed", 1.0),
                    strategy=data.get("strategy", "Min-Cost Max-Flow"),
                )
            elif cmd == "pause":
                await simulation_manager.pause()
            elif cmd == "resume":
                await simulation_manager.resume()
            elif cmd == "reset":
                await simulation_manager.reset()
            elif cmd == "set_speed":
                await simulation_manager.update_config(speed=float(data.get("speed", 1.0)))
            elif cmd == "set_strategy":
                strat = validate_and_normalize_strategy(data.get("strategy", "Min-Cost Max-Flow"))
                await simulation_manager.update_config(strategy=strat)
            elif cmd == "set_mode":
                await simulation_manager.update_config(mode=data.get("mode", "normal"))
    except WebSocketDisconnect:
        simulation_manager.disconnect_client(websocket)
    except Exception:
        simulation_manager.disconnect_client(websocket)

