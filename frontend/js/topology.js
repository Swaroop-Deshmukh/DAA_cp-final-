/**
 * Network Topology Visualizer (SVG)
 * Renders interactive nodes (Regions, Edge Servers, Origin) with real-time link flows.
 */

export const TopologyViewer = {
  containerId: 'topology-svg',
  onNodeClick: null,

  // Node Layout Coordinates in 960x480 coordinate space
  nodes: {
    // Left: 5 Client Regions
    'reg-pune':      { x: 140, y: 70,  type: 'region', label: 'Pune',      sub: 'Demand: 2,500 req/s', id: 0, name: 'Pune' },
    'reg-mumbai':    { x: 140, y: 155, type: 'region', label: 'Mumbai',    sub: 'Demand: 3,500 req/s', id: 1, name: 'Mumbai' },
    'reg-delhi':     { x: 140, y: 240, type: 'region', label: 'Delhi',     sub: 'Demand: 2,000 req/s', id: 2, name: 'Delhi' },
    'reg-bangalore': { x: 140, y: 325, type: 'region', label: 'Bangalore', sub: 'Demand: 2,500 req/s', id: 3, name: 'Bangalore' },
    'reg-hyderabad': { x: 140, y: 410, type: 'region', label: 'Hyderabad', sub: 'Demand: 1,500 req/s', id: 4, name: 'Hyderabad' },

    // Middle-Right: 4 Edge Servers
    'srv-pune':      { x: 540, y: 90,  type: 'server', label: 'Pune Server',      sub: 'Cap: 5,000 req/s', id: 0, name: 'Pune Server', healthy: true },
    'srv-mumbai':    { x: 540, y: 190, type: 'server', label: 'Mumbai Server',    sub: 'Cap: 6,000 req/s', id: 1, name: 'Mumbai Server', healthy: true },
    'srv-delhi':     { x: 540, y: 290, type: 'server', label: 'Delhi Server',     sub: 'Cap: 4,500 req/s', id: 2, name: 'Delhi Server', healthy: true },
    'srv-bangalore': { x: 540, y: 390, type: 'server', label: 'Bangalore Server', sub: 'Cap: 5,000 req/s', id: 3, name: 'Bangalore Server', healthy: true },

    // Far-Right: Origin Central DC
    'origin':        { x: 820, y: 240, type: 'origin', label: 'Origin Server',    sub: 'Capacity: Unlimited', id: -1, name: 'Origin Server' }
  },

  init(onNodeSelect) {
    this.onNodeClick = onNodeSelect;
    this.render();
  },

  render(networkData = null, routingAllocations = null) {
    const svg = document.getElementById(this.containerId);
    if (!svg) return;

    // Apply live network status if provided
    if (networkData && networkData.servers) {
      networkData.servers.forEach(srv => {
        const key = this.getServerKey(srv.name);
        if (key && this.nodes[key]) {
          const isHealthy = srv.isActive ?? srv.isHealthy ?? srv.healthy ?? true;
          const cap = srv.configuredCapacity ?? srv.capacity ?? 5000;
          const avail = srv.availableCapacity ?? (isHealthy ? cap : 0);
          this.nodes[key].healthy = isHealthy;
          this.nodes[key].capacity = cap;
          this.nodes[key].sub = isHealthy ? `Active: ${avail.toLocaleString()} req/s` : 'OFFLINE (Outage)';
        }
      });
    }

    if (networkData && networkData.regions) {
      networkData.regions.forEach(reg => {
        const key = this.getRegionKey(reg.name);
        if (key && this.nodes[key]) {
          this.nodes[key].sub = `Demand: ${(reg.trafficDemand || 0).toLocaleString()} req/s`;
        }
      });
    }

    // Build SVG Elements
    let svgHtml = `
      <defs>
        <filter id="glow-cyan" x="-20%" y="-20%" width="140%" height="140%">
          <feGaussianBlur stdDeviation="3" result="blur" />
          <feComposite in="SourceGraphic" in2="blur" operator="over" />
        </filter>
        <filter id="glow-rose" x="-20%" y="-20%" width="140%" height="140%">
          <feGaussianBlur stdDeviation="4" result="blur" />
          <feComposite in="SourceGraphic" in2="blur" operator="over" />
        </filter>
      </defs>
    `;

    // 1. Draw Links (Edges)
    const links = this.generateLinks(routingAllocations);
    links.forEach(link => {
      svgHtml += `
        <path class="topo-edge ${link.status}"
              d="M ${link.x1} ${link.y1} C ${(link.x1 + link.x2)/2} ${link.y1}, ${(link.x1 + link.x2)/2} ${link.y2}, ${link.x2} ${link.y2}"
              data-from="${link.from}" data-to="${link.to}" />
      `;
    });

    // 2. Draw Nodes
    Object.entries(this.nodes).forEach(([key, node]) => {
      let nodeColor = '#3b82f6';
      let strokeColor = '#60a5fa';

      if (node.type === 'server') {
        if (!node.healthy) {
          nodeColor = '#ef4444';
          strokeColor = '#f87171';
        } else {
          nodeColor = '#10b981';
          strokeColor = '#34d399';
        }
      } else if (node.type === 'origin') {
        nodeColor = '#8b5cf6';
        strokeColor = '#a78bfa';
      }

      svgHtml += `
        <g class="topo-node" data-id="${key}" transform="translate(${node.x}, ${node.y})">
          <circle r="22" fill="${nodeColor}" fill-opacity="0.18" stroke="${strokeColor}" />
          <circle r="8" fill="${strokeColor}" />
          <text y="-28">${node.label}</text>
          <text y="38" class="sub">${node.sub}</text>
        </g>
      `;
    });

    svg.innerHTML = svgHtml;

    // Attach Click Handlers
    svg.querySelectorAll('.topo-node').forEach(elem => {
      elem.addEventListener('click', () => {
        const id = elem.getAttribute('data-id');
        if (this.onNodeClick && this.nodes[id]) {
          this.onNodeClick(this.nodes[id]);
        }
      });
    });
  },

  getRegionKey(name) {
    const s = String(name).toLowerCase();
    if (s.includes('pune')) return 'reg-pune';
    if (s.includes('mumbai')) return 'reg-mumbai';
    if (s.includes('delhi')) return 'reg-delhi';
    if (s.includes('bangalore')) return 'reg-bangalore';
    if (s.includes('hyderabad')) return 'reg-hyderabad';
    return null;
  },

  getServerKey(identifier) {
    const s = String(identifier).toLowerCase();
    if (s.includes('pune')) return 'srv-pune';
    if (s.includes('mumbai')) return 'srv-mumbai';
    if (s.includes('delhi')) return 'srv-delhi';
    if (s.includes('bangalore')) return 'srv-bangalore';
    if (s.includes('origin')) return 'origin';
    return null;
  },

  generateLinks(allocations) {
    const baseLinks = [
      // Pune Region Links
      { from: 'Pune', to: 'Pune Server', fromKey: 'reg-pune', toKey: 'srv-pune', x1: 140, y1: 70, x2: 540, y2: 90 },
      { from: 'Pune', to: 'Mumbai Server', fromKey: 'reg-pune', toKey: 'srv-mumbai', x1: 140, y1: 70, x2: 540, y2: 190 },

      // Mumbai Region Links
      { from: 'Mumbai', to: 'Mumbai Server', fromKey: 'reg-mumbai', toKey: 'srv-mumbai', x1: 140, y1: 155, x2: 540, y2: 190 },
      { from: 'Mumbai', to: 'Pune Server', fromKey: 'reg-mumbai', toKey: 'srv-pune', x1: 140, y1: 155, x2: 540, y2: 90 },

      // Delhi Region Links
      { from: 'Delhi', to: 'Delhi Server', fromKey: 'reg-delhi', toKey: 'srv-delhi', x1: 140, y1: 240, x2: 540, y2: 290 },
      { from: 'Delhi', to: 'Mumbai Server', fromKey: 'reg-delhi', toKey: 'srv-mumbai', x1: 140, y1: 240, x2: 540, y2: 190 },

      // Bangalore Region Links
      { from: 'Bangalore', to: 'Bangalore Server', fromKey: 'reg-bangalore', toKey: 'srv-bangalore', x1: 140, y1: 325, x2: 540, y2: 390 },
      { from: 'Bangalore', to: 'Pune Server', fromKey: 'reg-bangalore', toKey: 'srv-pune', x1: 140, y1: 325, x2: 540, y2: 90 },

      // Hyderabad Region Links
      { from: 'Hyderabad', to: 'Bangalore Server', fromKey: 'reg-hyderabad', toKey: 'srv-bangalore', x1: 140, y1: 410, x2: 540, y2: 390 },
      { from: 'Hyderabad', to: 'Pune Server', fromKey: 'reg-hyderabad', toKey: 'srv-pune', x1: 140, y1: 410, x2: 540, y2: 90 },

      // Edge Servers to Origin (Fallback)
      { from: 'Pune Server', to: 'Origin Server', fromKey: 'srv-pune', toKey: 'origin', x1: 540, y1: 90, x2: 820, y2: 240 },
      { from: 'Mumbai Server', to: 'Origin Server', fromKey: 'srv-mumbai', toKey: 'origin', x1: 540, y1: 190, x2: 820, y2: 240 },
      { from: 'Delhi Server', to: 'Origin Server', fromKey: 'srv-delhi', toKey: 'origin', x1: 540, y1: 290, x2: 820, y2: 240 },
      { from: 'Bangalore Server', to: 'Origin Server', fromKey: 'srv-bangalore', toKey: 'origin', x1: 540, y1: 390, x2: 820, y2: 240 }
    ];

    if (allocations && allocations.length > 0) {
      return baseLinks.map(l => {
        const isFlowActive = allocations.some(a => {
          const from = a.from || a.from_node || a.region_name || '';
          const to = a.to || a.to_node || a.server_name || '';
          const flow = a.flow ?? a.allocated_traffic ?? 0;
          return from.toLowerCase().includes(l.from.toLowerCase()) && 
                 to.toLowerCase().includes(l.to.toLowerCase()) && 
                 flow > 0;
        });

        const isOrigin = l.to.toLowerCase().includes('origin');
        let status = 'topo-edge';
        if (isFlowActive) {
          status = isOrigin ? 'fallback active' : 'active';
        }
        return { ...l, status };
      });
    }

    return baseLinks.map(l => ({ ...l, status: 'active' }));
  }
};
