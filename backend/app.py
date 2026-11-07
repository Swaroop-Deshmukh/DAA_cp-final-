import os
from pathlib import Path
from contextlib import asynccontextmanager
from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware
from fastapi.staticfiles import StaticFiles
from fastapi.responses import FileResponse
from backend.models.responses import (
    HealthResponse,
    NetworkResponse,
    MetricsResponse,
)
from backend.services.cpp_engine import cpp_engine
from backend.services.simulation_manager import simulation_manager
from backend.routes import (
    routing_router,
    simulation_router,
    scenarios_router,
    benchmarks_router,
    live_router,
)

FRONTEND_DIR = Path(__file__).resolve().parent.parent / "frontend"

@asynccontextmanager
async def lifespan(app: FastAPI):
    """Lifespan context manager: automatically boots continuous simulation on launch."""
    try:
        await simulation_manager.start(mode="normal", speed=1.0, strategy="Min-Cost Max-Flow")
    except Exception as e:
        print(f"[Warning] Failed to auto-start simulation on launch: {e}")
    yield
    try:
        await simulation_manager.reset()
    except Exception:
        pass

app = FastAPI(
    title="Adaptive CDN Traffic Router API",
    description="High-performance FastAPI bridge connecting to the C++17 Min-Cost Max-Flow routing optimization engine.",
    version="1.0.0",
    lifespan=lifespan,
)

# Enable Cross-Origin Resource Sharing (CORS) for Phase 10 frontend consumption
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# Include Modular Feature Routers
app.include_router(routing_router)
app.include_router(simulation_router)
app.include_router(scenarios_router)
app.include_router(benchmarks_router)
app.include_router(live_router)

# Mount frontend static directories
if (FRONTEND_DIR / "css").exists():
    app.mount("/css", StaticFiles(directory=str(FRONTEND_DIR / "css")), name="css")
if (FRONTEND_DIR / "js").exists():
    app.mount("/js", StaticFiles(directory=str(FRONTEND_DIR / "js")), name="js")
if FRONTEND_DIR.exists():
    app.mount("/static", StaticFiles(directory=str(FRONTEND_DIR)), name="static")


@app.get("/", tags=["Dashboard"])
def read_root():
    """Serves the Interactive Web Dashboard if available, otherwise metadata."""
    index_file = FRONTEND_DIR / "index.html"
    if index_file.exists():
        return FileResponse(index_file)
    return {
        "service": "Adaptive CDN Traffic Router API",
        "version": "1.0.0",
        "docs_url": "/docs",
        "dashboard_url": "/dashboard",
        "health_url": "/api/health",
        "status": "online",
    }


@app.get("/dashboard", tags=["Dashboard"])
def get_dashboard():
    """Serves the main interactive network operations dashboard."""
    index_file = FRONTEND_DIR / "index.html"
    if index_file.exists():
        return FileResponse(index_file)
    return {"message": "Dashboard UI not yet created."}


@app.get("/api/health", response_model=HealthResponse, tags=["General"])
def get_health():
    """Checks system health and C++ engine binary connectivity."""
    return cpp_engine.get_health()


@app.get("/api/network", response_model=NetworkResponse, tags=["Network"])
def get_network():
    """Retrieves current CDN network topology, including regions, edge servers, and links."""
    return cpp_engine.get_network()


@app.get("/api/metrics", response_model=MetricsResponse, tags=["Metrics"])
def get_metrics():
    """Retrieves fleet-wide utilization, total capacity, and system status metrics."""
    return cpp_engine.get_metrics()


@app.get("/api/cache", tags=["Optimization"])
def get_cache(budget: int = 350):
    """Executes 0/1 Knapsack DP cache placement on the C++ engine."""
    return cpp_engine.get_cache(budget_mb=budget)


@app.get("/api/forecast", tags=["Forecasting"])
def get_forecast(region: str = "Pune"):
    """Executes Holt linear exponential smoothing traffic forecasting on the C++ engine."""
    return cpp_engine.get_forecast(region=region)


if __name__ == "__main__":
    import uvicorn

    uvicorn.run("backend.app:app", host="127.0.0.1", port=8000, reload=True)

