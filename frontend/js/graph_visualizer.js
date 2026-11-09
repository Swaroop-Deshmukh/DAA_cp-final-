/**
 * Interactive Graph Execution Visualizer
 * Renders the authentic C++ Min-Cost Max-Flow and Baseline residual graph topology.
 * Demonstrates node potentials, edge capacities, flows, residual reverse edges,
 * Dijkstra augmenting shortest paths, and animated flow augmentation.
 */

export const GraphVisualizer = {
  svgId: 'graphVisualizerSvg',
  containerId: 'graphVisualizerContainer',

  // Raw Trace Data
  initialNodes: [],
  initialEdges: [],
  steps: [],
  currentStepIndex: 0,

  // Node static layout positions in 1000x560 coordinate space
  nodePositions: {
    'Source (S)':        { x: 80,  y: 280, layer: 0, type: 'SOURCE', label: 'Super-Source S' },
    'Pune':              { x: 300, y: 80,  layer: 1, type: 'REGION', label: 'Pune Region' },
    'Mumbai':            { x: 300, y: 180, layer: 1, type: 'REGION', label: 'Mumbai Region' },
    'Delhi':             { x: 300, y: 280, layer: 1, type: 'REGION', label: 'Delhi Region' },
    'Bangalore':         { x: 300, y: 380, layer: 1, type: 'REGION', label: 'Bangalore Region' },
    'Hyderabad':         { x: 300, y: 480, layer: 1, type: 'REGION', label: 'Hyderabad Region' },
    'Pune Server':       { x: 680, y: 80,  layer: 2, type: 'SERVER', label: 'Pune Edge POP' },
    'Mumbai Server':     { x: 680, y: 180, layer: 2, type: 'SERVER', label: 'Mumbai Edge POP' },
    'Delhi Server':      { x: 680, y: 280, layer: 2, type: 'SERVER', label: 'Delhi Edge POP' },
    'Bangalore Server':  { x: 680, y: 380, layer: 2, type: 'SERVER', label: 'Bangalore Edge POP' },
    'Origin Server':     { x: 680, y: 480, layer: 2, type: 'ORIGIN', label: 'Central Origin DC' },
    'Sink (T)':          { x: 920, y: 280, layer: 3, type: 'SINK',   label: 'Super-Sink T' }
  },

  // Viewport / Zoom / Pan state
  zoom: 1.0,
  panX: 0,
  panY: 0,
  isPanning: false,
  startPan: { x: 0, y: 0 },

  // Display toggles
  showEdgeLabels: true,
  showResidualEdges: false,
  showCandidates: true,
  highlightedRegion: 'Pune',

  // Selected element for inspection flyout
  selectedElement: null,
  activeCandidatePath: null,

  init() {
    this.bindControls();
    this.setupPanAndZoom();
  },

  bindControls() {
    const btnZoomIn = document.getElementById('btnGraphZoomIn');
    if (btnZoomIn) btnZoomIn.addEventListener('click', () => this.adjustZoom(0.15));

    const btnZoomOut = document.getElementById('btnGraphZoomOut');
    if (btnZoomOut) btnZoomOut.addEventListener('click', () => this.adjustZoom(-0.15));

    const btnFit = document.getElementById('btnGraphFit');
    if (btnFit) btnFit.addEventListener('click', () => this.fitToScreen());

    const btnReset = document.getElementById('btnGraphReset');
    if (btnReset) btnReset.addEventListener('click', () => this.resetView());

    const chkLabels = document.getElementById('chkShowEdgeLabels');
    if (chkLabels) {
      chkLabels.addEventListener('change', (e) => {
        this.showEdgeLabels = e.target.checked;
        this.renderCurrentStep();
      });
    }

    const chkResidual = document.getElementById('chkShowResidualEdges');
    if (chkResidual) {
      chkResidual.addEventListener('change', (e) => {
        this.showResidualEdges = e.target.checked;
        this.renderCurrentStep();
      });
    }

    const chkCandidates = document.getElementById('chkShowCandidates');
    if (chkCandidates) {
      chkCandidates.addEventListener('change', (e) => {
        this.showCandidates = e.target.checked;
        this.renderCurrentStep();
      });
    }

    const btnFs = document.getElementById('btnGraphFullscreen');
    if (btnFs) {
      btnFs.addEventListener('click', () => this.toggleFullscreen());
    }
  },

  setupPanAndZoom() {
    const container = document.getElementById(this.containerId);
    if (!container) return;

    container.addEventListener('mousedown', (e) => {
      // Ignore clicks on detail cards or buttons
      if (e.target.closest('#graphElementDetails') || e.target.closest('#graphDijkstraHud') || e.target.closest('button')) {
        return;
      }
      this.isPanning = true;
      this.startPan = { x: e.clientX - this.panX, y: e.clientY - this.panY };
      container.style.cursor = 'grabbing';
    });

    window.addEventListener('mousemove', (e) => {
      if (!this.isPanning) return;
      this.panX = e.clientX - this.startPan.x;
      this.panY = e.clientY - this.startPan.y;
      this.applyTransform();
    });

    window.addEventListener('mouseup', () => {
      if (this.isPanning) {
        this.isPanning = false;
        container.style.cursor = 'grab';
      }
    });

    container.addEventListener('wheel', (e) => {
      e.preventDefault();
      const zoomFactor = e.deltaY < 0 ? 1.1 : 0.9;
      this.adjustZoomAtPoint(zoomFactor, e.offsetX, e.offsetY);
    }, { passive: false });
  },

  adjustZoom(delta) {
    this.zoom = Math.max(0.4, Math.min(2.5, this.zoom + delta));
    this.applyTransform();
  },

  adjustZoomAtPoint(factor, x, y) {
    const newZoom = Math.max(0.4, Math.min(2.5, this.zoom * factor));
    this.panX = x - (x - this.panX) * (newZoom / this.zoom);
    this.panY = y - (y - this.panY) * (newZoom / this.zoom);
    this.zoom = newZoom;
    this.applyTransform();
  },

  resetView() {
    this.zoom = 1.0;
    this.panX = 0;
    this.panY = 0;
    this.applyTransform();
  },

  fitToScreen() {
    const container = document.getElementById(this.containerId);
    if (!container) return;
    const w = container.clientWidth;
    const h = container.clientHeight;
    if (w <= 0 || h <= 0) {
      this.zoom = 1.0;
      this.panX = 0;
      this.panY = 0;
      this.applyTransform();
      return;
    }
    const scaleX = w / 1000;
    const scaleY = h / 560;
    this.zoom = Math.max(0.3, Math.min(scaleX, scaleY) * 0.95);
    this.panX = (w - 1000 * this.zoom) / 2;
    this.panY = (h - 560 * this.zoom) / 2;
    this.applyTransform();
  },

  toggleFullscreen() {
    const card = document.querySelector('.graph-visualizer-card');
    if (!card) return;
    if (!document.fullscreenElement) {
      card.requestFullscreen().catch(err => console.warn(err));
    } else {
      document.exitFullscreen().catch(err => console.warn(err));
    }
  },

  applyTransform() {
    const g = document.getElementById('graphMainGroup');
    if (g) {
      g.setAttribute('transform', `translate(${this.panX}, ${this.panY}) scale(${this.zoom})`);
    }
  },

  loadTrace(trace) {
    if (!trace) return;
    this.initialNodes = trace.initialNodes || [];
    this.initialEdges = trace.initialEdges || [];
    this.steps = trace.steps || [];
    this.currentStepIndex = 0;
    this.selectedElement = null;
    this.closeFlyout();

    const stratBadge = document.getElementById('graphAlgorithmBadge');
    if (stratBadge) {
      stratBadge.textContent = `${trace.strategy || 'MCMF'} Graph Engine`;
    }

    this.renderStep(0);
  },

  renderStep(stepIndex) {
    this.currentStepIndex = stepIndex;
    this.renderCurrentStep();
  },

  renderCurrentStep() {
    const svg = document.getElementById(this.svgId);
    if (!svg) return;

    const computedState = this.computeStateAtStep(this.currentStepIndex);
    const currentStep = this.steps[this.currentStepIndex] || null;

    // Draw SVG contents
    svg.innerHTML = this.buildSvgHtml(computedState, currentStep);

    // Re-apply Pan / Zoom Transform
    this.applyTransform();

    // Attach interactive node / edge listeners
    this.attachSvgListeners(computedState, currentStep);

    // Update Dijkstra HUD
    this.updateDijkstraHud(currentStep);
  },

  computeStateAtStep(targetIndex) {
    // 1. Clone initial edges
    const edgeMap = new Map();
    this.initialEdges.forEach(e => {
      const key = `${e.fromName}->${e.toName}:${e.isReverse ? 'rev' : 'fwd'}:${e.label}`;
      edgeMap.set(key, {
        ...e,
        currentFlow: e.flow,
        currentResidual: e.residualCapacity,
        status: 'NORMAL'
      });
    });

    // 2. Play edge updates forward from step 0 to targetIndex
    for (let i = 0; i <= targetIndex && i < this.steps.length; ++i) {
      const st = this.steps[i];
      if (st.edgeUpdates && Array.isArray(st.edgeUpdates)) {
        st.edgeUpdates.forEach(up => {
          const key = `${up.fromName}->${up.toName}:${up.isReverse ? 'rev' : 'fwd'}:${up.label}`;
          if (edgeMap.has(key)) {
            const edge = edgeMap.get(key);
            edge.currentFlow = up.newFlow;
            edge.currentResidual = up.newResidual;
            edge.reducedCost = up.reducedCost;
          }
        });
      }
    }

    // 3. Mark active augmenting path or candidate paths for current step
    const currentStep = this.steps[targetIndex];
    if (currentStep) {
      const pathNodeNames = currentStep.pathNodeNames || [];
      if (pathNodeNames.length >= 2) {
        for (let j = 0; j < pathNodeNames.length - 1; ++j) {
          const u = pathNodeNames[j];
          const v = pathNodeNames[j + 1];
          // Find matching edge
          edgeMap.forEach(e => {
            if (e.fromName === u && e.toName === v && !e.isReverse) {
              e.status = 'SELECTED';
            }
          });
        }
      }

      // Candidate paths
      if (this.showCandidates && currentStep.candidatePaths && Array.isArray(currentStep.candidatePaths)) {
        currentStep.candidatePaths.forEach(cand => {
          const names = cand.nodeNames || [];
          for (let k = 0; k < names.length - 1; ++k) {
            const u = names[k];
            const v = names[k + 1];
            edgeMap.forEach(e => {
              if (e.fromName === u && e.toName === v && !e.isReverse && e.status !== 'SELECTED') {
                if (cand.status === 'EXCLUDED_CAPACITY') e.status = 'SATURATED';
                else if (cand.status === 'EXCLUDED_OUTAGE') e.status = 'UNAVAILABLE';
                else e.status = 'CANDIDATE';
              }
            });
          }
        });
      }

      // Region Candidate Highlighting
      if (this.highlightedRegion) {
        edgeMap.forEach(e => {
          if (e.fromName === this.highlightedRegion && !e.isReverse && e.status === 'NORMAL') {
            e.status = 'REGION_CANDIDATE';
          }
        });
      }

      // Active Focused Candidate Route (when user clicks any candidate row in the matrix)
      if (this.activeCandidatePath) {
        const fromR = this.activeCandidatePath.from;
        const toS = this.activeCandidatePath.to;
        edgeMap.forEach(e => {
          if (!e.isReverse) {
            if (e.fromName === 'Source (S)' && e.toName === fromR) {
              e.status = 'ACTIVE_CANDIDATE_INGRESS';
            } else if (e.fromName === fromR && e.toName === toS) {
              e.status = 'ACTIVE_CANDIDATE';
            } else if (e.fromName === toS && e.toName === 'Sink (T)') {
              e.status = 'ACTIVE_CANDIDATE_EGRESS';
            }
          }
        });
      }
    }

    // 4. Compute Node inflow and outflow
    const nodeStateMap = new Map();
    this.initialNodes.forEach(n => {
      nodeStateMap.set(n.name, {
        ...n,
        inflow: 0,
        outflow: 0
      });
    });

    edgeMap.forEach(e => {
      if (!e.isReverse && e.currentFlow > 0) {
        if (nodeStateMap.has(e.fromName)) nodeStateMap.get(e.fromName).outflow += e.currentFlow;
        if (nodeStateMap.has(e.toName)) nodeStateMap.get(e.toName).inflow += e.currentFlow;
      }
    });

    return {
      nodes: Array.from(nodeStateMap.values()),
      edges: Array.from(edgeMap.values())
    };
  },

  buildSvgHtml(state, currentStep) {
    const defs = `
      <defs>
        <filter id="glow-cyan" x="-20%" y="-20%" width="140%" height="140%">
          <feGaussianBlur stdDeviation="3.5" result="blur" />
          <feComposite in="SourceGraphic" in2="blur" operator="over" />
        </filter>
        <filter id="glow-emerald" x="-20%" y="-20%" width="140%" height="140%">
          <feGaussianBlur stdDeviation="4" result="blur" />
          <feComposite in="SourceGraphic" in2="blur" operator="over" />
        </filter>
        <filter id="glow-amber" x="-20%" y="-20%" width="140%" height="140%">
          <feGaussianBlur stdDeviation="3.5" result="blur" />
          <feComposite in="SourceGraphic" in2="blur" operator="over" />
        </filter>
        <filter id="glow-purple" x="-20%" y="-20%" width="140%" height="140%">
          <feGaussianBlur stdDeviation="3.5" result="blur" />
          <feComposite in="SourceGraphic" in2="blur" operator="over" />
        </filter>

        <!-- Arrowheads -->
        <marker id="arrow-default" viewBox="0 0 10 10" refX="22" refY="5" markerWidth="6" markerHeight="6" orient="auto-start-reverse">
          <path d="M 0 1 L 10 5 L 0 9 z" fill="#475569" />
        </marker>
        <marker id="arrow-active" viewBox="0 0 10 10" refX="22" refY="5" markerWidth="6" markerHeight="6" orient="auto-start-reverse">
          <path d="M 0 1 L 10 5 L 0 9 z" fill="#3b82f6" />
        </marker>
        <marker id="arrow-selected" viewBox="0 0 10 10" refX="22" refY="5" markerWidth="7" markerHeight="7" orient="auto-start-reverse">
          <path d="M 0 1 L 10 5 L 0 9 z" fill="#10b981" />
        </marker>
        <marker id="arrow-residual" viewBox="0 0 10 10" refX="22" refY="5" markerWidth="6" markerHeight="6" orient="auto-start-reverse">
          <path d="M 0 1 L 10 5 L 0 9 z" fill="#c084fc" />
        </marker>
        <marker id="arrow-candidate" viewBox="0 0 10 10" refX="22" refY="5" markerWidth="6" markerHeight="6" orient="auto-start-reverse">
          <path d="M 0 1 L 10 5 L 0 9 z" fill="#f59e0b" />
        </marker>
      </defs>
    `;

    // Group with Pan & Zoom transform
    let html = `${defs}<g id="graphMainGroup" transform="translate(${this.panX}, ${this.panY}) scale(${this.zoom})">`;

    // 1. Draw Edges
    html += '<g class="graph-edges-layer">';
    state.edges.forEach((edge, idx) => {
      if (edge.isReverse && !this.showResidualEdges) return;

      const p1 = this.nodePositions[edge.fromName];
      const p2 = this.nodePositions[edge.toName];
      if (!p1 || !p2) return;

      const edgeSvg = this.renderEdgeSvg(edge, p1, p2, idx);
      html += edgeSvg;
    });
    html += '</g>';

    // 2. Draw Flow Particles Animation (Candidate path or Augmenting path)
    let particlePath = null;
    let particleFlow = 1000;
    let particleColor = '#10b981';

    if (this.activeCandidatePath) {
      particlePath = ['Source (S)', this.activeCandidatePath.from, this.activeCandidatePath.to, 'Sink (T)'];
      particleFlow = this.activeCandidatePath.info?.flow_allocated || 1500;
      particleColor = (this.activeCandidatePath.info?.flow_allocated > 0) 
        ? '#10b981' 
        : (this.activeCandidatePath.info?.is_origin ? '#ef4444' : '#f59e0b');
    } else if (currentStep && currentStep.bottleneckFlow > 0 && currentStep.pathNodeNames && currentStep.pathNodeNames.length >= 2) {
      particlePath = currentStep.pathNodeNames;
      particleFlow = currentStep.bottleneckFlow;
      particleColor = '#10b981';
    }

    if (particlePath) {
      html += '<g class="graph-particles-layer">';
      html += this.renderPathParticles(particlePath, particleFlow, particleColor);
      html += '</g>';
    }

    // 3. Draw Nodes
    html += '<g class="graph-nodes-layer">';
    state.nodes.forEach(node => {
      const pos = this.nodePositions[node.name];
      if (!pos) return;
      html += this.renderNodeSvg(node, pos);
    });
    html += '</g>';

    html += '</g>';
    return html;
  },

  renderEdgeSvg(edge, p1, p2, idx) {
    const isRev = edge.isReverse;
    const isSelected = edge.status === 'SELECTED';
    const isCandidate = edge.status === 'CANDIDATE';
    const isSaturated = (edge.currentResidual <= 0 && !isRev);
    const hasFlow = (edge.currentFlow > 0 && !isRev);

    // Calculate Bézier curve control points
    let dx = p2.x - p1.x;
    let dy = p2.y - p1.y;

    // Slight curvature offset to separate forward and reverse edges
    let curvatureOffset = isRev ? 26 : 0;
    // Separate parallel load bands between Server and Sink
    if (edge.label && edge.label.includes('Band')) {
      const bandMatch = edge.label.match(/Band (\d)/);
      if (bandMatch) {
        const bandNum = parseInt(bandMatch[1], 10);
        curvatureOffset += (bandNum - 2) * 12;
      }
    }

    const cx1 = p1.x + dx * 0.45;
    const cy1 = p1.y + curvatureOffset;
    const cx2 = p1.x + dx * 0.55;
    const cy2 = p2.y + curvatureOffset;

    const pathD = `M ${p1.x} ${p1.y} C ${cx1} ${cy1}, ${cx2} ${cy2}, ${p2.x} ${p2.y}`;

    // Color and stroke
    let stroke = '#334155';
    let strokeWidth = 1.6;
    let strokeDash = 'none';
    let marker = 'url(#arrow-default)';
    let filter = '';

    if (edge.status === 'ACTIVE_CANDIDATE') {
      const isWinner = (edge.currentFlow > 0);
      stroke = isWinner ? '#10b981' : (edge.toName.includes('Origin') ? '#ef4444' : '#f59e0b');
      strokeWidth = 3.6;
      strokeDash = isWinner ? 'none' : '6,4';
      marker = isWinner ? 'url(#arrow-selected)' : 'url(#arrow-candidate)';
      filter = isWinner ? 'filter="url(#glow-emerald)"' : 'filter="url(#glow-amber)"';
    } else if (edge.status === 'ACTIVE_CANDIDATE_INGRESS') {
      stroke = '#3b82f6';
      strokeWidth = 3.2;
      marker = 'url(#arrow-active)';
      filter = 'filter="url(#glow-cyan)"';
    } else if (edge.status === 'ACTIVE_CANDIDATE_EGRESS') {
      stroke = '#10b981';
      strokeWidth = 2.8;
      marker = 'url(#arrow-selected)';
    } else if (isSelected) {
      stroke = '#10b981';
      strokeWidth = 3;
      marker = 'url(#arrow-selected)';
      filter = 'filter="url(#glow-emerald)"';
    } else if (isRev) {
      stroke = '#a855f7';
      strokeWidth = 1.6;
      strokeDash = '4,4';
      marker = 'url(#arrow-residual)';
    } else if (hasFlow) {
      stroke = '#3b82f6';
      strokeWidth = Math.min(4, 1.8 + (edge.currentFlow / (edge.capacity || 5000)) * 2);
      marker = 'url(#arrow-active)';
    } else if (edge.status === 'REGION_CANDIDATE') {
      stroke = '#60a5fa';
      strokeWidth = 2.2;
      strokeDash = '5,3';
      marker = 'url(#arrow-active)';
    } else if (isCandidate) {
      stroke = '#f59e0b';
      strokeWidth = 2.2;
      strokeDash = '6,4';
      marker = 'url(#arrow-candidate)';
    } else if (isSaturated) {
      stroke = '#475569';
      strokeWidth = 1.2;
    }

    // Edge Label
    let labelHtml = '';
    if (this.showEdgeLabels) {
      // Midpoint on cubic bezier
      const midX = 0.125 * p1.x + 0.375 * cx1 + 0.375 * cx2 + 0.125 * p2.x;
      const midY = 0.125 * p1.y + 0.375 * cy1 + 0.375 * cy2 + 0.125 * p2.y;

      let displayText = '';
      if (isRev) {
        displayText = `r_rev: ${Math.round(edge.currentResidual)}`;
      } else if (edge.capacity > 0) {
        displayText = `${Math.round(edge.currentFlow)}/${Math.round(edge.capacity)}`;
      }

      if (displayText) {
        labelHtml = `
          <g class="graph-edge-label" transform="translate(${midX}, ${midY - 4})">
            <rect x="-30" y="-8" width="60" height="15" rx="3" fill="#111624" stroke="${isSelected ? '#10b981' : '#1e293b'}" stroke-width="0.8" opacity="0.95" />
            <text text-anchor="middle" y="3" fill="${isSelected ? '#10b981' : (isRev ? '#c084fc' : '#94a3b8')}" font-size="8.5" font-family="var(--font-mono)">
              ${displayText}
            </text>
          </g>
        `;
      }
    }

    return `
      <g class="graph-edge-group" data-edge-idx="${idx}" data-from="${edge.fromName}" data-to="${edge.toName}" data-rev="${isRev}" style="cursor:pointer;">
        <!-- Invisible hit area for easier clicking -->
        <path d="${pathD}" fill="none" stroke="transparent" stroke-width="14" />
        <path d="${pathD}" fill="none" stroke="${stroke}" stroke-width="${strokeWidth}" stroke-dasharray="${strokeDash}" marker-end="${marker}" ${filter} />
        ${labelHtml}
      </g>
    `;
  },

  renderPathParticles(nodeNames, bottleneckFlow) {
    let particlesSvg = '';
    for (let i = 0; i < nodeNames.length - 1; ++i) {
      const p1 = this.nodePositions[nodeNames[i]];
      const p2 = this.nodePositions[nodeNames[i + 1]];
      if (!p1 || !p2) continue;

      const dx = p2.x - p1.x;
      const cx1 = p1.x + dx * 0.45;
      const cy1 = p1.y;
      const cx2 = p1.x + dx * 0.55;
      const cy2 = p2.y;
      const pathId = `flowPath_${i}`;
      const pathD = `M ${p1.x} ${p1.y} C ${cx1} ${cy1}, ${cx2} ${cy2}, ${p2.x} ${p2.y}`;

      particlesSvg += `
        <path id="${pathId}" d="${pathD}" fill="none" stroke="none" />
        <circle cx="0" cy="0" r="4" fill="#10b981">
          <animateMotion dur="0.9s" repeatCount="indefinite" rotate="auto">
            <mpath href="#${pathId}" xlink:href="#${pathId}" />
          </animateMotion>
        </circle>
        <circle cx="0" cy="0" r="3" fill="#3b82f6">
          <animateMotion dur="0.9s" begin="0.45s" repeatCount="indefinite" rotate="auto">
            <mpath href="#${pathId}" xlink:href="#${pathId}" />
          </animateMotion>
        </circle>
      `;
    }
    return particlesSvg;
  },

  renderNodeSvg(node, pos) {
    const isSelected = this.selectedElement && this.selectedElement.type === 'node' && this.selectedElement.data.name === node.name;
    const type = pos.type;

    let shapeHtml = '';
    let mainColor = '#3b82f6';
    let filter = '';

    if (type === 'SOURCE') {
      mainColor = '#3b82f6';
      shapeHtml = `
        <circle cx="${pos.x}" cy="${pos.y}" r="20" fill="#1e293b" stroke="${mainColor}" stroke-width="2" />
        <text x="${pos.x}" y="${pos.y + 5}" text-anchor="middle" font-size="13" font-weight="700" fill="#e2e8f0" font-family="var(--font-mono)">S</text>
      `;
    } else if (type === 'SINK') {
      mainColor = '#10b981';
      shapeHtml = `
        <circle cx="${pos.x}" cy="${pos.y}" r="20" fill="#064e3b" stroke="${mainColor}" stroke-width="2" />
        <text x="${pos.x}" y="${pos.y + 5}" text-anchor="middle" font-size="13" font-weight="700" fill="#ecfdf5" font-family="var(--font-mono)">T</text>
      `;
    } else if (type === 'ORIGIN') {
      mainColor = '#ef4444';
      shapeHtml = `
        <rect x="${pos.x - 34}" y="${pos.y - 19}" width="68" height="38" rx="6" fill="#450a0a" stroke="${mainColor}" stroke-width="1.8" />
        <text x="${pos.x}" y="${pos.y - 2}" text-anchor="middle" font-size="10" font-weight="700" fill="#fee2e2">ORIGIN</text>
        <text x="${pos.x}" y="${pos.y + 11}" text-anchor="middle" font-size="8.5" fill="#f87171" font-family="var(--font-mono)">Fallback</text>
      `;
    } else if (type === 'REGION') {
      mainColor = '#6366f1';
      shapeHtml = `
        <rect x="${pos.x - 36}" y="${pos.y - 18}" width="72" height="36" rx="5" fill="#1e1b4b" stroke="${mainColor}" stroke-width="1.8" />
        <text x="${pos.x}" y="${pos.y - 1}" text-anchor="middle" font-size="10" font-weight="700" fill="#e0e7ff">${node.name}</text>
        <text x="${pos.x}" y="${pos.y + 11}" text-anchor="middle" font-size="8" fill="#a5b4fc" font-family="var(--font-mono)">${Math.round(node.outflow)} req/s</text>
      `;
    } else if (type === 'SERVER') {
      mainColor = '#10b981';
      const shortName = node.name.replace(' Server', '');
      shapeHtml = `
        <rect x="${pos.x - 42}" y="${pos.y - 19}" width="84" height="38" rx="6" fill="#0f172a" stroke="${mainColor}" stroke-width="1.8" />
        <circle cx="${pos.x - 28}" cy="${pos.y - 4}" r="3" fill="#10b981" />
        <text x="${pos.x + 4}" y="${pos.y - 1}" text-anchor="middle" font-size="10" font-weight="700" fill="#f8fafc">${shortName} POP</text>
        <text x="${pos.x}" y="${pos.y + 12}" text-anchor="middle" font-size="8" fill="#94a3b8" font-family="var(--font-mono)">Flow: ${Math.round(node.inflow)}</text>
      `;
    }

    // Node selection halo
    let haloHtml = '';
    if (isSelected) {
      haloHtml = `<circle cx="${pos.x}" cy="${pos.y}" r="38" fill="none" stroke="#10b981" stroke-width="2" stroke-dasharray="4,4" />`;
    }

    return `
      <g class="graph-node-group" data-node-name="${node.name}" style="cursor:pointer;">
        ${haloHtml}
        ${shapeHtml}
      </g>
    `;
  },

  attachSvgListeners(computedState, currentStep) {
    const svg = document.getElementById(this.svgId);
    if (!svg) return;

    // 1. Node Click Listeners
    svg.querySelectorAll('.graph-node-group').forEach(el => {
      el.addEventListener('click', (e) => {
        e.stopPropagation();
        const nodeName = el.getAttribute('data-node-name');
        const nodeObj = computedState.nodes.find(n => n.name === nodeName);
        if (nodeObj) {
          this.selectNode(nodeObj, computedState);
        }
      });
    });

    // 2. Edge Click Listeners
    svg.querySelectorAll('.graph-edge-group').forEach(el => {
      el.addEventListener('click', (e) => {
        e.stopPropagation();
        const fromName = el.getAttribute('data-from');
        const toName = el.getAttribute('data-to');
        const isRev = el.getAttribute('data-rev') === 'true';
        const edgeObj = computedState.edges.find(e => e.fromName === fromName && e.toName === toName && e.isReverse === isRev);
        if (edgeObj) {
          this.selectEdge(edgeObj, currentStep);
        }
      });
    });

    // 3. SVG Canvas click clears selection
    svg.addEventListener('click', () => {
      this.closeFlyout();
    });
  },

  selectNode(node, state) {
    this.selectedElement = { type: 'node', data: node };
    const flyout = document.getElementById('graphElementDetails');
    if (!flyout) return;

    const pos = this.nodePositions[node.name] || {};
    const flowBalance = node.inflow - node.outflow;

    flyout.innerHTML = `
      <div style="display:flex; justify-content:space-between; align-items:center; margin-bottom:8px;">
        <span class="badge badge-blue" style="font-size:0.68rem;">${pos.type || 'NODE'}</span>
        <button id="btnGraphCloseFlyout" style="background:none; border:none; color:var(--text-muted); cursor:pointer; font-size:1rem;">&times;</button>
      </div>
      <div style="font-size:0.95rem; font-weight:700; color:var(--text-primary); margin-bottom:10px;">${node.name}</div>
      <div class="graph-flyout-stat">
        <span class="flyout-stat-label">Total Inflow:</span>
        <span class="flyout-stat-val" style="color:var(--accent-blue);">${Math.round(node.inflow).toLocaleString()} req/s</span>
      </div>
      <div class="graph-flyout-stat">
        <span class="flyout-stat-label">Total Outflow:</span>
        <span class="flyout-stat-val" style="color:var(--accent-blue);">${Math.round(node.outflow).toLocaleString()} req/s</span>
      </div>
      <div class="graph-flyout-stat">
        <span class="flyout-stat-label">Flow Balance (In - Out):</span>
        <span class="flyout-stat-val" style="color:${Math.abs(flowBalance) < 0.01 || pos.type === 'SOURCE' || pos.type === 'SINK' ? 'var(--accent-emerald)' : 'var(--accent-amber)'};">
          ${pos.type === 'SOURCE' || pos.type === 'SINK' ? 'Boundary Endpoint' : `${Math.round(flowBalance)} req/s`}
        </span>
      </div>
      <div class="graph-flyout-stat">
        <span class="flyout-stat-label">Graph Layer:</span>
        <span class="flyout-stat-val">Layer ${pos.layer}</span>
      </div>
    `;

    flyout.style.display = 'block';
    const closeBtn = document.getElementById('btnGraphCloseFlyout');
    if (closeBtn) closeBtn.addEventListener('click', () => this.closeFlyout());
  },

  selectEdge(edge, currentStep) {
    this.selectedElement = { type: 'edge', data: edge };
    const flyout = document.getElementById('graphElementDetails');
    if (!flyout) return;

    flyout.innerHTML = `
      <div style="display:flex; justify-content:space-between; align-items:center; margin-bottom:8px;">
        <span class="badge ${edge.isReverse ? 'badge-purple' : 'badge-blue'}" style="font-size:0.68rem;">
          ${edge.isReverse ? 'REVERSE RESIDUAL EDGE' : 'FORWARD EDGE'}
        </span>
        <button id="btnGraphCloseFlyout" style="background:none; border:none; color:var(--text-muted); cursor:pointer; font-size:1rem;">&times;</button>
      </div>
      <div style="font-size:0.95rem; font-weight:700; color:var(--text-primary); margin-bottom:10px;">
        ${edge.fromName} &rarr; ${edge.toName}
      </div>
      <div class="graph-flyout-stat">
        <span class="flyout-stat-label">Allocated Flow:</span>
        <span class="flyout-stat-val" style="color:var(--accent-blue); font-weight:700;">${Math.round(edge.currentFlow).toLocaleString()} req/s</span>
      </div>
      <div class="graph-flyout-stat">
        <span class="flyout-stat-label">Maximum Capacity:</span>
        <span class="flyout-stat-val">${Math.round(edge.capacity).toLocaleString()} req/s</span>
      </div>
      <div class="graph-flyout-stat">
        <span class="flyout-stat-label">Residual Capacity c_f:</span>
        <span class="flyout-stat-val" style="color:var(--accent-emerald); font-weight:700;">${Math.round(edge.currentResidual).toLocaleString()} req/s</span>
      </div>
      <div class="graph-flyout-stat">
        <span class="flyout-stat-label">Unit Cost c(u,v):</span>
        <span class="flyout-stat-val">$${(edge.unitCost || 0).toFixed(2)} / req</span>
      </div>
      <div class="graph-flyout-stat">
        <span class="flyout-stat-label">Johnson Reduced Cost:</span>
        <span class="flyout-stat-val" style="color:var(--accent-amber); font-weight:700;">
          ${edge.reducedCost !== undefined ? edge.reducedCost : '0'}
        </span>
      </div>
      ${edge.isReverse ? `
        <div style="margin-top:10px; padding:6px 8px; background:rgba(168,85,247,0.12); border-radius:4px; font-size:0.75rem; color:#d8b4fe; line-height:1.4;">
          <strong>Residual Mechanism:</strong> Reverse residual capacity allows Dijkstra to cancel/redirect previous suboptimal routing decisions to guarantee global mathematical optimality.
        </div>
      ` : ''}
    `;

    flyout.style.display = 'block';
    const closeBtn = document.getElementById('btnGraphCloseFlyout');
    if (closeBtn) closeBtn.addEventListener('click', () => this.closeFlyout());
  },

  closeFlyout() {
    this.selectedElement = null;
    const flyout = document.getElementById('graphElementDetails');
    if (flyout) flyout.style.display = 'none';
  },

  updateDijkstraHud(currentStep) {
    const hud = document.getElementById('graphDijkstraHud');
    if (!hud) return;

    if (this.activeCandidatePath && this.activeCandidatePath.info) {
      const cand = this.activeCandidatePath.info;
      const isWinner = (cand.flow_allocated || 0) > 0;
      const isOrigin = cand.is_origin;
      const badgeClass = isWinner ? 'badge-emerald' : (isOrigin ? 'badge-purple' : 'badge-amber');
      const badgeLabel = isWinner ? 'SELECTED ROUTE' : (isOrigin ? 'ORIGIN RESERVE' : 'CANDIDATE EVALUATION');
      const formulaStr = cand.cost_formula?.formula_string || `C = $${cand.cost_formula?.unit_cost || 0}`;

      hud.innerHTML = `
        <div style="display:flex; align-items:center; gap:10px; flex-wrap:wrap;">
          <span class="badge ${badgeClass}" style="font-size:0.68rem; font-weight:700;">${badgeLabel}</span>
          <span style="font-family:var(--font-mono); font-size:0.85rem; font-weight:700; color:var(--text-primary);">
            Source (S) &rarr; ${this.activeCandidatePath.from} &rarr; <span style="color:${isWinner ? 'var(--accent-emerald)' : 'var(--accent-amber)'};">${this.activeCandidatePath.to}</span> &rarr; Sink (T)
          </span>
        </div>
        <div style="display:flex; align-items:center; gap:16px; flex-wrap:wrap; font-size:0.75rem;">
          <span style="color:var(--text-secondary);">
            Unit Cost: <strong style="color:var(--accent-blue); font-family:var(--font-mono);">$${cand.cost_formula?.unit_cost?.toFixed(2) || '0.00'}/req</strong>
            <span style="font-size:0.68rem; color:var(--text-muted); font-family:var(--font-mono);">(${formulaStr})</span>
          </span>
          <span style="color:var(--text-secondary);">
            Latency SLA: <strong style="color:${isWinner ? 'var(--accent-emerald)' : 'var(--text-primary)'}; font-family:var(--font-mono);">${cand.link_effective_latency_ms.toFixed(1)} ms</strong>
          </span>
          <span style="color:var(--text-secondary);">
            Throughput: <strong style="color:${isWinner ? 'var(--accent-emerald)' : 'var(--text-muted)'}; font-family:var(--font-mono);">${Math.round(cand.flow_allocated || 0).toLocaleString()} req/s</strong>
          </span>
          <span style="color:var(--text-secondary);">
            Link Headroom: <strong style="font-family:var(--font-mono);">${cand.link_remaining_bandwidth > 90000 ? '∞' : Math.round(cand.link_remaining_bandwidth).toLocaleString()} req/s free</strong>
          </span>
        </div>
      `;
      hud.style.display = 'flex';
      return;
    }

    if (!currentStep) {
      hud.style.display = 'none';
      return;
    }

    const hasAugment = currentStep.bottleneckFlow > 0 && currentStep.pathNodeNames && currentStep.pathNodeNames.length >= 2;

    if (hasAugment) {
      const pathStr = currentStep.pathNodeNames.join(' &rarr; ');
      const bottleneck = Math.round(currentStep.bottleneckFlow).toLocaleString();

      hud.innerHTML = `
        <div style="display:flex; align-items:center; gap:8px;">
          <span class="badge badge-emerald" style="font-size:0.65rem;">AUGMENTING PATH</span>
          <span style="font-family:var(--font-mono); font-size:0.8rem; font-weight:700; color:var(--text-primary);">${pathStr}</span>
        </div>
        <div style="display:flex; align-items:center; gap:16px;">
          <span style="font-size:0.75rem; color:var(--text-secondary);">
            Bottleneck Flow <strong style="color:var(--accent-blue);">&Delta;f: ${bottleneck} req/s</strong>
          </span>
          <span style="font-size:0.75rem; color:var(--text-secondary);">
            Action: <strong style="color:var(--accent-emerald);">${currentStep.action || 'Push Flow'}</strong>
          </span>
        </div>
      `;
      hud.style.display = 'flex';
    } else {
      hud.style.display = 'none';
    }
  },

  highlightRegion(regionName) {
    this.highlightedRegion = regionName;
    this.renderCurrentStep();
  },

  highlightCandidatePath(fromRegion, toServer, candidateInfo) {
    this.activeCandidatePath = {
      from: fromRegion,
      to: toServer,
      info: candidateInfo
    };
    this.highlightedRegion = fromRegion;
    this.renderCurrentStep();
  },

  clearCandidatePath() {
    this.activeCandidatePath = null;
    this.renderCurrentStep();
  }
};

