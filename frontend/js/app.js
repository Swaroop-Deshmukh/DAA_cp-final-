/**
 * Main Application Controller
 * Manages view switching, polling, global inspector drawer, and system state.
 */

import { ApiService } from './api.js';
import { ChartManager } from './charts.js';
import { TopologyViewer } from './topology.js';
import { RoutingStudio } from './routing.js';
import { ServerMonitoring } from './monitoring.js';
import { AlgorithmComparison } from './comparison.js';
import { SimulationLab } from './simulation.js';
import { ForecastAndCache } from './forecast.js';
import { EventLogManager } from './logs.js';
import { LiveSimulation } from './live.js';
import { Inspector } from './inspector.js';

class App {
  constructor() {
    this.currentView = 'overview';
    this.pollInterval = null;
  }

  init() {
    // 1. Toast Notification Setup
    window.showToast = this.showToast.bind(this);

    // 2. Navigation Routing
    this.initNavigation();

    // 3. Inspection Drawer Setup
    this.initDrawer();

    // 4. API Status Handler
    ApiService.onStatusChange = (isOnline, msg) => {
      const dot = document.getElementById('connectionStatusDot');
      const text = document.getElementById('connectionStatusText');
      if (dot && text) {
        if (isOnline) {
          dot.className = 'status-dot';
          text.textContent = 'Backend Connected';
        } else {
          dot.className = 'status-dot disconnected';
          text.textContent = 'Backend Offline';
        }
      }
    };

    // 5. Initialize Sub-modules
    ChartManager.initOverviewCharts();
    TopologyViewer.init(this.openNodeInspector.bind(this));
    RoutingStudio.init();
    SimulationLab.init();
    AlgorithmComparison.init();
    ForecastAndCache.init();
    LiveSimulation.init();
    Inspector.init();
    window.Inspector = Inspector;
    window.switchTab = (viewName) => this.switchView(viewName);
    this.initLogFilters();

    // 6. Connect Refresh Button
    const refreshBtn = document.getElementById('btnRefreshAll');
    if (refreshBtn) {
      refreshBtn.addEventListener('click', () => this.refreshData(true));
    }

    // 7. Initial Data Load
    this.refreshData();

    // 8. Start Background Polling (every 8 seconds)
    this.pollInterval = setInterval(() => {
      this.refreshData(false);
    }, 8000);

    EventLogManager.init();
  }

  initNavigation() {
    const navItems = document.querySelectorAll('.nav-item');
    navItems.forEach(item => {
      item.addEventListener('click', (e) => {
        e.preventDefault();
        const targetView = item.getAttribute('data-view');
        if (targetView) this.switchView(targetView);
      });
    });

    const closeInspectorBtn = document.getElementById('btnCloseInspector');
    if (closeInspectorBtn) {
      closeInspectorBtn.addEventListener('click', () => {
        this.switchView('live');
      });
    }
  }

  switchView(viewName) {
    this.currentView = viewName;

    // Update active nav item
    document.querySelectorAll('.nav-item').forEach(item => {
      item.classList.toggle('active', item.getAttribute('data-view') === viewName);
    });

    // Update view containers
    document.querySelectorAll('.view-container').forEach(container => {
      container.classList.toggle('active', container.id === `view-${viewName}`);
    });

    // View-specific trigger refreshes
    if (viewName === 'topology') {
      TopologyViewer.render();
    } else if (viewName === 'monitoring') {
      ServerMonitoring.refresh();
    } else if (viewName === 'comparison') {
      AlgorithmComparison.loadBenchmarks();
    } else if (viewName === 'inspector') {
      Inspector.refresh();
      Inspector.fitGraph();
    } else if (viewName === 'logs') {
      this.renderLogs();
    }
  }

  async refreshData(showFeedback = false) {
    try {
      // 1. Fetch live metrics
      const metrics = await ApiService.getMetrics();
      this.updateOverviewMetrics(metrics);

      // 2. Fetch network state
      const net = await ApiService.getNetwork();
      ServerMonitoring.renderServers(net.servers);
      TopologyViewer.render(net);

      // 3. Update Last Updated Timestamp
      const lastUpdatedEl = document.getElementById('lastUpdatedTime');
      if (lastUpdatedEl) {
        lastUpdatedEl.textContent = new Date().toLocaleTimeString();
      }

      if (showFeedback) {
        this.showToast('Dashboard metrics refreshed', 'success');
      }
    } catch (err) {
      console.warn('Background sync encountered issue:', err.message);
    }
  }

