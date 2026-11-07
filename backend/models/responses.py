from typing import List, Dict, Optional, Any
from pydantic import BaseModel, Field, ConfigDict


class HealthResponse(BaseModel):
    status: str
    engine: str
    executable: str
    available: bool


class RegionInfo(BaseModel):
    name: str
    trafficDemand: float


class ServerInfo(BaseModel):
    name: str
    configuredCapacity: float
    availableCapacity: float
    isActive: bool


class LinkInfo(BaseModel):
    regionName: str
    serverName: str
    baseLatencyMs: float
    congestionLatencyMs: float
    effectiveLatencyMs: float
    bandwidthCost: float
    bandwidthCapacity: float
    cacheHitRatio: float


class NetworkResponse(BaseModel):
    totalDemand: float
    totalActiveCapacity: float
    regions: List[RegionInfo]
    servers: List[ServerInfo]
    links: List[LinkInfo]


class MetricsResponse(BaseModel):
    totalRegions: int
    totalServers: int
    totalLinks: int
    totalDemand: float
    totalActiveCapacity: float
    fleetUtilization: float
    availableStrategies: List[str]
    status: str


class AllocationItem(BaseModel):
    from_node: str = Field(alias="from")
    to_node: str = Field(alias="to")
    flow: float
    capacity: float
    unitCost: float
    totalCost: float
    label: str

    model_config = ConfigDict(populate_by_name=True)


class RoutingResponse(BaseModel):
    strategyName: str
    scenarioName: str
    totalTrafficRequested: float
    edgeTrafficRouted: float
    originTrafficRouted: float
    totalTrafficRouted: float
    unservedTraffic: float
    totalRoutingCost: float
    averageRoutingCostPerReq: float
    averageLatencyMs: float
    executionTimeMicroseconds: int
    serverLoads: Dict[str, float]
    serverUtilPct: Dict[str, float]
    linkLoads: Dict[str, float]
    linkUtilPct: Dict[str, float]
    regionalDemands: Dict[str, float]
    failedServers: List[str]
    congestedLinks: List[str]
    allocations: List[AllocationItem]
    explanation: str


class BenchmarkStrategyItem(BaseModel):
    strategyName: str
    totalTrafficRequested: float
    edgeTrafficRouted: float
    originTrafficRouted: float
    totalTrafficRouted: float
    unservedTraffic: float
    totalRoutingCost: float
    averageRoutingCostPerReq: float
    averageLatencyMs: float
    executionTimeMicroseconds: int
    serverUtilPct: Dict[str, float]
    serverLoads: Dict[str, float]


class BenchmarkScenarioResponse(BaseModel):
    scenario: str
    strategies: List[BenchmarkStrategyItem]
