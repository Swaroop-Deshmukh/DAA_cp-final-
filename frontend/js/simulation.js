/**
 * Simulation Lab View Controller
 * Interactive scenario injection for network failures, traffic surges, and recovery.
 */

import { ApiService } from './api.js';
import { EventLogManager } from './logs.js';
import { RoutingStudio } from './routing.js';
import { ServerMonitoring } from './monitoring.js';
import { TopologyViewer } from './topology.js';

export const SimulationLab = {
  activeScenarioId: 'normal',

  scenarios: [
    {
      id: 'normal',
      name: 'Normal Traffic Baseline',
      desc: 'Standard nominal load across all 5 regions (12,000 req/s total). All 4 edge POPs healthy.',
      type: 'Nominal',
      badgeClass: 'badge-emerald'
    },
    {
      id: 'mumbai_spike',
      name: 'Sudden Mumbai Surge',
      desc: 'Mumbai demand surges to 7,000 req/s during peak concurrency event. Neighbor POPs absorb load.',
      type: 'Surge',
      badgeClass: 'badge-amber'
    },
    {
      id: 'delhi_failure',
      name: 'Delhi POP Outage',
      desc: 'Delhi Server suffers outage. North regional traffic reroutes dynamically to Mumbai & Bangalore.',
      type: 'Failure',
      badgeClass: 'badge-rose'
    },
    {
      id: 'delhi_recovery',
      name: 'Delhi POP Restored',
      desc: 'Delhi Server comes back online. System automatically replans and restores optimal flow.',
      type: 'Recovery',
      badgeClass: 'badge-emerald'
    },
    {
      id: 'pune_congestion',
      name: 'Transit Link Congestion',
      desc: 'Pune link suffers +35ms transient congestion delay; flow intelligently shifts away.',
      type: 'Congestion',
      badgeClass: 'badge-amber'
    },
    {
      id: 'over_capacity',
      name: 'Massive Overcapacity',
      desc: 'Massive 24,000 req/s surge exceeding edge capacity; safely triggers Origin DC fallback for spill.',
      type: 'Overload',
      badgeClass: 'badge-rose'
    },
    {
      id: 'all_servers_failed',
      name: 'Catastrophic POP Outage',
      desc: 'All 4 edge POPs lose upstream connectivity. 100% traffic falls back cleanly to Origin DC.',
      type: 'Disaster',
      badgeClass: 'badge-rose'
    },
    {
      id: 'cache_improvement',
      name: 'High Cache Hit Rate',
      desc: '0/1 Knapsack optimal cache placement yields 98-99% hit rate, dramatically lowering routing cost.',
      type: 'Optimized',
      badgeClass: 'badge-emerald'
    }
  ],

  init() {
    this.renderCards();

    const resetBtn = document.getElementById('btnResetSimulation');
    if (resetBtn) {
      resetBtn.addEventListener('click', () => this.triggerScenario('normal'));
    }
  },

  renderCards() {
    const grid = document.getElementById('scenarioCardsGrid');
    if (!grid) return;

    grid.innerHTML = this.scenarios.map(sc => {
      const isActive = this.activeScenarioId === sc.id;
      return `
        <div class="scenario-card ${isActive ? 'active' : ''}" data-scenario="${sc.id}">
          <div>
            <div class="scenario-title">
              ${sc.name}
            </div>
            <div class="scenario-desc">${sc.desc}</div>
          </div>
          <div class="scenario-footer">
            <span class="badge ${sc.badgeClass}">${sc.type}</span>
            <button class="btn btn-secondary btn-sm btn-inject" data-scenario="${sc.id}">
              ${isActive ? 'Active' : 'Simulate'}
            </button>
          </div>
        </div>
      `;
    }).join('');

    grid.querySelectorAll('.btn-inject').forEach(btn => {
      btn.addEventListener('click', (e) => {
        e.stopPropagation();
        const scId = btn.getAttribute('data-scenario');
        this.triggerScenario(scId);
      });
    });

    grid.querySelectorAll('.scenario-card').forEach(card => {
      card.addEventListener('click', () => {
        const scId = card.getAttribute('data-scenario');
        this.triggerScenario(scId);
      });
    });
  },

  async triggerScenario(scenarioId) {
    this.activeScenarioId = scenarioId;
    this.renderCards();

    EventLogManager.add('SIMULATION', `Triggered test scenario: '${scenarioId}'`, 'WARN');
    window.showToast?.(`Injecting scenario: ${scenarioId}...`, 'info');

    try {
      const result = await ApiService.runScenario(scenarioId);

      const totalRouted = result.totalTrafficRouted ?? result.total_traffic_routed ?? 0;
      const originSpill = result.originTrafficRouted ?? result.traffic_sent_to_origin ?? 0;

      EventLogManager.add(
        'REPLAN',
        `Rebalanced network for '${scenarioId}'. Routed ${totalRouted.toLocaleString()} req/s, Origin Spill: ${originSpill.toLocaleString()} req/s`,
        originSpill > 0 ? 'WARN' : 'INFO',
        result
      );

      // Synchronize Routing Studio
      RoutingStudio.renderResults(result);

      // Refresh Servers & Topology
      await ServerMonitoring.refresh();
      TopologyViewer.render(null, result.allocations);

      // Refresh Decision Inspector with latest scenario snapshot
      if (window.Inspector && window.Inspector.refresh) {
        window.Inspector.refresh();
      }

      window.showToast?.(`Scenario '${scenarioId}' active! Flow recomputed.`, 'success');
    } catch (err) {
      console.error('Scenario injection failed:', err);
      EventLogManager.add('SIMULATION_ERROR', `Failed running scenario '${scenarioId}': ${err.message}`, 'ERROR');
      window.showToast?.(`Simulation error: ${err.message}`, 'error');
    }
  }
};