  updateOverviewMetrics(metrics) {
    const totalTrafficEl = document.getElementById('kpiTotalTraffic');
    const avgLatencyEl = document.getElementById('kpiAvgLatency');
    const edgeRatioEl = document.getElementById('kpiEdgeRatio');
    const healthyServersEl = document.getElementById('kpiHealthyServers');

    const totalDemand = metrics.totalDemand ?? metrics.total_traffic ?? 12000;
    const utilization = metrics.fleetUtilization ?? metrics.edge_efficiency_pct ?? 58.5;
    const totalServers = metrics.totalServers ?? 4;
    const healthy = metrics.healthy_servers ?? (metrics.status === 'healthy' ? totalServers : totalServers - 1);

    if (totalTrafficEl) {
      totalTrafficEl.textContent = `${totalDemand.toLocaleString()} req/s`;
    }
    if (avgLatencyEl) {
      avgLatencyEl.textContent = metrics.average_latency ? `${metrics.average_latency.toFixed(1)} ms` : '14.2 ms';
    }
    if (edgeRatioEl) {
      edgeRatioEl.textContent = `${Math.min(100, Math.round(utilization))}% Fleet Load`;
    }
    if (healthyServersEl) {
      healthyServersEl.textContent = `${healthy} / ${totalServers}`;
    }
  }

  initDrawer() {
    const drawer = document.getElementById('inspectDrawer');
    const closeBtn = document.getElementById('drawerCloseBtn');
    if (closeBtn && drawer) {
      closeBtn.addEventListener('click', () => {
        drawer.classList.remove('open');
      });
    }
  }

  openNodeInspector(node) {
    const drawer = document.getElementById('inspectDrawer');
    const title = document.getElementById('drawerTitle');
    const content = document.getElementById('drawerBody');

    if (!drawer || !title || !content) return;

    title.textContent = node.label;
    let detailsHtml = '';

    if (node.type === 'server') {
      const isOnline = node.healthy ?? true;
      detailsHtml = `
        <div style="margin-bottom:16px;">
          <span class="badge ${isOnline ? 'badge-emerald' : 'badge-rose'}">
            <span class="badge-dot"></span>${isOnline ? 'ONLINE (HEALTHY)' : 'OFFLINE (OUTAGE)'}
          </span>
        </div>
        <div class="form-group">
          <label class="form-label">Location / Tier</label>
          <div class="mono" style="color:var(--text-primary)">${node.name} &bull; Tier-1 POP</div>
        </div>
        <div class="form-group">
          <label class="form-label">Configured Capacity</label>
          <div class="mono" style="color:var(--text-primary)">${(node.capacity || 3000).toLocaleString()} req/s</div>
        </div>
        <div class="form-group">
          <label class="form-label">Current Active Load</label>
          <div class="mono strong" style="color:var(--accent-blue)">${(node.currentLoad || 0).toLocaleString()} req/s</div>
        </div>
        <div class="form-group">
          <label class="form-label">Load Band Schedule (Convex MCMF Cost)</label>
          <div style="font-size:0.75rem; color:var(--text-secondary); line-height:1.5;">
            &bull; Band 1 (0–50% capacity): Cost 0 pts<br>
            &bull; Band 2 (50–75% capacity): Cost 6 pts<br>
            &bull; Band 3 (75–90% capacity): Cost 15 pts<br>
            &bull; Band 4 (90–100% capacity): Cost 60 pts
          </div>
        </div>
      `;
    } else if (node.type === 'region') {
      detailsHtml = `
        <div class="form-group">
          <label class="form-label">Region Name</label>
          <div class="mono strong" style="color:var(--text-primary)">${node.label}</div>
        </div>
        <div class="form-group">
          <label class="form-label">Nominal Ingress Demand</label>
          <div class="mono" style="color:var(--accent-blue)">${node.sub}</div>
        </div>
        <div class="form-group">
          <label class="form-label">Primary Assigned POP</label>
          <div class="mono">${node.name === 'North' ? 'Edge-1 (Delhi)' : (node.name === 'West' ? 'Edge-2 (Mumbai)' : (node.name === 'South' ? 'Edge-3 (Bangalore)' : 'Edge-4 (Kolkata)'))}</div>
        </div>
      `;
    } else if (node.type === 'origin') {
      detailsHtml = `
        <div style="margin-bottom:16px;">
          <span class="badge badge-blue"><span class="badge-dot"></span>CENTRAL DATA CENTER</span>
        </div>
        <div class="form-group">
          <label class="form-label">Role</label>
          <div style="font-size:0.8rem; color:var(--text-secondary)">
            Authoritative source of truth and disaster failover hub when edge POP capacities are saturated.
          </div>
        </div>
        <div class="form-group">
          <label class="form-label">Routing Penalty Cost</label>
          <div class="mono strong" style="color:var(--accent-rose)">500 pts / unit flow</div>
        </div>
      `;
    }

    content.innerHTML = detailsHtml;
    drawer.classList.add('open');
  }

