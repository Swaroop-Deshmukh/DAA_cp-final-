/**
 * Algorithm Comparison View Controller
 * Evaluates MCMF vs Round Robin, Nearest Server, and Greedy Cost baselines.
 */

import { ApiService } from './api.js';
import { ChartManager } from './charts.js';

export const AlgorithmComparison = {
  currentScenario: 'normal',

  init() {
    const selector = document.getElementById('comparisonScenarioSelect');
    if (selector) {
      selector.addEventListener('change', (e) => {
        this.currentScenario = e.target.value;
        this.loadBenchmarks(this.currentScenario);
      });
    }
  },

  async loadBenchmarks(scenario = 'normal') {
    try {
      const data = await ApiService.getBenchmarkById(scenario);
      let strategies = [];
      if (data && data.strategies) {
        strategies = data.strategies;
      } else if (Array.isArray(data) && data.length > 0) {
        strategies = data[0].strategies || data;
      }
      this.renderTable(strategies);
      ChartManager.initComparisonCharts(strategies);
    } catch (err) {
      console.error('Failed to load benchmarks:', err);
    }
  },

  renderTable(benchmarks) {
    const tbody = document.getElementById('comparisonTableBody');
    if (!tbody || !benchmarks) return;

    // Find best cost
    let minCost = Infinity;
    benchmarks.forEach(b => {
      const c = b.averageRoutingCostPerReq ?? b.avg_cost_per_request ?? b.avgCost ?? Infinity;
      if (c < minCost) minCost = c;
    });

    tbody.innerHTML = benchmarks.map(b => {
      const name = b.strategyName || b.strategy || b.name || 'Unknown';
      const isMcmf = name.toLowerCase().includes('min-cost') || name.toLowerCase().includes('mcmf');
      const cost = b.averageRoutingCostPerReq ?? b.avg_cost_per_request ?? b.avgCost ?? 0;
      const latency = b.averageLatencyMs ?? b.avg_latency_ms ?? b.avgLatency ?? 0;
      const routed = b.totalTrafficRouted ?? b.total_traffic_routed ?? b.trafficRouted ?? 0;
      const origin = b.originTrafficRouted ?? b.traffic_sent_to_origin ?? b.originSpill ?? 0;
      const execTime = b.executionTimeMicroseconds !== undefined
        ? `${b.executionTimeMicroseconds} μs`
        : (b.execution_time_us !== undefined ? `${b.execution_time_us} μs` : (b.execution_time_ms ? `${b.execution_time_ms.toFixed(3)} ms` : '—'));

      const isWinner = Math.abs(cost - minCost) < 0.001;

      return `
        <tr class="${isMcmf ? 'strong' : ''}">
          <td>
            <div style="display:flex; align-items:center; gap:8px;">
              <span class="badge ${isMcmf ? 'badge-blue' : 'badge-gray'}">${name}</span>
              ${isWinner ? '<span class="badge badge-emerald" style="font-size:0.62rem">Lowest Cost</span>' : ''}
            </div>
          </td>
          <td class="mono">${routed.toLocaleString()} req/s</td>
          <td class="mono ${origin > 0 ? 'text-amber' : ''}">${origin.toLocaleString()} req/s</td>
          <td class="mono">${latency.toFixed(2)} ms</td>
          <td class="mono strong" style="color: ${isWinner ? 'var(--accent-emerald)' : 'var(--text-primary)'}">
            ${cost.toFixed(2)} pts
          </td>
          <td class="mono">${execTime}</td>
        </tr>
      `;
    }).join('');
  }
};
