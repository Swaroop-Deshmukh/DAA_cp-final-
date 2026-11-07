from typing import List, Union
from fastapi import APIRouter
from backend.models.responses import BenchmarkScenarioResponse
from backend.services.cpp_engine import cpp_engine
from backend.services.validation import validate_and_normalize_scenario

router = APIRouter(prefix="/api/benchmarks", tags=["Benchmarks"])


@router.get("", response_model=List[BenchmarkScenarioResponse])
def get_all_benchmarks():
    """
    Returns comparative performance metrics for all 4 routing strategies
    (Round Robin, Nearest Server, Greedy Cost, Min-Cost Max-Flow) across all 8 operational scenarios.
    """
    raw_results = cpp_engine.get_benchmarks()
    return raw_results


@router.get("/{scenario_id}", response_model=BenchmarkScenarioResponse)
def get_scenario_benchmark(scenario_id: str):
    """
    Returns comparative benchmark results for a single specified scenario.
    """
    normalized_scenario = validate_and_normalize_scenario(scenario_id)
    raw_result = cpp_engine.get_benchmarks(scenario_id=normalized_scenario)
    return raw_result

