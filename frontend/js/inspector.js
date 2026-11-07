/**
 * Routing Decision Inspector & Step-by-Step Demonstration Controller
 * Enables evaluators to pause the simulation, freeze network state,
 * inspect incoming requests, capacities, and step through genuine C++ algorithm calculations.
 */

import { ApiService } from './api.js';
import { GraphVisualizer } from './graph_visualizer.js';

export const Inspector = {
  currentSnapshot: null,
  currentTrace: null,
  currentStepIndex: 0,
  selectedRegion: 'Pune',
  autoPlayTimer: null,
  isPlaying: false,

  init() {
    this.bindEvents();
    GraphVisualizer.init();
  },

  bindEvents() {
    // 0. Region Pills Selector
    const regionPills = document.querySelectorAll('#inspectorRegionPills .region-pill');
    regionPills.forEach(pill => {
      pill.addEventListener('click', () => {
        const reg = pill.getAttribute('data-region');
        if (reg) {
          this.selectedRegion = reg;
          regionPills.forEach(p => p.classList.toggle('active', p === pill));
          this.renderCandidates();
        }
      });
    });

    // 1. Tick Selector
    const tickSelect = document.getElementById('inspectorTickSelect');
    if (tickSelect) {
      tickSelect.addEventListener('change', (e) => {
        const val = e.target.value;
        if (val === 'latest') {
          this.loadSnapshot(null);
        } else {
          this.loadSnapshot(parseInt(val, 10));
        }
      });
    }

    // 2. Replay Strategy Selector & Button
    const btnReplay = document.getElementById('btnInspectorReplay');
    if (btnReplay) {
      btnReplay.addEventListener('click', () => this.handleReplay());
    }

    // 3. Step Controls
    const btnPrev = document.getElementById('btnTracePrev');
    if (btnPrev) {
      btnPrev.addEventListener('click', () => this.prevStep());
    }

    const btnNext = document.getElementById('btnTraceNext');
    if (btnNext) {
      btnNext.addEventListener('click', () => this.nextStep());
    }

    const btnStart = document.getElementById('btnTraceStart');
    if (btnStart) {
      btnStart.addEventListener('click', () => this.jumpToStep(0));
    }

    const btnEnd = document.getElementById('btnTraceEnd');
    if (btnEnd) {
      btnEnd.addEventListener('click', () => {
        if (this.currentTrace && this.currentTrace.steps) {
          this.jumpToStep(this.currentTrace.steps.length - 1);
        }
      });
    }

    const btnPlay = document.getElementById('btnTracePlay');
    if (btnPlay) {
      btnPlay.addEventListener('click', () => this.toggleAutoPlay());
    }

    // 4. Refresh Button
    const btnRefresh = document.getElementById('btnRefreshInspector');
    if (btnRefresh) {
      btnRefresh.addEventListener('click', () => {
        this.refresh();
        if (window.showToast) window.showToast('Inspector refreshed with latest simulation snapshot.', 'info');
      });
    }

    // Keyboard shortcuts for stepper: Left / Right arrows
    document.addEventListener('keydown', (e) => {
      const inspectorView = document.getElementById('view-inspector');
      if (inspectorView && inspectorView.classList.contains('active')) {
        if (e.key === 'ArrowRight') {
          e.preventDefault();
          this.nextStep();
        } else if (e.key === 'ArrowLeft') {
          e.preventDefault();
          this.prevStep();
        }
      }
    });
  },

  async refresh() {
    await this.refreshHistoryDropdown();
    await this.loadSnapshot(null);
    this.fitGraph();
  },

  fitGraph() {
    if (GraphVisualizer && GraphVisualizer.fitToScreen) {
      setTimeout(() => GraphVisualizer.fitToScreen(), 80);
    }
  },

  async refreshHistoryDropdown() {
    try {
      const history = await ApiService.getInspectorHistory();
      const tickSelect = document.getElementById('inspectorTickSelect');
      if (!tickSelect) return;

      const currentVal = tickSelect.value;
      tickSelect.innerHTML = '<option value="latest">Latest Frozen Tick</option>';

      // Render descending order (most recent first)
      for (let i = history.length - 1; i >= 0; --i) {
        const h = history[i];
        const opt = document.createElement('option');
        opt.value = h.tick;
        const outText = h.failed_servers && h.failed_servers.length > 0 ? ` [${h.failed_servers.join(',')}]` : '';
        const congText = h.has_congestion ? ' [Congested]' : '';
        opt.textContent = `Tick #${h.tick} (${h.total_demand.toLocaleString()} req/s - ${h.strategy})${outText}${congText}`;
        tickSelect.appendChild(opt);
      }

      if (currentVal && currentVal !== 'latest') {
        tickSelect.value = currentVal;
      }
    } catch (e) {
      console.warn('Failed to load history dropdown:', e);
    }
  },

  async loadSnapshot(tick = null) {
    this.stopAutoPlay();
    const loadingElem = document.getElementById('inspectorLoading');
    if (loadingElem) loadingElem.style.display = 'flex';

    try {
      let data;
      if (tick !== null) {
        data = await ApiService.getInspectorSnapshot(tick);
      } else {
        data = await ApiService.getInspectorLatest();
      }

      this.currentSnapshot = data.snapshot;
      this.currentTrace = data.trace;
      this.currentStepIndex = 0;

      // Load into Interactive Graph Execution Visualizer
      GraphVisualizer.loadTrace(this.currentTrace);

      // Update Header Info
      this.renderHeader();

      // Render Network & Ingress Snapshot
      this.renderSnapshot();

      // Render Region Candidate Investigator Matrix
      this.renderCandidates();

      // Render Step Timeline list
      this.renderTimeline();

      // Render Current Step
      this.renderStep(0);

      // Render Final Decision Table
      this.renderFinalDecision();

      // Sync Replay Strategy Select
      const stratSelect = document.getElementById('inspectorReplayStrategy');
      if (stratSelect && this.currentSnapshot) {
        stratSelect.value = this.currentSnapshot.strategy || 'Min-Cost Max-Flow';
      }
    } catch (err) {
      console.error('Failed to load snapshot in inspector:', err);
      if (window.showToast) window.showToast('Could not load snapshot: ' + err.message, 'error');
    } finally {
      if (loadingElem) loadingElem.style.display = 'none';
    }
  },

  async handleReplay() {
    const stratSelect = document.getElementById('inspectorReplayStrategy');
    if (!stratSelect || !this.currentSnapshot) return;

    const strategy = stratSelect.value;
    const tick = this.currentSnapshot.tick;

    if (window.showToast) window.showToast(`Replaying Tick #${tick} with ${strategy}...`, 'info');

    try {
      const data = await ApiService.replayInspector(
        tick,
        strategy,
        this.currentSnapshot.demands,
        this.currentSnapshot.failed_servers,
        this.currentSnapshot.congestion_spec
      );

      this.currentTrace = data.trace;
      this.currentStepIndex = 0;
      this.currentSnapshot.strategy = strategy;
      this.currentSnapshot.routing = data.routing;

      // Load into Interactive Graph Execution Visualizer
      GraphVisualizer.loadTrace(this.currentTrace);

      this.renderHeader();
      this.renderTimeline();
      this.renderCandidates();
      this.renderStep(0);
      this.renderFinalDecision();

      if (window.showToast) window.showToast(`Replay complete! Generated ${data.trace.totalSteps} execution steps.`, 'success');
    } catch (err) {
      console.error('Replay error:', err);
      if (window.showToast) window.showToast('Replay failed: ' + err.message, 'error');
    }
  },

  renderHeader() {
    if (!this.currentSnapshot) return;

    const tickBadge = document.getElementById('inspectorTickBadge');
    if (tickBadge) tickBadge.textContent = `Tick #${this.currentSnapshot.tick}`;

    const stratBadge = document.getElementById('inspectorStrategyBadge');
    if (stratBadge) stratBadge.textContent = this.currentSnapshot.strategy || 'Min-Cost Max-Flow';

    const reasonBadge = document.getElementById('inspectorReasonBadge');
    if (reasonBadge) {
      const r = this.currentSnapshot.decision_reason || 'NORMAL';
      reasonBadge.textContent = r;
      if (r.includes('FAILURE')) {
        reasonBadge.className = 'badge badge-rose';
      } else if (r.includes('RECOVERY')) {
        reasonBadge.className = 'badge badge-emerald';
      } else if (r.includes('CONGESTION') || r.includes('SPIKE')) {
        reasonBadge.className = 'badge badge-amber';
      } else {
        reasonBadge.className = 'badge badge-blue';
      }
    }

    const timeDisplay = document.getElementById('inspectorTimeDisplay');
    if (timeDisplay) {
      const s = this.currentSnapshot.sim_time_sec || 0;
      const m = Math.floor(s / 60).toString().padStart(2, '0');
      const sec = (s % 60).toString().padStart(2, '0');
      timeDisplay.textContent = `${m}:${sec} elapsed`;
    }
  },

  renderSnapshot() {
    if (!this.currentSnapshot) return;

    // A. Regional Incoming Demand
    const demandList = document.getElementById('inspectorDemandsList');
    if (demandList) {
      demandList.innerHTML = '';
      const demands = this.currentSnapshot.demands || {};
      const total = this.currentSnapshot.total_demand || Object.values(demands).reduce((a, b) => a + b, 0);

      for (const [reg, dem] of Object.entries(demands)) {
        const pct = total > 0 ? ((dem / total) * 100).toFixed(1) : 0;
        const item = document.createElement('div');
        item.style.marginBottom = '8px';
        item.innerHTML = `
          <div style="display:flex; justify-content:space-between; font-size:0.8rem; margin-bottom:3px;">
            <span style="font-weight:600; color:var(--text-primary);">${reg}</span>
            <span style="font-family:var(--font-mono); color:var(--accent-blue);">${dem.toLocaleString()} req/s (${pct}%)</span>
          </div>
          <div class="progress-bar">
            <div class="progress-fill" style="width: ${pct}%; background:var(--accent-blue);"></div>
          </div>
        `;
        demandList.appendChild(item);
      }

      const totalElem = document.getElementById('inspectorTotalDemand');
      if (totalElem) totalElem.textContent = `${total.toLocaleString()} req/s`;
    }

    // B. Server Health & Capacities
    const serverList = document.getElementById('inspectorServersList');
    if (serverList) {
      serverList.innerHTML = '';
      const failed = new Set(this.currentSnapshot.failed_servers || []);
      const serverLoads = (this.currentSnapshot.routing && this.currentSnapshot.routing.serverLoads) || {};
      const serverUtils = (this.currentSnapshot.routing && this.currentSnapshot.routing.serverUtilPct) || {};

      // Derive servers dynamically from snapshot topology
      let serversConfig = [];
      const candsMap = this.currentSnapshot.candidates_by_region || {};
      const firstReg = Object.keys(candsMap)[0];
      if (firstReg && candsMap[firstReg]) {
        serversConfig = candsMap[firstReg]
          .filter(c => !c.is_origin)
          .map(c => ({ name: c.server, cap: c.server_total_capacity }));
      }
      if (serversConfig.length === 0) {
        serversConfig = Object.keys(serverLoads).filter(k => k !== 'Origin Server').map(k => ({
          name: k,
          cap: k.includes('Mumbai') ? 6000 : (k.includes('Delhi') ? 4500 : 5000)
        }));
      }

      for (const s of serversConfig) {
        const isOffline = failed.has(s.name);
        const load = serverLoads[s.name] || 0;
        const util = serverUtils[s.name] || (isOffline ? 0 : (load / s.cap * 100));

        const item = document.createElement('div');
        item.style.marginBottom = '8px';
        item.innerHTML = `
          <div style="display:flex; justify-content:space-between; align-items:center; font-size:0.8rem; margin-bottom:3px;">
            <span style="font-weight:600; color:var(--text-primary);">${s.name}</span>
            <span class="badge ${isOffline ? 'badge-rose' : 'badge-emerald'}" style="font-size:0.65rem; padding:1px 6px;">
              ${isOffline ? 'FAILED / OFFLINE' : 'ACTIVE ONLINE'}
            </span>
          </div>
          <div style="display:flex; justify-content:space-between; font-size:0.75rem; color:var(--text-muted); font-family:var(--font-mono); margin-bottom:2px;">
            <span>Load: ${Math.round(load).toLocaleString()} / ${s.cap.toLocaleString()} req/s</span>
            <span>${util.toFixed(1)}%</span>
          </div>
          <div class="progress-bar">
            <div class="progress-fill" style="width: ${Math.min(100, util)}%; background:${isOffline ? 'var(--accent-rose)' : (util > 85 ? 'var(--accent-amber)' : 'var(--accent-emerald)')};"></div>
          </div>
        `;
        serverList.appendChild(item);
      }
    }

    // C. Network Congestion & Link Status
    const congStatus = document.getElementById('inspectorCongestionStatus');
    if (congStatus) {
      const cong = this.currentSnapshot.congestion_spec || 'none';
      if (cong !== 'none') {
        congStatus.innerHTML = `<span class="badge badge-amber"><span class="badge-dot"></span>ACTIVE CONGESTION</span> <span style="font-family:var(--font-mono); color:var(--text-primary); margin-left:6px;">${cong}</span>`;
      } else {
        congStatus.innerHTML = `<span class="badge badge-emerald"><span class="badge-dot"></span>NOMINAL PROPAGATION (0 ms delay)</span>`;
      }
    }
  },

  renderCandidates() {
    if (!this.currentSnapshot) return;

    // Synchronize Region Highlight in SVG Graph Visualizer
    GraphVisualizer.highlightRegion(this.selectedRegion);

    const candidatesMap = this.currentSnapshot.candidates_by_region || {};
    const regionCandidates = candidatesMap[this.selectedRegion] || [];
    const demands = this.currentSnapshot.demands || {};
    const regionDemand = demands[this.selectedRegion] || 0;

    // 1. Render Banner
    const banner = document.getElementById('inspectorRegionBanner');
    if (banner) {
      const edgeRouted = regionCandidates
        .filter(c => !c.is_origin)
        .reduce((sum, c) => sum + (c.flow_allocated || 0), 0);
      const originRouted = regionCandidates
        .filter(c => c.is_origin)
        .reduce((sum, c) => sum + (c.flow_allocated || 0), 0);
      const edgePct = regionDemand > 0 ? ((edgeRouted / regionDemand) * 100).toFixed(1) : 0;
      const originPct = regionDemand > 0 ? ((originRouted / regionDemand) * 100).toFixed(1) : 0;

      banner.innerHTML = `
        <div style="display:flex; align-items:center; gap:12px;">
          <div style="width:34px; height:34px; border-radius:var(--radius-sm); background:rgba(37,99,235,0.12); display:flex; align-items:center; justify-content:center; color:var(--accent-blue); font-weight:700; font-family:var(--font-mono); border:1px solid rgba(37,99,235,0.25);">
            ${this.selectedRegion.substring(0, 2).toUpperCase()}
          </div>
          <div>
            <div style="font-weight:700; font-size:0.95rem; color:var(--text-primary);">${this.selectedRegion} Ingress Traffic Profile</div>
            <div style="font-size:0.72rem; color:var(--text-muted); font-family:var(--font-mono);">Evaluating 4 Edge POP candidates + 1 Origin fallback</div>
          </div>
        </div>
        <div style="display:flex; align-items:center; gap:18px; flex-wrap:wrap;">
          <div>
            <div style="font-size:0.65rem; color:var(--text-muted); text-transform:uppercase;">Regional Demand</div>
            <div style="font-family:var(--font-mono); font-weight:700; font-size:0.92rem; color:var(--accent-blue);">${Math.round(regionDemand).toLocaleString()} req/s</div>
          </div>
          <div>
            <div style="font-size:0.65rem; color:var(--text-muted); text-transform:uppercase;">Edge Fleet Absorption</div>
            <div style="font-family:var(--font-mono); font-weight:700; font-size:0.92rem; color:var(--accent-emerald);">${Math.round(edgeRouted).toLocaleString()} req/s (${edgePct}%)</div>
          </div>
          <div>
            <div style="font-size:0.65rem; color:var(--text-muted); text-transform:uppercase;">Origin DC Spillover</div>
            <div style="font-family:var(--font-mono); font-weight:700; font-size:0.92rem; color:${originRouted > 0 ? 'var(--accent-rose)' : 'var(--text-secondary)'};">${Math.round(originRouted).toLocaleString()} req/s (${originPct}%)</div>
          </div>
        </div>
      `;
    }

    // 2. Render Prominent Decision Callout Box (Explains which server is chosen and why others were rejected)
    this.renderDecisionCallout(regionCandidates, regionDemand);

    // 3. Render Candidates Table Body
    const tbody = document.getElementById('inspectorCandidateTableBody');
    if (!tbody) return;
    tbody.innerHTML = '';

    if (regionCandidates.length === 0) {
      tbody.innerHTML = `<tr><td colspan="7" style="text-align:center; padding:16px; color:var(--text-muted);">No candidate servers found for ${this.selectedRegion}.</td></tr>`;
      return;
    }

    const winners = regionCandidates.filter(c => (c.flow_allocated || 0) > 0).sort((a, b) => b.flow_allocated - a.flow_allocated);
    const primary = winners[0] || regionCandidates[0];

    regionCandidates.forEach(cand => {
      const tr = document.createElement('tr');
      const isWinner = (cand.flow_allocated > 0);
      if (isWinner) {
        tr.style.background = 'rgba(16, 185, 129, 0.08)';
        tr.style.borderLeft = '4px solid var(--accent-emerald)';
      }

      tr.style.cursor = 'pointer';
      tr.setAttribute('title', `Click to inspect and visualize route path to ${cand.server} on graph`);
      tr.addEventListener('click', () => {
        tbody.querySelectorAll('tr').forEach(r => r.classList.remove('selected-candidate-row'));
        tr.classList.add('selected-candidate-row');
        GraphVisualizer.highlightCandidatePath(this.selectedRegion, cand.server, cand);
      });

      // Column 1: Candidate Server & Route Status
      let serverBadge = '';
      if (isWinner) {
        serverBadge = `
          <div style="display:flex; align-items:center; gap:6px; margin-bottom:2px;">
            <span class="badge badge-emerald" style="font-weight:700; font-size:0.68rem; padding:2px 7px;">
              <svg width="11" height="11" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="3" style="margin-right:2px; vertical-align:-1px;"><polyline points="20 6 9 17 4 12"/></svg>
              CHOSEN ROUTE
            </span>
            <span style="font-weight:700; font-size:0.86rem; color:var(--text-primary);">${cand.server}</span>
          </div>
          <div style="font-size:0.75rem; color:var(--accent-emerald); font-weight:700; font-family:var(--font-mono); margin-left:2px;">
            ${Math.round(cand.flow_allocated).toLocaleString()} req/s (${cand.allocation_pct.toFixed(1)}% routed)
          </div>
        `;
      } else if (cand.is_origin) {
        serverBadge = `
          <div style="display:flex; align-items:center; gap:6px; margin-bottom:2px;">
            <span class="badge" style="background:rgba(239,68,68,0.12); color:#f87171; border:1px solid rgba(239,68,68,0.3); font-size:0.65rem; padding:2px 6px;">
              ORIGIN (STANDBY)
            </span>
            <span style="font-weight:600; font-size:0.84rem; color:var(--text-secondary);">${cand.server}</span>
          </div>
          <div style="font-size:0.7rem; color:var(--text-muted); font-family:var(--font-mono); margin-left:2px;">
            ${cand.flow_allocated > 0 ? `Spillover Active: ${Math.round(cand.flow_allocated).toLocaleString()} req/s` : 'Zero Spillover (0 req/s — In Reserve)'}
          </div>
        `;
      } else {
        serverBadge = `
          <div style="display:flex; align-items:center; gap:6px; margin-bottom:2px;">
            <span class="badge" style="background:rgba(255,255,255,0.04); color:var(--text-muted); border:1px solid var(--border-subtle); font-size:0.65rem; padding:2px 6px;">
              ${!cand.server_healthy ? 'OFFLINE' : 'STANDBY'}
            </span>
            <span style="font-weight:600; font-size:0.84rem; color:${cand.server_healthy ? 'var(--text-primary)' : 'var(--text-muted)'};">${cand.server}</span>
          </div>
          <div style="font-size:0.7rem; color:var(--text-muted); font-family:var(--font-mono); margin-left:2px;">
            ${!cand.server_healthy ? 'Hardware Outage' : '0 req/s (Not Selected)'}
          </div>
        `;
      }

      // Column 2: Latency (SLA)
      const congText = cand.link_congestion_ms > 0 ? `<div style="color:var(--accent-rose); font-size:0.68rem; font-family:var(--font-mono);">(+${cand.link_congestion_ms.toFixed(0)}ms cong)</div>` : '';
      const latencyCell = `
        <div style="font-family:var(--font-mono); font-weight:700; font-size:0.82rem; color:${!cand.server_healthy ? 'var(--accent-rose)' : (isWinner ? 'var(--accent-emerald)' : 'var(--text-primary)')};">
          ${!cand.server_healthy ? 'OFFLINE' : `${cand.link_effective_latency_ms.toFixed(1)} ms`}
        </div>
        <div style="font-size:0.68rem; color:var(--text-muted); font-family:var(--font-mono);">Base: ${cand.link_base_latency_ms.toFixed(1)} ms</div>
        ${congText}
      `;

      // Column 3: Transit Link (Flow / Capacity)
      const isUnlimitedBw = cand.link_bandwidth_capacity > 90000;
      const capText = isUnlimitedBw ? '∞ (Unlimited)' : `${Math.round(cand.link_bandwidth_capacity).toLocaleString()} req/s`;
      let flowText = '';
      let remText = '';
      if (isUnlimitedBw) {
        flowText = cand.flow_allocated > 0 
          ? `<span style="color:var(--accent-rose); font-weight:700;">Flow: ${Math.round(cand.flow_allocated).toLocaleString()} req/s</span>`
          : `<span style="color:var(--text-muted);">Flow: 0 req/s routed</span>`;
        remText = `<span style="color:var(--text-muted);">Unlimited fallback headroom</span>`;
      } else {
        flowText = cand.flow_allocated > 0 
          ? `<span style="color:var(--accent-blue); font-weight:700;">Flow: ${Math.round(cand.flow_allocated).toLocaleString()} req/s (${cand.link_utilization_pct.toFixed(0)}% link load)</span>`
          : `<span style="color:var(--text-muted);">Flow: 0 req/s routed (0% link load)</span>`;
        remText = cand.flow_allocated > 0 
          ? `<span style="color:var(--accent-emerald);">Free: ${Math.round(cand.link_remaining_bandwidth).toLocaleString()} req/s available</span>`
          : `<span style="color:var(--text-muted);">Free: ${Math.round(cand.link_bandwidth_capacity).toLocaleString()} req/s (100% available)</span>`;
      }

      const linkCell = `
        <div style="font-family:var(--font-mono); font-size:0.8rem; font-weight:700; color:var(--text-primary);">
          Cap: ${capText}
        </div>
        <div style="font-size:0.72rem; font-family:var(--font-mono); margin-top:2px;">
          ${flowText}
        </div>
        <div style="font-size:0.68rem; font-family:var(--font-mono);">
          ${remText}
        </div>
      `;

      // Column 4: Fleet Load & Cap
      const fleetLoadCell = cand.is_origin ? `
        <div style="font-family:var(--font-mono); font-size:0.78rem; color:var(--text-muted);">Unlimited Cap</div>
        <div style="font-size:0.68rem; color:var(--text-muted);">Fallback Tier</div>
      ` : `
        <div style="font-family:var(--font-mono); font-size:0.78rem; color:var(--text-primary);">
          ${Math.round(cand.server_existing_load).toLocaleString()} / ${Math.round(cand.server_total_capacity).toLocaleString()} req/s
        </div>
        <div style="font-size:0.68rem; color:${cand.server_utilization_pct > 80 ? 'var(--accent-amber)' : 'var(--text-muted)'}; font-family:var(--font-mono);">
          Fleet Load: ${cand.server_utilization_pct.toFixed(1)}%
        </div>
      `;

      // Column 5: Cache Hit
      const cacheCell = `
        <div style="font-family:var(--font-mono); font-weight:700; font-size:0.82rem; color:${cand.cache_hit_ratio >= 0.8 ? 'var(--accent-blue)' : 'var(--text-secondary)'};">
          ${cand.cache_hit_ratio > 0 ? `${(cand.cache_hit_ratio * 100).toFixed(0)}%` : '0%'}
        </div>
      `;

      // Column 6: Unit Cost ($/req)
      const formulaStr = (cand.cost_formula && cand.cost_formula.formula_string) || `C = ${cand.cost_formula?.unit_cost || 0}`;
      const costCell = `
        <div style="font-family:var(--font-mono); font-weight:700; font-size:0.85rem; color:${isWinner ? 'var(--accent-emerald)' : 'var(--accent-blue)'};">
          $${cand.cost_formula?.unit_cost?.toFixed(2) || '0.00'} / req
        </div>
        <div class="formula-display-pill" style="font-size:0.65rem; max-width:200px; overflow:hidden; text-overflow:ellipsis;" title="${formulaStr}">
          ${formulaStr}
        </div>
      `;

      // Column 7: Algorithm Decision & Reason
      let decisionReason = '';
      if (!cand.server_healthy) {
        decisionReason = `<span class="badge badge-rose" style="font-size:0.65rem;">EXCLUDED</span> <div style="font-size:0.7rem; color:#f87171; margin-top:2px;">Offline due to hardware outage.</div>`;
      } else if (isWinner) {
        decisionReason = `<span class="badge badge-emerald" style="font-size:0.65rem;">SELECTED</span> <div style="font-size:0.7rem; color:var(--accent-emerald); font-weight:600; margin-top:2px;">Global minimum cost ($${cand.cost_formula?.unit_cost?.toFixed(2)}) & lowest latency (${cand.link_effective_latency_ms.toFixed(1)}ms).</div>`;
      } else if (cand.is_origin) {
        decisionReason = `<span class="badge badge-purple" style="font-size:0.65rem;">IN RESERVE</span> <div style="font-size:0.7rem; color:var(--text-muted); margin-top:2px;">Zero spillover (0 req/s); edge POP capacity was sufficient.</div>`;
      } else {
        const costDiff = (cand.cost_formula?.unit_cost || 0) - (primary.cost_formula?.unit_cost || 0);
        const latDiff = cand.link_effective_latency_ms - (primary.link_effective_latency_ms || 0);
        decisionReason = `<span class="badge badge-outline" style="font-size:0.65rem; color:var(--text-muted);">STANDBY</span> <div style="font-size:0.7rem; color:var(--text-muted); margin-top:2px;">Suboptimal: +${latDiff > 0 ? latDiff.toFixed(1) : 0}ms latency, +$${costDiff > 0 ? costDiff.toFixed(2) : '0.00'}/req cost.</div>`;
      }

      tr.innerHTML = `
        <td>${serverBadge}</td>
        <td>${latencyCell}</td>
        <td>${linkCell}</td>
        <td>${fleetLoadCell}</td>
        <td>${cacheCell}</td>
        <td>${costCell}</td>
        <td>${decisionReason}</td>
      `;

      tbody.appendChild(tr);
    });

    // Auto-select and visualize primary winning candidate on the graph
    const firstRow = tbody.querySelector('tr');
    if (firstRow) {
      firstRow.classList.add('selected-candidate-row');
      GraphVisualizer.highlightCandidatePath(this.selectedRegion, primary.server, primary);
    }

    // 4. Render In-Depth Decision Synthesis: Mathematical Proof & CDN System Flow
    this.renderDecisionSynthesis(regionCandidates, regionDemand);
  },

  renderDecisionCallout(regionCandidates, regionDemand) {
    const callout = document.getElementById('inspectorDecisionCallout');
    if (!callout) return;

    if (!regionCandidates || regionCandidates.length === 0) {
      callout.innerHTML = '';
      return;
    }

    const winners = regionCandidates.filter(c => (c.flow_allocated || 0) > 0).sort((a, b) => b.flow_allocated - a.flow_allocated);
    const nonWinners = regionCandidates.filter(c => (c.flow_allocated || 0) === 0);
    const primary = winners[0] || regionCandidates[0];
    const isMultiPop = winners.length > 1;
    const originCand = regionCandidates.find(c => c.is_origin);
    const originRouted = originCand ? (originCand.flow_allocated || 0) : 0;
    const originPct = regionDemand > 0 ? ((originRouted / regionDemand) * 100).toFixed(1) : '0.0';
    const strategy = this.currentSnapshot?.strategy || 'Min-Cost Max-Flow';
    const primaryCost = primary.cost_formula?.unit_cost || 0;
    const primaryLat = primary.link_effective_latency_ms || 0;
    const primaryCache = ((primary.cache_hit_ratio || 0) * 100).toFixed(0);

    // Build alternate candidates rejection breakdown (only for nodes with 0 flow)
    const altBreakdownHtml = nonWinners.map(cand => {
      if (!cand.server_healthy) {
        return `
          <div style="margin-bottom:5px; display:flex; align-items:flex-start; gap:8px;">
            <span class="badge badge-rose" style="font-size:0.65rem; padding:1px 6px; flex-shrink:0;">OFFLINE</span>
            <div>
              <strong style="color:var(--text-primary);">${cand.server}:</strong>
              <span style="color:#f87171;">Excluded from routing due to active hardware outage.</span>
            </div>
          </div>
        `;
      }
      if (cand.is_origin) {
        if (originRouted > 0) {
          return `
            <div style="margin-bottom:5px; display:flex; align-items:flex-start; gap:8px;">
              <span class="badge badge-amber" style="font-size:0.65rem; padding:1px 6px; flex-shrink:0;">SPILLOVER</span>
              <div>
                <strong style="color:var(--text-primary);">${cand.server}:</strong>
                <span style="color:var(--accent-amber);">Active overflow handling: ${Math.round(originRouted).toLocaleString()} req/s (${originPct}%) routed to central origin because edge POP capacity was saturated.</span>
              </div>
            </div>
          `;
        } else {
          return `
            <div style="margin-bottom:5px; display:flex; align-items:flex-start; gap:8px;">
              <span class="badge badge-purple" style="font-size:0.65rem; padding:1px 6px; flex-shrink:0;">NOT USED</span>
              <div>
                <strong style="color:var(--text-primary);">${cand.server}:</strong>
                <span style="color:var(--text-secondary);">Zero spillover (0 req/s — Standby Reserve). High penalty cost ($177.50/req, 100.0 ms) successfully avoided because edge POP had sufficient capacity.</span>
              </div>
            </div>
          `;
        }
      }

      const latDiff = cand.link_effective_latency_ms - primaryLat;
      const costDiff = (cand.cost_formula?.unit_cost || 0) - primaryCost;
      const isCongested = cand.link_congestion_ms > 0;
      return `
        <div style="margin-bottom:5px; display:flex; align-items:flex-start; gap:8px;">
          <span class="badge badge-outline" style="font-size:0.65rem; padding:1px 6px; flex-shrink:0; color:var(--text-muted);">STANDBY</span>
          <div>
            <strong style="color:var(--text-primary);">${cand.server}:</strong>
            <span style="color:var(--text-secondary);">
              Eligible (Link Cap: ${Math.round(cand.link_bandwidth_capacity).toLocaleString()} req/s, Fleet Load: ${cand.server_utilization_pct.toFixed(0)}%), but rejected as suboptimal due to
              <strong style="color:var(--text-primary);">+${latDiff > 0 ? latDiff.toFixed(1) : 0} ms</strong> latency penalty and
              <strong style="color:var(--accent-blue);">+$${costDiff > 0 ? costDiff.toFixed(2) : '0.00'}/req</strong> higher multi-factor cost ($${cand.cost_formula?.unit_cost?.toFixed(2)} vs $${primaryCost.toFixed(2)}).
              ${isCongested ? `<span style="color:var(--accent-rose);">[Link Congested: +${cand.link_congestion_ms.toFixed(0)}ms delay]</span>` : ''}
            </span>
          </div>
        </div>
      `;
    }).join('');

    // Dynamic header and narrative based on single vs multi-pop split
    let headerRouteText = '';
    let headerFlowText = '';
    let whyChosenNarrative = '';

    if (isMultiPop) {
      const routesStr = winners.map(w => `<span style="color:var(--accent-emerald);">${w.server} (${w.allocation_pct.toFixed(0)}%)</span>`).join(' + ');
      headerRouteText = `${this.selectedRegion} Ingress &rarr; ${routesStr}`;
      headerFlowText = `${Math.round(regionDemand).toLocaleString()} req/s (Balanced across ${winners.length} Edge POPs)`;

      const breakdownDetails = winners.map(w => `<strong>${w.server}</strong> received ${Math.round(w.flow_allocated).toLocaleString()} req/s (${w.allocation_pct.toFixed(1)}%, Cost: $${w.cost_formula?.unit_cost?.toFixed(2)}/req)`).join(' and ');
      whyChosenNarrative = `
        Traffic was dynamically divided across multiple edge nodes: ${breakdownDetails}.
        <strong>Min-Cost Max-Flow Dijkstra with Johnson Potentials</strong> computed that after saturating ${primary.server}'s lowest-cost load tier, routing the remaining volume to the next closest POP minimizes overall fleet latency and cost without incurring queue delay or congestion.
      `;
    } else {
      headerRouteText = `${this.selectedRegion} Ingress &rarr; <span style="color:var(--accent-emerald);">${primary.server}</span>`;
      headerFlowText = `${Math.round(primary.flow_allocated).toLocaleString()} req/s (${primary.allocation_pct.toFixed(1)}% of Regional Demand)`;
      whyChosenNarrative = `
        Selected as the single global mathematical optimum because it minimizes unit routing cost <code>C = αL + βB + (1-H)M</code> ($${primaryCost.toFixed(2)} / req) while providing minimal propagation delay (${primaryLat.toFixed(1)} ms) and high edge cache hit ratio (${primaryCache}%). The dedicated transit link has ${Math.round(primary.link_bandwidth_capacity).toLocaleString()} req/s capacity with ${Math.round(primary.link_remaining_bandwidth).toLocaleString()} req/s remaining headroom, ensuring zero congestion packet loss.
      `;
    }

    callout.innerHTML = `
      <div style="background: rgba(15, 23, 42, 0.95); border: 1px solid rgba(16, 185, 129, 0.35); border-left: 5px solid var(--accent-emerald); border-radius: var(--radius-sm); padding: 14px 18px; box-shadow: var(--shadow-md);">
        <!-- Callout Header -->
        <div style="display:flex; justify-content:space-between; align-items:center; flex-wrap:wrap; gap:12px; margin-bottom:12px; padding-bottom:10px; border-bottom:1px solid rgba(255,255,255,0.08);">
          <div style="display:flex; align-items:center; gap:10px;">
            <span class="badge badge-emerald" style="font-size:0.75rem; font-weight:700; padding:3px 9px;">
              <svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="3" style="margin-right:4px; vertical-align:-1px;"><polyline points="20 6 9 17 4 12"/></svg>
              ${isMultiPop ? 'OPTIMAL MULTI-POP ROUTE' : 'OPTIMAL ROUTE SELECTED'}
            </span>
            <span style="font-size:1.05rem; font-weight:700; color:var(--text-primary);">
              ${headerRouteText}
            </span>
          </div>
          <div style="font-family:var(--font-mono); font-size:0.88rem; font-weight:700; color:var(--accent-emerald);">
            ${headerFlowText}
          </div>
        </div>

        <!-- 4-Stat Comparison Tiles -->
        <div style="display:grid; grid-template-columns: repeat(auto-fit, minmax(180px, 1fr)); gap:10px; margin-bottom:12px;">
          <div style="background:rgba(255,255,255,0.025); border:1px solid var(--border-subtle); border-radius:4px; padding:8px 12px;">
            <div style="font-size:0.65rem; color:var(--text-muted); text-transform:uppercase; font-weight:600;">Central Origin Status</div>
            <div style="font-family:var(--font-mono); font-weight:700; font-size:0.85rem; color:${originRouted > 0 ? 'var(--accent-amber)' : 'var(--accent-emerald)'}; margin-top:2px;">
              ${originRouted > 0 ? `SPILLOVER: ${Math.round(originRouted).toLocaleString()} req/s (${originPct}%)` : `ORIGIN NOT USED (0 req/s — In Reserve)`}
            </div>
          </div>
          <div style="background:rgba(255,255,255,0.025); border:1px solid var(--border-subtle); border-radius:4px; padding:8px 12px;">
            <div style="font-size:0.65rem; color:var(--text-muted); text-transform:uppercase; font-weight:600;">Cost Advantage</div>
            <div style="font-family:var(--font-mono); font-weight:700; font-size:0.85rem; color:var(--accent-blue); margin-top:2px;">
              $${primaryCost.toFixed(2)} / req <span style="font-size:0.7rem; color:var(--text-muted);">(Saves $${(177.50 - primaryCost).toFixed(2)} vs Origin)</span>
            </div>
          </div>
          <div style="background:rgba(255,255,255,0.025); border:1px solid var(--border-subtle); border-radius:4px; padding:8px 12px;">
            <div style="font-size:0.65rem; color:var(--text-muted); text-transform:uppercase; font-weight:600;">Latency SLA</div>
            <div style="font-family:var(--font-mono); font-weight:700; font-size:0.85rem; color:var(--accent-emerald); margin-top:2px;">
              ${primaryLat.toFixed(1)} ms <span style="font-size:0.7rem; color:var(--text-muted);">(${(100.0 - primaryLat).toFixed(1)} ms faster than Origin)</span>
            </div>
          </div>
          <div style="background:rgba(255,255,255,0.025); border:1px solid var(--border-subtle); border-radius:4px; padding:8px 12px;">
            <div style="font-size:0.65rem; color:var(--text-muted); text-transform:uppercase; font-weight:600;">Link Bandwidth Headroom</div>
            <div style="font-family:var(--font-mono); font-weight:700; font-size:0.85rem; color:var(--text-primary); margin-top:2px;">
              ${Math.round(primary.link_remaining_bandwidth).toLocaleString()} req/s free <span style="font-size:0.7rem; color:var(--accent-emerald);">(${(100 - primary.link_utilization_pct).toFixed(0)}% available)</span>
            </div>
          </div>
        </div>

        <!-- Explanatory Narrative: Why Chosen vs Why Rejected -->
        <div style="background:rgba(255,255,255,0.02); border:1px solid var(--border-subtle); border-radius:4px; padding:10px 14px; font-size:0.76rem; line-height:1.5;">
          <div style="margin-bottom:8px;">
            <div style="font-weight:700; color:var(--accent-emerald); margin-bottom:3px; display:flex; align-items:center; gap:6px;">
              <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5"><polyline points="20 6 9 17 4 12"/></svg>
              Why Chosen by ${strategy}:
            </div>
            <div style="color:var(--text-secondary); margin-left:20px;">
              ${whyChosenNarrative}
            </div>
          </div>

          <div>
            <div style="font-weight:700; color:var(--text-muted); margin-bottom:4px; display:flex; align-items:center; gap:6px;">
              <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><circle cx="12" cy="12" r="10"/><line x1="15" y1="9" x2="9" y2="15"/><line x1="9" y1="9" x2="15" y2="15"/></svg>
              Why Alternate Candidates Were Not Chosen:
            </div>
            <div style="color:var(--text-muted); margin-left:20px;">
              ${altBreakdownHtml}
            </div>
          </div>
        </div>
      </div>
    `;
  },

  renderDecisionSynthesis(candidates, regionDemand) {
    const container = document.getElementById('inspectorDecisionSynthesis');
    if (!container) return;

    if (!candidates || candidates.length === 0) {
      container.innerHTML = '';
      return;
    }

    const winners = candidates.filter(c => (c.flow_allocated || 0) > 0).sort((a, b) => b.flow_allocated - a.flow_allocated);
    const nonWinners = candidates.filter(c => (c.flow_allocated || 0) === 0);
    const primary = winners[0] || candidates[0];
    const strategy = this.currentSnapshot?.strategy || 'Min-Cost Max-Flow';
    const isMCMF = strategy.toLowerCase().includes('mcmf') || strategy.toLowerCase().includes('min-cost');
    const isNearest = strategy.toLowerCase().includes('nearest');
    const isRR = strategy.toLowerCase().includes('round');
    const isGreedy = strategy.toLowerCase().includes('greedy');

    const primaryCost = primary.cost_formula?.unit_cost || 0;
    const primaryLat = primary.link_effective_latency_ms || 0;
    const primaryCache = ((primary.cache_hit_ratio || 0) * 100).toFixed(0);

    // Compute alternative averages & minimums for comparison
    const eligibleAlts = nonWinners.filter(c => c.server_healthy && !c.is_origin);
    const lowestAltCost = eligibleAlts.length > 0 ? Math.min(...eligibleAlts.map(c => c.cost_formula?.unit_cost || 999)) : null;
    const lowestAltLat = eligibleAlts.length > 0 ? Math.min(...eligibleAlts.map(c => c.link_effective_latency_ms || 999)) : null;

    // Build Card 1: Real CDN System Flow & Operational Rationale
    const systemCardHtml = `
      <div class="synthesis-card card-system">
        <div class="synthesis-header">
          <div class="synthesis-title">
            <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="var(--accent-emerald)" stroke-width="2"><rect x="2" y="2" width="20" height="8" rx="2"/><rect x="2" y="14" width="20" height="8" rx="2"/><line x1="6" y1="6" x2="6.01" y2="6"/><line x1="6" y1="18" x2="6.01" y2="18"/></svg>
            CDN Operational & Infrastructure Rationale
          </div>
          <span class="factor-tag tag-emerald">System Operations</span>
        </div>

        <div class="factor-item">
          <span class="factor-tag tag-emerald">Ingress Demand</span>
          <div>
            <strong>${this.selectedRegion}</strong> received <span style="font-family:var(--font-mono); color:var(--accent-blue);">${Math.round(regionDemand).toLocaleString()} req/s</span>.
            Traffic dispatched to <strong>${primary.server}</strong> (${Math.round(primary.flow_allocated || 0).toLocaleString()} req/s, ${primary.allocation_pct.toFixed(1)}% share) to achieve maximum edge termination.
          </div>
        </div>

        <div class="factor-item">
          <span class="factor-tag tag-emerald">Proximity SLA</span>
          <div>
            Direct fiber hop provides <span style="font-family:var(--font-mono); color:var(--accent-emerald);">${primaryLat.toFixed(1)} ms</span> latency.
            ${lowestAltLat ? `Avoids sub-optimal inter-city cross-transit (next closest available POP is ${lowestAltLat.toFixed(1)} ms, a +${(lowestAltLat - primaryLat).toFixed(1)} ms penalty).` : 'No alternate edge POPs available.'}
          </div>
        </div>

        <div class="factor-item">
          <span class="factor-tag tag-emerald">Link Capacity</span>
          <div>
            Allocated <span style="font-family:var(--font-mono);">${Math.round(primary.link_allocated_flow).toLocaleString()} req/s</span> on a <span style="font-family:var(--font-mono);">${primary.link_bandwidth_capacity > 90000 ? '∞' : Math.round(primary.link_bandwidth_capacity).toLocaleString()} req/s</span> link (${primary.link_utilization_pct.toFixed(0)}% link util).
            <span style="color:var(--text-secondary);">Leaves <span style="font-family:var(--font-mono);">${primary.link_remaining_bandwidth > 90000 ? '∞' : Math.round(primary.link_remaining_bandwidth).toLocaleString()} req/s</span> headroom for sudden burst spikes.</span>
          </div>
        </div>

        <div class="factor-item">
          <span class="factor-tag tag-emerald">Edge Cache</span>
          <div>
            <span style="font-family:var(--font-mono); color:var(--accent-blue); font-weight:700;">${primaryCache}%</span> cache hit ratio absorbs the majority of static content locally, preventing costly backhaul round-trips to the central origin data center.
          </div>
        </div>

        <div class="factor-item">
          <span class="factor-tag tag-emerald">Fleet Load</span>
          <div>
            <strong>${primary.server}</strong> operates at <span style="font-family:var(--font-mono); font-weight:700;">${primary.server_utilization_pct.toFixed(1)}%</span> fleet utilization (${Math.round(primary.server_existing_load).toLocaleString()} / ${Math.round(primary.server_total_capacity).toLocaleString()} req/s), well beneath saturation thresholds to prevent tail-latency spikes.
          </div>
        </div>
      </div>
    `;

    // Build Card 2: Mathematical Proof & Algorithmic Formulation
    let mathAlgoDetails = '';
    if (isMCMF) {
      mathAlgoDetails = `
        <div class="factor-item">
          <span class="factor-tag tag-blue">Optimization Goal</span>
          <div>
            Linear Program objective: <span style="font-family:var(--font-mono); color:var(--accent-blue);">min &Sigma; c(u,v) &middot; f(u,v)</span> subject to capacity constraints <span style="font-family:var(--font-mono);">0 &le; f(u,v) &le; u(u,v)</span> and flow conservation <span style="font-family:var(--font-mono);">&Sigma; f_in - &Sigma; f_out = b(v)</span>.
          </div>
        </div>
        <div class="factor-item">
          <span class="factor-tag tag-blue">Cost Function</span>
          <div>
            Multi-factor cost formula: <span style="font-family:var(--font-mono); color:var(--accent-blue);">C = &alpha;&middot;L + &beta;&middot;B + (1 - H)&middot;M</span> with &alpha;=1.0, &beta;=1.5, M=20.0.<br>
            Selected route unit cost: <span style="font-family:var(--font-mono); font-weight:700; color:var(--accent-emerald);">$${primaryCost.toFixed(2)}/req</span>.
            ${lowestAltCost ? `Closest alternative costs <span style="font-family:var(--font-mono);">$${lowestAltCost.toFixed(2)}/req</span> (&Delta;C = +$${(lowestAltCost - primaryCost).toFixed(2)}/req premium).` : ''}
          </div>
        </div>
        <div class="factor-item">
          <span class="factor-tag tag-blue">Johnson Dual Potentials</span>
          <div>
            Dual potentials <span style="font-family:var(--font-mono);">&pi;(v)</span> maintain non-negative reduced costs <span style="font-family:var(--font-mono); color:var(--accent-blue);">c<sup>&pi;</sup>(u,v) = c(u,v) + &pi;(u) - &pi;(v) &ge; 0</span>, enabling optimal successive shortest path augmentations via Dijkstra in <span style="font-family:var(--font-mono);">O(E log V)</span>.
          </div>
        </div>
        <div class="factor-item">
          <span class="factor-tag tag-blue">Augmenting Flow</span>
          <div>
            Pushed bottleneck flow <span style="font-family:var(--font-mono); color:var(--accent-emerald);">&Delta;f = min<sub>e&isin;P</sub> c<sub>f</sub>(e) = ${Math.round(primary.flow_allocated || 0).toLocaleString()} req/s</span> along residual path <span style="font-family:var(--font-mono); font-size:0.75rem;">S &rarr; ${this.selectedRegion} &rarr; ${primary.server} &rarr; T</span>.
          </div>
        </div>
      `;
    } else if (isNearest) {
      mathAlgoDetails = `
        <div class="factor-item">
          <span class="factor-tag tag-blue">Decision Rule</span>
          <div>
            Greedy Min-Latency rule: <span style="font-family:var(--font-mono); color:var(--accent-blue);">argmin<sub>s &isin; S_healthy</sub> L(region, s)</span>.
            Selected <strong>${primary.server}</strong> with strictly lowest transmission latency <span style="font-family:var(--font-mono); font-weight:700; color:var(--accent-emerald);">${primaryLat.toFixed(1)} ms</span>.
          </div>
        </div>
        <div class="factor-item">
          <span class="factor-tag tag-blue">Latency Comparison</span>
          <div>
            ${eligibleAlts.map(a => `<span style="font-family:var(--font-mono); font-size:0.75rem; margin-right:8px;">${a.server.replace(' Server','')}: ${a.link_effective_latency_ms.toFixed(1)}ms (+${(a.link_effective_latency_ms - primaryLat).toFixed(1)}ms)</span>`).join('')}
          </div>
        </div>
        <div class="factor-item">
          <span class="factor-tag tag-blue">Algorithmic Tradeoff</span>
          <div>
            Does not account for link bandwidth pricing or cache miss multipliers; minimizes pure geographical delay.
          </div>
        </div>
      `;
    } else if (isRR) {
      mathAlgoDetails = `
        <div class="factor-item">
          <span class="factor-tag tag-blue">Decision Rule</span>
          <div>
            Cyclic Round Robin index: <span style="font-family:var(--font-mono); color:var(--accent-blue);">Server_idx = (tick + region_offset) mod K</span>.
            Dispatches traffic in alternating rounds across healthy edge nodes to balance request counts symmetrically.
          </div>
        </div>
        <div class="factor-item">
          <span class="factor-tag tag-blue">Cost / Latency Agnostic</span>
          <div>
            No cost minimization or latency optimization performed; assigns identical unit weights irrespective of link distance or cache efficiency.
          </div>
        </div>
      `;
    } else { // Greedy Cost
      mathAlgoDetails = `
        <div class="factor-item">
          <span class="factor-tag tag-blue">Decision Rule</span>
          <div>
            Greedy Cost Minimization: <span style="font-family:var(--font-mono); color:var(--accent-blue);">argmin<sub>s &isin; S_available</sub> C(s)</span>.
            Sequentially exhausts the lowest unit cost server (<span style="font-family:var(--font-mono); color:var(--accent-emerald);">$${primaryCost.toFixed(2)}/req</span>) up to capacity before overflowing to secondary servers.
          </div>
        </div>
        <div class="factor-item">
          <span class="factor-tag tag-blue">Algorithmic Limitation</span>
          <div>
            Lacks global max-flow coordinate awareness; can lead to myopic decisions where low-cost capacity is exhausted before high-priority regional flows arrive.
          </div>
        </div>
      `;
    }

    const mathCardHtml = `
      <div class="synthesis-card card-math">
        <div class="synthesis-header">
          <div class="synthesis-title">
            <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="var(--accent-blue)" stroke-width="2"><path d="M4 19.5A2.5 2.5 0 0 1 6.5 17H20"/><path d="M6.5 2H20v20H6.5A2.5 2.5 0 0 1 4 19.5v-15A2.5 2.5 0 0 1 6.5 2z"/></svg>
            Mathematical Formulation & Algorithm Proof (${strategy})
          </div>
          <span class="factor-tag tag-blue">Algorithmic Proof</span>
        </div>
        ${mathAlgoDetails}
      </div>
    `;

    // Build Card 3: Alternative Route Differential Analysis
    let altCardsHtml = '';
    candidates.forEach(cand => {
      const isCandWinner = (cand.flow_allocated || 0) > 0;
      const cCost = cand.cost_formula?.unit_cost || 0;
      const cLat = cand.link_effective_latency_ms || 0;
      const deltaCost = cCost - primaryCost;
      const deltaLat = cLat - primaryLat;

      let tagClass = 'tag-blue';
      let statusTag = 'OPTIMAL ROUTE';
      let reasonText = `Selected as primary route carrying ${Math.round(cand.flow_allocated).toLocaleString()} req/s (${cand.allocation_pct.toFixed(1)}%).`;

      if (isCandWinner && cand !== primary) {
        tagClass = 'tag-emerald';
        statusTag = 'SECONDARY ROUTE';
        reasonText = `Absorbs overflow spillover of ${Math.round(cand.flow_allocated).toLocaleString()} req/s after primary saturation.`;
      } else if (!cand.server_healthy) {
        tagClass = 'tag-rose';
        statusTag = 'OUTAGE EXCLUDED';
        reasonText = 'Server is offline due to injected hardware/link failure; zero capacity available in graph.';
      } else if (cand.is_origin) {
        tagClass = 'tag-rose';
        statusTag = 'ORIGIN FALLBACK';
        reasonText = cand.flow_allocated > 0
          ? `Emergency fallback activated: carrying ${Math.round(cand.flow_allocated).toLocaleString()} req/s at expensive penalty rate ($${cCost.toFixed(2)}/req).`
          : `Avoided: Origin carries an extreme cost penalty ($${cCost.toFixed(2)}/req vs $${primaryCost.toFixed(2)}/req, a +$${deltaCost.toFixed(2)}/req surcharge).`;
      } else if (cand.eligibility.includes('CAPACITY')) {
        tagClass = 'tag-amber';
        statusTag = 'SATURATED';
        reasonText = `Server load at capacity limit (${Math.round(cand.server_existing_load)} / ${Math.round(cand.server_total_capacity)} req/s); residual capacity is 0.`;
      } else {
        tagClass = 'tag-amber';
        statusTag = 'REJECTED';
        reasonText = `Sub-optimal: &Delta;C = +$${deltaCost.toFixed(2)}/req, &Delta;L = +${deltaLat.toFixed(1)}ms latency penalty compared to ${primary.server}.`;
      }

      altCardsHtml += `
        <div class="alt-card ${isCandWinner ? 'is-winner' : ''}">
          <div class="alt-card-header">
            <span style="font-weight:700; font-size:0.82rem; color:var(--text-primary);">${cand.server}</span>
            <span class="factor-tag ${tagClass}">${statusTag}</span>
          </div>
          <div style="font-size:0.72rem; color:var(--text-secondary); margin-bottom:6px; line-height:1.4;">
            ${reasonText}
          </div>
          <div style="display:flex; justify-content:space-between; font-size:0.7rem; font-family:var(--font-mono); color:var(--text-muted); border-top:1px solid rgba(255,255,255,0.05); padding-top:4px;">
            <span>Latency: <strong style="color:var(--text-primary);">${cLat.toFixed(1)}ms</strong></span>
            <span>Cost: <strong style="color:var(--accent-blue);">$${cCost.toFixed(2)}</strong></span>
            <span>Cache: <strong style="color:var(--accent-emerald);">${(cand.cache_hit_ratio * 100).toFixed(0)}%</strong></span>
          </div>
        </div>
      `;
    });

    const alternativesCardHtml = `
      <div class="synthesis-card card-alternatives">
        <div class="synthesis-header">
          <div class="synthesis-title">
            <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="var(--accent-amber)" stroke-width="2"><circle cx="12" cy="12" r="10"/><line x1="12" y1="8" x2="12" y2="12"/><line x1="12" y1="16" x2="12.01" y2="16"/></svg>
            Alternative Edge Candidate Differential Analysis (Why nodes were accepted or rejected)
          </div>
          <span class="factor-tag tag-amber">4 Edge POPs + 1 Origin Evaluated</span>
        </div>
        <div style="font-size:0.78rem; color:var(--text-secondary); margin-bottom:8px;">
          For <strong style="color:var(--text-primary);">${this.selectedRegion}</strong> ingress traffic (${Math.round(regionDemand).toLocaleString()} req/s), every network route was evaluated against latency, link capacity, server compute limits, and cache hit metrics:
        </div>
        <div class="alt-grid">
          ${altCardsHtml}
        </div>
      </div>
    `;

    container.innerHTML = `
      ${systemCardHtml}
      ${mathCardHtml}
      ${alternativesCardHtml}
    `;
  },

  renderTimeline() {
    const list = document.getElementById('inspectorStepsList');
    if (!list) return;

    list.innerHTML = '';
    const steps = (this.currentTrace && this.currentTrace.steps) || [];

    if (steps.length === 0) {
      list.innerHTML = '<div style="padding:16px; color:var(--text-muted); text-align:center;">No execution steps recorded.</div>';
      return;
    }

    steps.forEach((step, idx) => {
      const item = document.createElement('div');
      item.className = `inspector-step-item ${idx === this.currentStepIndex ? 'active' : ''}`;
      item.setAttribute('data-index', idx);

      let badgeClass = 'badge-blue';
      if (step.phase.includes('INIT')) badgeClass = 'badge-blue';
      else if (step.phase.includes('AUGMENT') || step.phase.includes('ASSIGN')) badgeClass = 'badge-emerald';
      else if (step.phase.includes('POTENTIAL') || step.phase.includes('EVALUATION')) badgeClass = 'badge-amber';
      else if (step.phase.includes('EXHAUSTED')) badgeClass = 'badge-orange';
      else if (step.phase.includes('FALLBACK')) badgeClass = 'badge-rose';
      else if (step.phase.includes('TERMINATION')) badgeClass = 'badge-emerald';

      item.innerHTML = `
        <div style="display:flex; justify-content:space-between; align-items:center; margin-bottom:4px;">
          <span style="font-family:var(--font-mono); font-weight:700; font-size:0.75rem; color:var(--text-secondary);">#${step.stepNumber}</span>
          <span class="badge ${badgeClass}" style="font-size:0.6rem; padding:1px 5px;">${step.phase}</span>
        </div>
        <div style="font-weight:600; font-size:0.8rem; color:var(--text-primary); text-overflow:ellipsis; overflow:hidden; white-space:nowrap;">
          ${step.action}
        </div>
      `;

      item.addEventListener('click', () => {
        this.jumpToStep(idx);
      });

      list.appendChild(item);
    });
  },

  renderStep(index) {
    if (!this.currentTrace || !this.currentTrace.steps || this.currentTrace.steps.length === 0) {
      return;
    }

    const steps = this.currentTrace.steps;
    this.currentStepIndex = Math.max(0, Math.min(steps.length - 1, index));
    const step = steps[this.currentStepIndex];

    // Update Counter & Progress
    const counterElem = document.getElementById('traceStepCounter');
    if (counterElem) {
      counterElem.textContent = `Step ${this.currentStepIndex + 1} of ${steps.length}`;
    }

    const progressFill = document.getElementById('traceStepProgressFill');
    if (progressFill) {
      const pct = ((this.currentStepIndex + 1) / steps.length) * 100;
      progressFill.style.width = `${pct}%`;
    }

    // Update Active Step Card
    const titleElem = document.getElementById('stepActionTitle');
    if (titleElem) titleElem.textContent = step.action;

    const phaseElem = document.getElementById('stepPhaseBadge');
    if (phaseElem) {
      phaseElem.textContent = step.phase;
      let badgeClass = 'badge-blue';
      if (step.phase.includes('INIT')) badgeClass = 'badge-blue';
      else if (step.phase.includes('AUGMENT') || step.phase.includes('ASSIGN')) badgeClass = 'badge-emerald';
      else if (step.phase.includes('POTENTIAL') || step.phase.includes('EVALUATION')) badgeClass = 'badge-amber';
      else if (step.phase.includes('EXHAUSTED')) badgeClass = 'badge-orange';
      else if (step.phase.includes('FALLBACK')) badgeClass = 'badge-rose';
      else if (step.phase.includes('TERMINATION')) badgeClass = 'badge-emerald';
      phaseElem.className = `badge ${badgeClass}`;
    }

    const descElem = document.getElementById('stepDescriptionText');
    if (descElem) descElem.textContent = step.description;

    // Render Step Details Grid
    const detailsContainer = document.getElementById('stepDetailsGrid');
    if (detailsContainer) {
      detailsContainer.innerHTML = '';
      const details = step.details || {};

      if (Object.keys(details).length === 0) {
        detailsContainer.innerHTML = '<span style="color:var(--text-muted); font-size:0.8rem;">No additional variables logged for this step.</span>';
      } else {
        for (const [key, val] of Object.entries(details)) {
          const detailCard = document.createElement('div');
          detailCard.className = 'inspector-detail-box';
          const cleanKey = key.replace(/_/g, ' ').toUpperCase();
          detailCard.innerHTML = `
            <div class="inspector-detail-label">${cleanKey}</div>
            <div class="inspector-detail-value">${val}</div>
          `;
          detailsContainer.appendChild(detailCard);
        }
      }
    }

    // Highlight active timeline step item and scroll into view
    const timelineItems = document.querySelectorAll('.inspector-step-item');
    timelineItems.forEach((item, idx) => {
      if (idx === this.currentStepIndex) {
        item.classList.add('active');
        item.scrollIntoView({ behavior: 'smooth', block: 'nearest' });
      } else {
        item.classList.remove('active');
      }
    });

    // Update Prev / Next button disabled state
    const btnPrev = document.getElementById('btnTracePrev');
    const btnNext = document.getElementById('btnTraceNext');
    if (btnPrev) btnPrev.disabled = (this.currentStepIndex === 0);
    if (btnNext) btnNext.disabled = (this.currentStepIndex === steps.length - 1);

    // Update Interactive Graph Execution Visualizer
    GraphVisualizer.renderStep(this.currentStepIndex);
  },

  nextStep() {
    if (!this.currentTrace || !this.currentTrace.steps) return;
    if (this.currentStepIndex < this.currentTrace.steps.length - 1) {
      this.renderStep(this.currentStepIndex + 1);
    } else {
      this.stopAutoPlay();
    }
  },

  prevStep() {
    if (this.currentStepIndex > 0) {
      this.renderStep(this.currentStepIndex - 1);
    }
  },

  jumpToStep(idx) {
    this.renderStep(idx);
  },

  toggleAutoPlay() {
    if (this.isPlaying) {
      this.stopAutoPlay();
    } else {
      this.startAutoPlay();
    }
  },

  startAutoPlay() {
    this.isPlaying = true;
    const btnPlay = document.getElementById('btnTracePlay');
    if (btnPlay) {
      btnPlay.innerHTML = `<svg width="14" height="14" viewBox="0 0 24 24" fill="currentColor"><rect x="6" y="4" width="4" height="16"/><rect x="14" y="4" width="4" height="16"/></svg> Pause`;
      btnPlay.classList.add('btn-secondary');
      btnPlay.classList.remove('btn-outline');
    }

    // If at the end, restart from step 0
    if (this.currentTrace && this.currentStepIndex >= this.currentTrace.steps.length - 1) {
      this.renderStep(0);
    }

    this.autoPlayTimer = setInterval(() => {
      if (this.currentTrace && this.currentStepIndex < this.currentTrace.steps.length - 1) {
        this.nextStep();
      } else {
        this.stopAutoPlay();
      }
    }, 1200);
  },

  stopAutoPlay() {
    this.isPlaying = false;
    if (this.autoPlayTimer) {
      clearInterval(this.autoPlayTimer);
      this.autoPlayTimer = null;
    }
    const btnPlay = document.getElementById('btnTracePlay');
    if (btnPlay) {
      btnPlay.innerHTML = `<svg width="14" height="14" viewBox="0 0 24 24" fill="currentColor"><polygon points="5 3 19 12 5 21 5 3"/></svg> Play`;
      btnPlay.classList.remove('btn-secondary');
      btnPlay.classList.add('btn-outline');
    }
  },

  renderFinalDecision() {
    if (!this.currentSnapshot) return;

    const routing = this.currentSnapshot.routing || {};
    const allocations = routing.allocations || [];

    // Final KPI cards
    const totalRoutedElem = document.getElementById('inspectKpiTotalRouted');
    if (totalRoutedElem) totalRoutedElem.textContent = `${Math.round(routing.totalTrafficRouted || 0).toLocaleString()} req/s`;

    const edgeRoutedElem = document.getElementById('inspectKpiEdgeTraffic');
    if (edgeRoutedElem) edgeRoutedElem.textContent = `${Math.round(routing.edgeTrafficRouted || 0).toLocaleString()} req/s`;

    const originElem = document.getElementById('inspectKpiOriginTraffic');
    if (originElem) {
      const orig = routing.originTrafficRouted || 0;
      originElem.textContent = `${Math.round(orig).toLocaleString()} req/s`;
      originElem.style.color = orig > 0 ? 'var(--accent-rose)' : 'var(--accent-emerald)';
    }

    const costElem = document.getElementById('inspectKpiRoutingCost');
    if (costElem) costElem.textContent = `$${(routing.totalRoutingCost || 0).toFixed(2)}`;

    const latElem = document.getElementById('inspectKpiAvgLatency');
    if (latElem) latElem.textContent = `${(routing.averageLatencyMs || 0).toFixed(2)} ms`;

    // Allocation Table
    const tbody = document.getElementById('inspectAllocationsTableBody');
    if (tbody) {
      tbody.innerHTML = '';

      // Filter only region-to-server and region-to-origin allocations for clean readability
      const clientAllocs = allocations.filter(a => 
        !a.from.includes('Source') && !a.to.includes('Sink') && a.flow > 0.01
      );

      if (clientAllocs.length === 0) {
        tbody.innerHTML = '<tr><td colspan="5" style="text-align:center; padding:16px; color:var(--text-muted);">No active allocations.</td></tr>';
      } else {
        clientAllocs.forEach(a => {
          const row = document.createElement('tr');
          const isOrigin = a.to.includes('Origin');
          row.innerHTML = `
            <td style="font-weight:600; color:var(--text-primary);">${a.from}</td>
            <td>
              <span class="badge ${isOrigin ? 'badge-rose' : 'badge-blue'}">
                ${a.to}
              </span>
            </td>
            <td style="font-family:var(--font-mono); font-weight:600;">${Math.round(a.flow).toLocaleString()} req/s</td>
            <td style="font-family:var(--font-mono); color:var(--text-muted);">$${Number(a.unitCost || 0).toFixed(2)}/r</td>
            <td style="font-family:var(--font-mono); color:var(--accent-emerald); font-weight:600;">$${Number(a.totalCost || 0).toFixed(2)}</td>
          `;
          tbody.appendChild(row);
        });
      }
    }

    // Explanation Box
    const explanationBox = document.getElementById('inspectExplanationText');
    if (explanationBox) {
      const exp = (this.currentTrace && this.currentTrace.finalExplanation) || routing.explanation || 'Optimal allocation achieved.';
      explanationBox.textContent = exp;
    }
  }
};

