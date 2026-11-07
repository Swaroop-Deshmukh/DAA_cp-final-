/**
 * API Service Module
 * Handles all async communication with the FastAPI backend.
 */

const API_BASE = ''; // Same-origin relative paths

export const ApiService = {
  // Connection state callback
  onStatusChange: null,

  async request(endpoint, options = {}) {
    try {
      const response = await fetch(`${API_BASE}${endpoint}`, {
        headers: {
          'Content-Type': 'application/json',
          ...options.headers,
        },
        ...options,
      });

      if (!response.ok) {
        const errorText = await response.text();
        throw new Error(`API error ${response.status}: ${errorText}`);
      }

      if (this.onStatusChange) this.onStatusChange(true);
      return await response.json();
    } catch (err) {
      if (this.onStatusChange) this.onStatusChange(false, err.message);
      console.error(`[API Error] ${endpoint}:`, err);
      throw err;
    }
  },

  // Endpoints
  async getHealth() {
    return this.request('/api/health');
  },

  async getNetwork() {
    return this.request('/api/network');
  },

  async getMetrics() {
    return this.request('/api/metrics');
  },

  async runRouting(strategy = 'mcmf', scenario = 'normal', regionTraffic = null) {
    const payload = { strategy, scenario };
    if (regionTraffic) {
      payload.region_traffic = regionTraffic;
    }
    return this.request('/api/routing/run', {
      method: 'POST',
      body: JSON.stringify(payload),
    });
  },

  async runSimulation(eventType, target, value = 1.5, strategy = 'mcmf', secondaryTarget = 'Pune Server') {
    const payload = { event_type: eventType, target, value, strategy, secondary_target: secondaryTarget };
    return this.request('/api/simulation/run', {
      method: 'POST',
      body: JSON.stringify(payload),
    });
  },

  async runScenario(scenarioId) {
    return this.request('/api/scenarios/run', {
      method: 'POST',
      body: JSON.stringify({ scenario: scenarioId }),
    });
  },

  async getBenchmarks(scenario = null) {
    const url = scenario ? `/api/benchmarks?scenario=${encodeURIComponent(scenario)}` : '/api/benchmarks';
    return this.request(url);
  },

  async getBenchmarkById(scenarioId) {
    return this.request(`/api/benchmarks/${encodeURIComponent(scenarioId)}`);
  },

  async getCachePlacement(budgetMb = 350) {
    return this.request(`/api/cache?budget=${budgetMb}`);
  },

  async getForecast(region = 'mumbai', periods = 5) {
    return this.request(`/api/forecast?region=${encodeURIComponent(region)}&periods=${periods}`);
  },

  // Live Simulation Controls
  async liveStart(mode = 'normal', speed = 1.0, strategy = 'Min-Cost Max-Flow', seed = 42) {
    return this.request('/api/live/start', {
      method: 'POST',
      body: JSON.stringify({ mode, speed, strategy, seed }),
    });
  },

  async livePause() {
    return this.request('/api/live/pause', { method: 'POST' });
  },

  async liveResume() {
    return this.request('/api/live/resume', { method: 'POST' });
  },

  async liveReset() {
    return this.request('/api/live/reset', { method: 'POST' });
  },

  async liveStep() {
    return this.request('/api/live/step', { method: 'POST' });
  },

  async liveConfig(config) {
    return this.request('/api/live/config', {
      method: 'POST',
      body: JSON.stringify(config),
    });
  },

  async liveInject(eventType, target, value = 1.5) {
    return this.request('/api/live/inject', {
      method: 'POST',
      body: JSON.stringify({ event_type: eventType, target, value }),
    });
  },

  // Routing Decision Inspector
  async getInspectorHistory() {
    return this.request('/api/inspector/history');
  },

  async getInspectorSnapshot(tick) {
    return this.request(`/api/inspector/snapshot/${tick}`);
  },

  async getInspectorLatest() {
    return this.request('/api/inspector/latest');
  },

  async replayInspector(tick, strategy, demands = null, failedServers = null, congestion = null) {
    return this.request('/api/inspector/replay', {
      method: 'POST',
      body: JSON.stringify({
        tick,
        strategy,
        demands,
        failed_servers: failedServers,
        congestion
      }),
    });
  }
};
