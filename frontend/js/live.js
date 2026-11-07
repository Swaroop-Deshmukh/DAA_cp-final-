/**
 * Continuous Live Real-Time Simulation Controller
 * Manages WebSocket streaming, dynamic time-series charts, live replanning telemetry,
 * and user simulation controls.
 */

import { ApiService } from './api.js';
import { TopologyViewer } from './topology.js';
import { ServerMonitoring } from './monitoring.js';
import { EventLogManager } from './logs.js';

export const LiveSimulation = {
  socket: null,
  isConnected: false,
  status: 'stopped', // 'stopped', 'running', 'paused'
  mode: 'normal',
  speed: 1.0,
  strategy: 'Min-Cost Max-Flow',
  step: 0,
  simTimeSec: 0,

  // Rolling chart buffers (keep last 30 data points)
  maxDataPoints: 30,
  timeSeries: {
    labels: [],
    demand: [],
    edgeFlow: [],
    originFlow: [],
    latency: [],
    cost: [],
    serverUtil: {
      pune: [],
      mumbai: [],
      delhi: [],
      bangalore: []
    }
  },

  charts: {},

  init() {
    this.initControls();
    this.initCharts();
    this.connectWebSocket();
  },

  connectWebSocket() {
    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    const wsUrl = `${protocol}//${window.location.host}/ws/live`;

    try {
      this.socket = new WebSocket(wsUrl);

      this.socket.onopen = () => {
        this.isConnected = true;
        this.updateConnectionBadge(true);
      };

      this.socket.onmessage = (event) => {
        try {
          const msg = JSON.parse(event.data);
          this.handleSocketMessage(msg);
        } catch (err) {
          console.error('[Live WS Error] Failed parsing message:', err);
        }
      };

      this.socket.onclose = () => {
        this.isConnected = false;
        this.updateConnectionBadge(false);
        // Try auto-reconnect after 3 seconds if page is active
        setTimeout(() => {
          if (!this.isConnected) this.connectWebSocket();
        }, 3000);
      };

      this.socket.onerror = (err) => {
        console.warn('[Live WS Error]:', err);
      };
    } catch (err) {
      console.warn('[Live WS Connect Exception]:', err);
    }
  },

  updateConnectionBadge(online) {
    const badge = document.getElementById('liveWsStatusBadge');
    if (badge) {
      if (online) {
        badge.className = 'badge badge-emerald';
        badge.innerHTML = '<span class="badge-dot"></span>WS STREAM LIVE';
      } else {
        badge.className = 'badge badge-rose';
        badge.innerHTML = '<span class="badge-dot"></span>WS RECONNECTING';
      }
    }
  },

  handleSocketMessage(msg) {
    if (msg.type === 'LIVE_TICK') {
      this.handleTick(msg.data);
    } else if (msg.type === 'INIT_STATE' || msg.type === 'RESET_STATE') {
      if (msg.data && msg.data.status) {
        this.applyStatusUpdate(msg.data.status);
      }
    } else if (msg.type === 'STATUS_CHANGE') {
      this.applyStatusUpdate({ status: msg.status });
    } else if (msg.type === 'CONFIG_CHANGE') {
      this.applyStatusUpdate(msg.data);
    }
  },

  initControls() {
    // Start / Pause / Resume / Reset
    const btnStart = document.getElementById('btnLiveStart');
    const btnPause = document.getElementById('btnLivePause');
    const btnResume = document.getElementById('btnLiveResume');
    const btnReset = document.getElementById('btnLiveReset');

    if (btnStart) btnStart.addEventListener('click', () => this.sendAction('start'));
    if (btnPause) btnPause.addEventListener('click', () => this.sendAction('pause'));
    if (btnResume) btnResume.addEventListener('click', () => this.sendAction('resume'));
    if (btnReset) btnReset.addEventListener('click', () => this.sendAction('reset'));

    // Step forward 1 tick
    const btnStep = document.getElementById('btnLiveStep');
    if (btnStep) {
      btnStep.addEventListener('click', async () => {
        try {
          const res = await ApiService.liveStep();
          this.applyStatusUpdate(res);
          if (window.showToast) window.showToast('Advanced simulation by 1 tick.', 'info');
        } catch (e) {
          console.error('Failed to step forward:', e);
        }
      });
    }

    // Inspect current decision
    const btnInspect = document.getElementById('btnLiveInspect');
    if (btnInspect) {
      btnInspect.addEventListener('click', () => {
        if (this.status === 'running') {
          this.sendAction('pause');
        }
        const navInspector = document.querySelector('.nav-item[data-view="inspector"]');
        if (navInspector) navInspector.click();
      });
    }

    // Mode Toggle
    const modeSelect = document.getElementById('liveModeSelect');
    if (modeSelect) {
      modeSelect.addEventListener('change', (e) => {
        this.mode = e.target.value;
        this.sendAction('set_mode', { mode: this.mode });
        if (this.mode === 'stress') {
          window.showToast?.('Stress Mode active: Pune transit link congestion (+35ms) injected. Traffic dynamically rerouting to Mumbai Server.', 'warning');
        } else {
          window.showToast?.('Normal Mode active: Baseline network conditions restored.', 'info');
        }
        if (window.Inspector && window.Inspector.refresh) {
          setTimeout(() => window.Inspector.refresh(), 200);
        }
      });
    }

    // Speed Selector
    const speedSelect = document.getElementById('liveSpeedSelect');
    if (speedSelect) {
      speedSelect.addEventListener('change', (e) => {
        this.speed = parseFloat(e.target.value);
        this.sendAction('set_speed', { speed: this.speed });
      });
    }

    // Algorithm Selector
    const stratSelect = document.getElementById('liveStrategySelect');
    if (stratSelect) {
      stratSelect.addEventListener('change', (e) => {
        this.strategy = e.target.value;
        this.sendAction('set_strategy', { strategy: this.strategy });
      });
    }
  },

  sendAction(action, extra = {}) {
    if (this.socket && this.socket.readyState === WebSocket.OPEN) {
      this.socket.send(JSON.stringify({ action, ...extra }));
    } else {
      // Fallback via HTTP REST
      if (action === 'start') ApiService.request('/api/live/start', { method: 'POST', body: JSON.stringify({ mode: this.mode, speed: this.speed, strategy: this.strategy }) });
      else if (action === 'pause') ApiService.request('/api/live/pause', { method: 'POST' });
      else if (action === 'resume') ApiService.request('/api/live/resume', { method: 'POST' });
      else if (action === 'reset') ApiService.request('/api/live/reset', { method: 'POST' });
      else if (action === 'set_speed' || action === 'set_strategy' || action === 'set_mode') {
        ApiService.request('/api/live/config', { method: 'POST', body: JSON.stringify(extra) });
      }
    }
  },

  applyStatusUpdate(data) {
    if (data.status) {
      this.status = data.status;
      const statusBadge = document.getElementById('liveStateBadge');
      const btnStart = document.getElementById('btnLiveStart');
      const btnPause = document.getElementById('btnLivePause');
      const btnResume = document.getElementById('btnLiveResume');

      if (statusBadge) {
        if (this.status === 'running') {
          statusBadge.className = 'badge badge-emerald';
          statusBadge.innerHTML = '<span class="badge-dot"></span>SIMULATING (LIVE)';
          if (btnStart) btnStart.style.display = 'none';
          if (btnResume) btnResume.style.display = 'none';
          if (btnPause) btnPause.style.display = 'inline-flex';
        } else if (this.status === 'paused') {
          statusBadge.className = 'badge badge-amber';
          statusBadge.innerHTML = '<span class="badge-dot"></span>PAUSED';
          if (btnStart) btnStart.style.display = 'none';
          if (btnPause) btnPause.style.display = 'none';
          if (btnResume) btnResume.style.display = 'inline-flex';
        } else {
          statusBadge.className = 'badge badge-gray';
          statusBadge.innerHTML = '<span class="badge-dot"></span>IDLE / STOPPED';
          if (btnStart) btnStart.style.display = 'inline-flex';
          if (btnPause) btnPause.style.display = 'none';
          if (btnResume) btnResume.style.display = 'none';
        }
      }
    }

    if (data.step !== undefined) this.step = data.step;
    if (data.sim_time_sec !== undefined) this.simTimeSec = data.sim_time_sec;
    this.updateTimeDisplay();
  },

  handleTick(data) {
    this.step = data.step;
    this.simTimeSec = data.sim_time_sec;
    this.status = data.status;
    this.mode = data.mode;
    this.strategy = data.strategy;

    this.applyStatusUpdate(data);
    this.updateKPIs(data);
    this.updateReplanningBanner(data);
    this.updateCharts(data);

    // Update Topology and Server Cards
    if (data.routing) {
      TopologyViewer.render(null, data.routing.allocations);
      this.renderLiveTable(data.routing.allocations);
    }
  },

  updateKPIs(data) {
    const demand = data.total_demand || 0;
    const routed = data.routing ? data.routing.totalTrafficRouted : demand;
    const origin = data.routing ? data.routing.originTrafficRouted : 0;
    const latency = data.routing ? data.routing.averageLatencyMs : 0;
    const cost = data.routing ? data.routing.totalRoutingCost : 0;
    const failedCount = (data.failed_servers || []).length;
    const activeServers = 4 - failedCount;

    const elDemand = document.getElementById('liveKpiDemand');
    const elRouted = document.getElementById('liveKpiRouted');
    const elOrigin = document.getElementById('liveKpiOrigin');
    const elLatency = document.getElementById('liveKpiLatency');
    const elCost = document.getElementById('liveKpiCost');
    const elActive = document.getElementById('liveKpiActiveServers');

    if (elDemand) elDemand.textContent = `${demand.toLocaleString()} req/s`;
    if (elRouted) elRouted.textContent = `${routed.toLocaleString()} req/s`;
    if (elOrigin) {
      elOrigin.textContent = `${origin.toLocaleString()} req/s`;
      elOrigin.style.color = origin > 0 ? 'var(--accent-rose)' : 'var(--text-primary)';
    }
    if (elLatency) elLatency.textContent = `${latency.toFixed(2)} ms`;
    if (elCost) elCost.textContent = `${Math.round(cost).toLocaleString()} pts`;
    if (elActive) elActive.textContent = `${activeServers} / 4`;
  },

  updateReplanningBanner(data) {
    const banner = document.getElementById('liveDecisionBanner');
    const badge = document.getElementById('liveDecisionBadge');
    const text = document.getElementById('liveDecisionText');
    const deltaEl = document.getElementById('liveCostDeltaText');
    const solveEl = document.getElementById('liveSolveTimeText');

    if (!banner || !badge || !text) return;

    const reason = data.decision_reason || 'UNCHANGED';
    let badgeClass = 'badge-blue';
    let desc = 'Routing state maintained within stability hysteresis thresholds.';

    if (reason === 'TRIGGER_SERVER_FAILURE') {
      badgeClass = 'badge-rose';
      desc = `Hardware outage detected on: ${(data.failed_servers || []).join(', ')}. Instant failover recalculation executed.`;
    } else if (reason === 'TRIGGER_SERVER_RECOVERY') {
      badgeClass = 'badge-emerald';
      desc = 'Edge POP restored online. Traffic automatically rebalanced across the fleet.';
    } else if (reason === 'TRIGGER_TRAFFIC_SPIKE') {
      badgeClass = 'badge-amber';
      desc = 'Significant regional demand surge detected (Delta >= 8%). Flow re-optimized.';
    } else if (reason === 'TRIGGER_CONGESTION') {
      badgeClass = 'badge-amber';
      desc = `Transit link delay penalty updated (${data.congestion_spec}). Flow diverted from congested path.`;
    } else if (reason === 'REPLANNED' || reason === 'INITIAL_PLAN') {
      badgeClass = 'badge-emerald';
      desc = 'Global optimal flow recomputed via C++ Min-Cost Max-Flow Johnson potentials.';
    }

    badge.className = `badge ${badgeClass}`;
    badge.innerHTML = `<span class="badge-dot"></span>${reason.replace(/_/g, ' ')}`;
    text.textContent = desc;

    if (deltaEl && data.cost_delta !== undefined) {
      const delta = Math.round(data.cost_delta);
      const sign = delta > 0 ? `+${delta}` : `${delta}`;
      deltaEl.textContent = `Cost Delta: ${sign} pts`;
      deltaEl.style.color = delta > 0 ? 'var(--accent-amber)' : (delta < 0 ? 'var(--accent-emerald)' : 'var(--text-muted)');
    }

    if (solveEl && data.routing && data.routing.executionTimeMicroseconds !== undefined) {
      solveEl.textContent = `C++ Solve Time: ${data.routing.executionTimeMicroseconds} μs`;
    }
  },

  updateTimeDisplay() {
    const el = document.getElementById('liveSimTimeDisplay');
    const elStep = document.getElementById('liveStepDisplay');
    if (el) {
      const m = Math.floor(this.simTimeSec / 60).toString().padStart(2, '0');
      const s = (this.simTimeSec % 60).toString().padStart(2, '0');
      el.textContent = `${m}:${s}`;
    }
    if (elStep) {
      elStep.textContent = `Tick #${this.step}`;
    }
  },

  initCharts() {
    const darkOpts = {
      responsive: true,
      maintainAspectRatio: false,
      animation: false, // Turn off heavy animation for smooth 1s streaming
      plugins: {
        legend: { labels: { color: '#94a3b8', font: { family: 'Inter', size: 10 } } },
        tooltip: { backgroundColor: '#1f2937', borderColor: 'rgba(255,255,255,0.1)', borderWidth: 1 }
      },
      scales: {
        x: { grid: { color: 'rgba(255, 255, 255, 0.05)' }, ticks: { color: '#64748b', font: { size: 9 } } },
        y: { grid: { color: 'rgba(255, 255, 255, 0.05)' }, ticks: { color: '#64748b', font: { size: 9 } } }
      }
    };

    // 1. Demand & Edge Flow Chart
    const ctxDemand = document.getElementById('liveDemandChart');
    if (ctxDemand) {
      this.charts.demand = new Chart(ctxDemand, {
        type: 'line',
        data: {
          labels: [],
          datasets: [
            { label: 'Total Ingress (req/s)', data: [], borderColor: '#3b82f6', backgroundColor: 'rgba(59,130,246,0.1)', tension: 0.2, fill: true, borderWidth: 2 },
            { label: 'Edge Served (req/s)', data: [], borderColor: '#10b981', borderWidth: 2, tension: 0.2 },
            { label: 'Origin Spill (req/s)', data: [], borderColor: '#ef4444', borderDash: [4, 4], borderWidth: 2, tension: 0.2 }
          ]
        },
        options: darkOpts
      });
    }

    // 2. Server Utilization Chart
    const ctxServer = document.getElementById('liveServerUtilChart');
    if (ctxServer) {
      this.charts.server = new Chart(ctxServer, {
        type: 'line',
        data: {
          labels: [],
          datasets: [
            { label: 'Pune Server (%)', data: [], borderColor: '#10b981', borderWidth: 2, tension: 0.2 },
            { label: 'Mumbai Server (%)', data: [], borderColor: '#3b82f6', borderWidth: 2, tension: 0.2 },
            { label: 'Delhi Server (%)', data: [], borderColor: '#f59e0b', borderWidth: 2, tension: 0.2 },
            { label: 'Bangalore Server (%)', data: [], borderColor: '#8b5cf6', borderWidth: 2, tension: 0.2 }
          ]
        },
        options: {
          ...darkOpts,
          scales: { ...darkOpts.scales, y: { ...darkOpts.scales.y, min: 0, max: 100 } }
        }
      });
    }

    // 3. Latency & Cost Chart
    const ctxLatency = document.getElementById('liveLatencyChart');
    if (ctxLatency) {
      this.charts.latency = new Chart(ctxLatency, {
        type: 'line',
        data: {
          labels: [],
          datasets: [
            { label: 'Avg Latency (ms)', data: [], borderColor: '#60a5fa', tension: 0.2, borderWidth: 2 }
          ]
        },
        options: darkOpts
      });
    }
  },

  updateCharts(data) {
    const timeLabel = `T+${data.sim_time_sec}s`;
    const demand = data.total_demand || 0;
    const edge = data.routing ? data.routing.edgeTrafficRouted : demand;
    const origin = data.routing ? data.routing.originTrafficRouted : 0;
    const lat = data.routing ? data.routing.averageLatencyMs : 0;

    // Shift window
    if (this.timeSeries.labels.length >= this.maxDataPoints) {
      this.timeSeries.labels.shift();
      this.timeSeries.demand.shift();
      this.timeSeries.edgeFlow.shift();
      this.timeSeries.originFlow.shift();
      this.timeSeries.latency.shift();
      this.timeSeries.serverUtil.pune.shift();
      this.timeSeries.serverUtil.mumbai.shift();
      this.timeSeries.serverUtil.delhi.shift();
      this.timeSeries.serverUtil.bangalore.shift();
    }

    this.timeSeries.labels.push(timeLabel);
    this.timeSeries.demand.push(demand);
    this.timeSeries.edgeFlow.push(edge);
    this.timeSeries.originFlow.push(origin);
    this.timeSeries.latency.push(lat);

    const srvUtil = (data.routing && data.routing.serverUtilPct) || {};
    this.timeSeries.serverUtil.pune.push(srvUtil['Pune Server'] || 0);
    this.timeSeries.serverUtil.mumbai.push(srvUtil['Mumbai Server'] || 0);
    this.timeSeries.serverUtil.delhi.push(srvUtil['Delhi Server'] || 0);
    this.timeSeries.serverUtil.bangalore.push(srvUtil['Bangalore Server'] || 0);

    // Update Chart.js instances
    if (this.charts.demand) {
      this.charts.demand.data.labels = this.timeSeries.labels;
      this.charts.demand.data.datasets[0].data = this.timeSeries.demand;
      this.charts.demand.data.datasets[1].data = this.timeSeries.edgeFlow;
      this.charts.demand.data.datasets[2].data = this.timeSeries.originFlow;
      this.charts.demand.update();
    }

    if (this.charts.server) {
      this.charts.server.data.labels = this.timeSeries.labels;
      this.charts.server.data.datasets[0].data = this.timeSeries.serverUtil.pune;
      this.charts.server.data.datasets[1].data = this.timeSeries.serverUtil.mumbai;
      this.charts.server.data.datasets[2].data = this.timeSeries.serverUtil.delhi;
      this.charts.server.data.datasets[3].data = this.timeSeries.serverUtil.bangalore;
      this.charts.server.update();
    }

    if (this.charts.latency) {
      this.charts.latency.data.labels = this.timeSeries.labels;
      this.charts.latency.data.datasets[0].data = this.timeSeries.latency;
      this.charts.latency.update();
    }
  },

  renderLiveTable(allocations) {
    const tbody = document.getElementById('liveAllocationsTableBody');
    if (!tbody || !allocations) return;

    const routes = allocations.filter(a => {
      const from = a.from || a.from_node || a.region_name || '';
      const to = a.to || a.to_node || a.server_name || '';
      return !from.startsWith('Source') && !to.startsWith('Sink') && (a.flow ?? a.allocated_traffic ?? 0) > 0;
    });

    tbody.innerHTML = routes.map(a => {
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
          <td class="mono">${typeof unitCost === 'number' ? unitCost.toFixed(2) : unitCost}</td>
          <td class="mono">${Math.round(totalCost).toLocaleString()}</td>
        </tr>
      `;
    }).join('');
  }
};