  initLogFilters() {
    EventLogManager.subscribe(() => {
      if (this.currentView === 'logs' || this.currentView === 'overview') {
        this.renderLogs();
      }
    });

    const searchInput = document.getElementById('logSearchInput');
    const levelSelect = document.getElementById('logLevelFilter');

    if (searchInput) {
      searchInput.addEventListener('input', () => this.renderLogs());
    }
    if (levelSelect) {
      levelSelect.addEventListener('change', () => this.renderLogs());
    }
  }

  renderLogs() {
    // 1. Overview recent logs widget
    const overviewContainer = document.getElementById('overviewRecentLogsList');
    if (overviewContainer) {
      const recent = EventLogManager.logs.slice(0, 5);
      overviewContainer.innerHTML = recent.map(log => `
        <div style="display:flex; justify-content:space-between; align-items:center; padding:8px 0; border-bottom:1px solid var(--border-subtle); font-size:0.75rem;">
          <div style="display:flex; align-items:center; gap:8px;">
            <span class="badge ${log.level === 'ERROR' ? 'badge-rose' : (log.level === 'WARN' ? 'badge-amber' : 'badge-blue')}">${log.level}</span>
            <span style="color:var(--text-primary)">${log.message}</span>
          </div>
          <span class="mono" style="color:var(--text-muted)">${log.timestamp}</span>
        </div>
      `).join('');
    }

    // 2. Full Event Logs Table
    const tbody = document.getElementById('eventLogsTableBody');
    if (!tbody) return;

    const searchInput = document.getElementById('logSearchInput');
    const levelSelect = document.getElementById('logLevelFilter');
    const searchVal = searchInput ? searchInput.value : '';
    const levelVal = levelSelect ? levelSelect.value : 'ALL';

    const filtered = EventLogManager.filter('ALL', levelVal, searchVal);

    tbody.innerHTML = filtered.map(log => {
      let badgeClass = 'badge-blue';
      if (log.level === 'ERROR') badgeClass = 'badge-rose';
      else if (log.level === 'WARN') badgeClass = 'badge-amber';
      else if (log.level === 'REROUTE') badgeClass = 'badge-emerald';

      return `
        <tr>
          <td class="mono" style="color:var(--text-muted)">${log.timestamp}</td>
          <td><span class="badge ${badgeClass}">${log.level}</span></td>
          <td class="mono strong">${log.category}</td>
          <td style="color:var(--text-primary)">${log.message}</td>
        </tr>
      `;
    }).join('');
  }

  showToast(message, type = 'info') {
    const container = document.getElementById('toast-container');
    if (!container) return;

    const toast = document.createElement('div');
    toast.className = `toast ${type}`;
    toast.innerHTML = `
      <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor">
        <circle cx="12" cy="12" r="10"/><path d="M12 16v-4"/><path d="M12 8h.01"/>
      </svg>
      <span>${message}</span>
    `;

    container.appendChild(toast);
    setTimeout(() => {
      toast.style.opacity = '0';
      toast.style.transition = 'opacity 0.25s ease';
      setTimeout(() => toast.remove(), 250);
    }, 3500);
  }
}

// Instantiate on DOM ready
document.addEventListener('DOMContentLoaded', () => {
  window.app = new App();
  window.app.init();
});

