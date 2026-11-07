import os
import json
import subprocess
from pathlib import Path
from typing import Dict, Any, List, Optional
from fastapi import HTTPException


class CppEngineService:
    def __init__(self):
        self.executable_path = self._find_executable()

    def _find_executable(self) -> Path:
        base_dir = Path(__file__).resolve().parent.parent.parent
        candidates = [
            base_dir / "cpp-engine" / "build" / "Release" / "cdn_router.exe",
            base_dir / "cpp-engine" / "build" / "cdn_router.exe",
            base_dir / "cpp-engine" / "build" / "Debug" / "cdn_router.exe",
            base_dir / "cpp-engine" / "build" / "Release" / "cdn_router",
            base_dir / "cpp-engine" / "build" / "cdn_router",
        ]
        for c in candidates:
            if c.exists() and os.access(c, os.X_OK):
                return c
        # If not yet found, return primary candidate path so error explains clearly
        return candidates[0]

    def is_available(self) -> bool:
        return self.executable_path.exists() and os.access(
            self.executable_path, os.X_OK
        )

    def execute_api_command(self, *args: str) -> Any:
        if not self.is_available():
            raise HTTPException(
                status_code=500,
                detail=f"C++ engine executable not found at '{self.executable_path}'. Please compile with CMake first.",
            )

        cmd = [str(self.executable_path), "--api", *args]

        try:
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=10.0,
                shell=False,
            )
        except subprocess.TimeoutExpired:
            raise HTTPException(
                status_code=504,
                detail="C++ engine processing timed out after 10.0 seconds.",
            )
        except FileNotFoundError:
            raise HTTPException(
                status_code=500,
                detail=f"C++ engine executable missing at '{self.executable_path}'.",
            )
        except Exception as ex:
            raise HTTPException(
                status_code=500,
                detail=f"Failed to execute C++ engine: {str(ex)}",
            )

        if result.returncode != 0:
            err_msg = (
                result.stderr.strip()
                if result.stderr
                else f"Exited with code {result.returncode}"
            )
            raise HTTPException(
                status_code=500, detail=f"C++ engine runtime error: {err_msg}"
            )

        stdout_clean = result.stdout.strip()
        if not stdout_clean:
            raise HTTPException(
                status_code=500,
                detail="C++ engine returned an empty output stream.",
            )

        # Robustly locate JSON payload boundary if any leading or trailing logs exist
        idx_obj = stdout_clean.find("{")
        idx_arr = stdout_clean.find("[")
        start_idx = -1
        end_idx = -1

        if idx_obj != -1 and (idx_arr == -1 or idx_obj < idx_arr):
            start_idx = idx_obj
            end_idx = stdout_clean.rfind("}") + 1
        elif idx_arr != -1:
            start_idx = idx_arr
            end_idx = stdout_clean.rfind("]") + 1

        json_str = (
            stdout_clean[start_idx:end_idx]
            if start_idx != -1 and end_idx > start_idx
            else stdout_clean
        )

        try:
            return json.loads(json_str)
        except json.JSONDecodeError as err:
            raise HTTPException(
                status_code=500,
                detail=f"Failed to parse C++ engine JSON response: {err.msg}. Raw output: {stdout_clean[:200]}",
            )

    def get_health(self) -> Dict[str, Any]:
        available = self.is_available()
        return {
            "status": "healthy" if available else "degraded",
            "engine": "C++17 Adaptive CDN Router Engine (SSP Dijkstra + Johnson)",
            "executable": str(self.executable_path),
            "available": available,
        }

    def get_network(self) -> Dict[str, Any]:
        return self.execute_api_command("get-network")

    def get_metrics(self) -> Dict[str, Any]:
        return self.execute_api_command("get-metrics")

    def run_routing(
        self, strategy: str = "Min-Cost Max-Flow", scenario: str = "normal"
    ) -> Dict[str, Any]:
        return self.execute_api_command("run-routing", strategy, scenario)

    def run_scenario(
        self, scenario: str, strategy: str = "Min-Cost Max-Flow"
    ) -> Dict[str, Any]:
        return self.execute_api_command("run-scenario", scenario, strategy)

    def run_simulation(
        self,
        event_type: str,
        target: str,
        value: float,
        strategy: str = "Min-Cost Max-Flow",
        secondary_target: str = "Pune Server",
    ) -> Dict[str, Any]:
        args = [
            "run-simulation",
            event_type,
            target,
            str(value),
            strategy,
            secondary_target,
        ]
        return self.execute_api_command(*args)

    def get_benchmarks(
        self, scenario_id: Optional[str] = None
    ) -> Any:
        if scenario_id:
            return self.execute_api_command("get-benchmarks", scenario_id)
        return self.execute_api_command("get-benchmarks")

    def get_cache(self, budget_mb: int = 350) -> Dict[str, Any]:
        return self.execute_api_command("get-cache", str(budget_mb))

    def get_forecast(self, region: str = "Pune") -> Dict[str, Any]:
        return self.execute_api_command("get-forecast", region)

    def run_dynamic(
        self,
        strategy: str,
        demands: Dict[str, float],
        failed_servers: Optional[List[str]] = None,
        congestion: Optional[str] = None,
        cache_hit_ratio: Optional[float] = None,
        record_trace: bool = False,
    ) -> Dict[str, Any]:
        d_pune = str(demands.get("Pune", 2500.0))
        d_mumbai = str(demands.get("Mumbai", 3500.0))
        d_delhi = str(demands.get("Delhi", 2000.0))
        d_blr = str(demands.get("Bangalore", 2500.0))
        d_hyd = str(demands.get("Hyderabad", 1500.0))
        failed_csv = ",".join(failed_servers) if failed_servers else "none"
        cong_str = congestion if congestion else "none"
        cache_str = str(cache_hit_ratio) if cache_hit_ratio is not None else "none"
        trace_str = "true" if record_trace else "false"

        action = "inspect-trace" if record_trace else "run-dynamic"
        return self.execute_api_command(
            action,
            strategy,
            d_pune,
            d_mumbai,
            d_delhi,
            d_blr,
            d_hyd,
            failed_csv,
            cong_str,
            cache_str,
            trace_str,
        )


# Global service singleton instance
cpp_engine = CppEngineService()
