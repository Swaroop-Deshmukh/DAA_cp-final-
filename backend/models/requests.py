from typing import Optional
from pydantic import BaseModel, Field


class RoutingRequest(BaseModel):
    strategy: str = Field(
        default="Min-Cost Max-Flow",
        description="Routing algorithm: 'Min-Cost Max-Flow', 'Round Robin', 'Nearest Server', or 'Greedy Cost-Based'",
    )
    scenario: Optional[str] = Field(
        default="normal",
        description="Operational scenario name (e.g. 'normal', 'mumbai_spike', 'delhi_failure', etc.)",
    )


class SimulationRequest(BaseModel):
    event_type: str = Field(
        description="Type of network event: 'traffic_spike', 'server_failure', 'server_recovery', 'congestion'",
    )
    target: str = Field(
        description="Target node/link name (e.g., 'Pune', 'Delhi Server')",
    )
    value: float = Field(
        default=1.5,
        description="Multiplier for spike (e.g., 2.0) or added delay for congestion (e.g., 35.0 ms)",
    )
    strategy: Optional[str] = Field(
        default="Min-Cost Max-Flow",
        description="Routing algorithm to run after simulation event is injected",
    )
    secondary_target: Optional[str] = Field(
        default="Pune Server",
        description="Destination server for link congestion events",
    )


class ScenarioRequest(BaseModel):
    scenario: str = Field(
        description="Preset scenario key: 'normal', 'mumbai_spike', 'delhi_failure', 'delhi_recovery', 'pune_congestion', 'over_capacity', 'all_servers_failed', 'cache_improvement'",
    )
    strategy: Optional[str] = Field(
        default="Min-Cost Max-Flow",
        description="Routing strategy to evaluate against the scenario",
    )



class LiveStartRequest(BaseModel):
    mode: str = Field(
        default="normal",
        description="Simulation mode: 'normal' (smooth realistic) or 'stress' (volatility & outages)",
    )
    speed: float = Field(
        default=1.0,
        description="Speed multiplier: 1.0 (1 cycle/s), 2.0 (2 cycles/s), 5.0 (5 cycles/s)",
    )
    strategy: str = Field(
        default="Min-Cost Max-Flow",
        description="Routing strategy algorithm to execute",
    )
    seed: int = Field(
        default=42,
        description="Random seed for deterministic reproducible traffic generation",
    )


class LiveConfigRequest(BaseModel):
    mode: Optional[str] = Field(default=None, description="Updated mode")
    speed: Optional[float] = Field(default=None, description="Updated speed multiplier")
    strategy: Optional[str] = Field(default=None, description="Updated routing algorithm")


class LiveInjectRequest(BaseModel):
    event_type: str = Field(description="Event type: 'server_failure', 'server_recovery', 'traffic_spike', 'congestion'")
    target: str = Field(description="Target server or region (e.g. 'Delhi Server', 'Mumbai')")
    value: float = Field(default=1.5, description="Multiplier for spike or added latency for congestion")


class InspectorReplayRequest(BaseModel):
    tick: Optional[int] = Field(default=None, description="Recorded tick to replay")
    strategy: str = Field(default="Min-Cost Max-Flow", description="Strategy to evaluate snapshot with")
    demands: Optional[dict] = Field(default=None, description="Optional custom demands")
    failed_servers: Optional[list] = Field(default=None, description="Optional failed servers list")
    congestion: Optional[str] = Field(default=None, description="Optional congestion specification")

