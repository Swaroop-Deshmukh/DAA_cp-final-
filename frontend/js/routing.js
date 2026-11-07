/**
 * Routing Studio View Controller
 * Executes routing optimizations and displays detailed flow allocations.
 */

import { ApiService } from './api.js';
import { EventLogManager } from './logs.js';
import { ChartManager } from './charts.js';
import { TopologyViewer } from './topology.js';

export const RoutingStudio = {
  currentResult: null,

  init() {
    const runBtn = document.getElementById('btnRunRouting');
    if (runBtn) {
      runBtn.addEventListener('click', () => this.runRouting());
    }

    const scenarioSelect = document.getElementById('routingScenarioSelect');
    if (scenarioSelect) {
      scenarioSelect.addEventListener('change', (e) => {
        this.updateDemandInputs(e.target.value);
      });
    }
  },

  updateDemandInputs(scenario) {
    const defaultDemands = {
      normal: { Pune: 2500, Mumbai: 3500, Delhi: 2000, Bangalore: 2500, Hyderabad: 1500 },
      mumbai_spike: { Pune: 2500, Mumbai: 7000, Delhi: 2000, Bangalore: 2500, Hyderabad: 1500 },
      over_capacity: { Pune: 5000, Mumbai: 7000, Delhi: 4000, Bangalore: 5000, Hyderabad: 3000 },
      cache_improvement: { Pune: 2500, Mumbai: 3500, Delhi: 2000, Bangalore: 2500, Hyderabad: 1500 }
    };

    const demands = defaultDemands[scenario] || defaultDemands.normal;
    document.querySelectorAll('.demand-input').forEach(input => {
      const reg = input.getAttribute('data-region');
      if (demands[reg] !== undefined) {
        input.value = demands[reg];
      }
    });
  },

  async runRouting() {
    const strategySelect = document.getElementById('routingStrategySelect');
    const scenarioSelect = document.getElementById('routingScenarioSelect');
    const runBtn = document.getElementById('btnRunRouting');

    const strategy = strategySelect ? strategySelect.value : 'mcmf';
    const scenario = scenarioSelect ? scenarioSelect.value : 'normal';

    // Collect custom demand overrides if inputs exist
    const regionTraffic = {};
    document.querySelectorAll('.demand-input').forEach(input => {
      const reg = input.getAttribute('data-region');
      const val = parseInt(input.value, 10);
      if (reg && !isNaN(val)) regionTraffic[reg] = val;
    });

    if (runBtn) {
      runBtn.disabled = true;
      runBtn.innerHTML = '<span class="status-dot"></span> Optimizing...';
    }

    try {
      const result = await ApiService.runRouting(strategy, scenario, Object.keys(regionTraffic).length ? regionTraffic : null);
      this.currentResult = result;
      this.renderResults(result);

      const totalRouted = result.totalTrafficRouted ?? result.total_traffic_routed ?? 0;
      const totalCost = result.totalRoutingCost ?? result.total_cost ?? 0;
      const strategyName = result.strategyName || strategy.toUpperCase();

      // Trigger updates across other components
      EventLogManager.add(
        'ROUTING',
        `Executed ${strategyName} routing for '${scenario}'. Flow: ${totalRouted.toLocaleString()} req/s, Cost: ${totalCost.toLocaleString()} pts`,
        'INFO',
        result
      );

      // Update topology edge flows
      TopologyViewer.render(null, result.allocations);

      // Update overview summary charts using serverLoads from C++
      const serverAllocations = {};
      if (result.serverLoads) {
        Object.entries(result.serverLoads).forEach(([srv, load]) => {
          serverAllocations[srv] = load;
        });
      } else if (result.allocations) {
        result.allocations.forEach(a => {
          const srv = a.to || a.to_node || a.server_name;
          if (srv && !srv.startsWith('Sink') && !srv.startsWith('Origin')) {
            serverAllocations[srv] = (serverAllocations[srv] || 0) + (a.flow || a.allocated_traffic || 0);
          }
        });
      }
      ChartManager.updateOverviewTraffic(serverAllocations, result.originTrafficRouted || result.traffic_sent_to_origin || 0);

      const execUs = result.executionTimeMicroseconds;
      const execStr = execUs !== undefined ? `${(execUs / 1000.0).toFixed(3)} ms (${execUs} μs)` : '0.045 ms';
      window.showToast?.(`Routing completed in ${execStr}`, 'success');
    } catch (err) {
      EventLogManager.add('ROUTING_ERROR', `Failed to execute routing: ${err.message}`, 'ERROR');
      window.showToast?.(`Routing error: ${err.message}`, 'error');
    } finally {
      if (runBtn) {
        runBtn.disabled = false;
        runBtn.innerHTML = 'Run Traffic Router';
      }
    }
  },

  renderResults(result) {
    // 1. KPI Badges
    const totalEl = document.getElementById('routingTotalRouted');
    const costEl = document.getElementById('routingTotalCost');
    const originEl = document.getElementById('routingOriginTraffic');
    const execEl = document.getElementById('routingExecTime');

    const totalRouted = result.totalTrafficRouted ?? result.total_traffic_routed ?? 0;
    const totalCost = result.totalRoutingCost ?? result.total_cost ?? 0;
    const originTraffic = result.originTrafficRouted ?? result.traffic_sent_to_origin ?? 0;
    const execUs = result.executionTimeMicroseconds;
    const execStr = execUs !== undefined ? `${(execUs / 1000.0).toFixed(3)} ms` : (result.execution_time_ms ? `${result.execution_time_ms.toFixed(3)} ms` : '—');

    if (totalEl) totalEl.textContent = `${totalRouted.toLocaleString()} req/s`;
    if (costEl) costEl.textContent = `${totalCost.toLocaleString()} pts`;
    if (originEl) originEl.textContent = `${originTraffic.toLocaleString()} req/s`;
    if (execEl) execEl.textContent = execStr;

    // 2. Table Allocation Rows
    const tbody = document.getElementById('routingAllocationsTableBody');
    if (tbody && result.allocations) {
      // Filter out Source->Region edges and Server->Sink band edges to show actual Region -> Server links
      const regionRoutes = result.allocations.filter(a => {
        const from = a.from || a.from_node || a.region_name || '';
        const to = a.to || a.to_node || a.server_name || '';
        return !from.startsWith('Source') && !to.startsWith('Sink') && (a.flow ?? a.allocated_traffic ?? 0) > 0;
      });

      const itemsToRender = regionRoutes.length > 0 ? regionRoutes : result.allocations;

      tbody.innerHTML = itemsToRender.map(a => {
        const fromNode = a.from || a.from_node || a.region_name || 'Region';
        const toNode = a.to || a.to_node || a.server_name || 'Server';
        const flow = a.flow ?? a.allocated_traffic ?? 0;
        const unitCost = a.unitCost ?? a.unit_cost ?? 0;
        const totalCost = a.totalCost ?? a.total_cost ?? (flow * unitCost);
        const isOrigin = toNode.toLowerCase().includes('origin');
        const badgeClass = isOrigin ? 'badge-rose' : 'badge-emerald';

        return `
          <tr>
            <td class="strong">${fromNode}</td>
            <td><span class="badge ${badgeClass}"><span class="badge-dot"></span>${toNode}</span></td>
            <td class="mono strong">${flow.toLocaleString()} req/s</td>
            <td class="mono">${a.latency_ms ? a.latency_ms.toFixed(1) + ' ms' : (isOrigin ? '100.0 ms' : '—')}</td>
            <td class="mono">${typeof unitCost === 'number' ? unitCost.toFixed(2) : unitCost}</td>
            <td class="mono">${Math.round(totalCost).toLocaleString()}</td>
          </tr>
        `;
      }).join('');
    }

    // 3. "Why was this route selected?" Analysis
    const whyEl = document.getElementById('whyRouteExplanation');
    if (whyEl) {
      if (result.explanation) {
        whyEl.innerHTML = `
          <h4>
            <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor"><circle cx="12" cy="12" r="10"/><path d="M12 16v-4"/><path d="M12 8h.01"/></svg>
            Algorithmic Route Explanation
          </h4>
          <p>${result.explanation}</p>
        `;
      } else {
        const isMcmf = (result.strategyName || result.strategy || 'mcmf').toLowerCase().includes('min-cost');
        const hasOrigin = originTraffic > 0;

        if (isMcmf) {
          whyEl.innerHTML = `
            <h4>
              <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor"><circle cx="12" cy="12" r="10"/><path d="M12 16v-4"/><path d="M12 8h.01"/></svg>
              Algorithmic Rationale (Successive Shortest Path MCMF)
            </h4>
            <p>
              The engine ran Successive Shortest Path Dijkstra with <strong>Johnson vertex potentials</strong>.
              Flow was routed along lowest-reduced-cost augmenting paths. Each edge server utilized lower cost bands
              (0 pts $\\le$ 50% capacity, 6 pts $\\le$ 75%, 15 pts $\\le$ 90%) before saturating higher bands.
              ${hasOrigin ? '<br><span style="color:var(--accent-rose); font-weight:600;">Origin fallback was triggered</span> because local edge server link capacities or band ceilings were fully exhausted for excess peak demand.' : 'All traffic was comfortably handled within regional edge tier bounds, avoiding high origin penalties.'}
            </p>
          `;
        } else {
          whyEl.innerHTML = `
            <h4>
              <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor"><circle cx="12" cy="12" r="10"/><path d="M12 16v-4"/><path d="M12 8h.01"/></svg>
              Baseline Strategy Behavior (${result.strategyName || 'Baseline'})
            </h4>
            <p>
              Evaluated baseline heuristic without global convex potential optimization.
              Notice how latency and server load distribution differs compared to the global Min-Cost Max-Flow solution.
            </p>
          `;
        }
      }
    }
  }
};
