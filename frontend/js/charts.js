/**
 * Chart Management Module (Chart.js)
 * High-performance enterprise charts configured for dark theme.
 */

// Shared dark theme chart options
const darkThemeDefaults = {
  responsive: true,
  maintainAspectRatio: false,
  plugins: {
    legend: {
      labels: {
        color: '#94a3b8',
        font: { family: 'Inter', size: 11 }
      }
    },
    tooltip: {
      backgroundColor: '#1f2937',
      titleColor: '#f8fafc',
      bodyColor: '#cbd5e1',
      borderColor: 'rgba(255, 255, 255, 0.1)',
      borderWidth: 1,
      padding: 10,
      cornerRadius: 6
    }
  },
  scales: {
    x: {
      grid: { color: 'rgba(255, 255, 255, 0.05)' },
      ticks: { color: '#64748b', font: { family: 'Inter', size: 10 } }
    },
    y: {
      grid: { color: 'rgba(255, 255, 255, 0.05)' },
      ticks: { color: '#64748b', font: { family: 'JetBrains Mono', size: 10 } }
    }
  }
};

export const ChartManager = {
  instances: {},

  initOverviewCharts() {
    // 1. Traffic Split Doughnut
    const trafficCtx = document.getElementById('overviewTrafficChart');
    if (trafficCtx) {
      if (this.instances.traffic) this.instances.traffic.destroy();
      this.instances.traffic = new Chart(trafficCtx, {
        type: 'doughnut',
        data: {
          labels: ['Pune Server', 'Mumbai Server', 'Delhi Server', 'Bangalore Server', 'Origin Server'],
          datasets: [{
            data: [2750, 3500, 2000, 3750, 0],
            backgroundColor: ['#2563eb', '#3b82f6', '#10b981', '#f59e0b', '#ef4444'],
            borderWidth: 1,
            borderColor: '#0f1420'
          }]
        },
        options: {
          responsive: true,
          maintainAspectRatio: false,
          plugins: {
            legend: { position: 'bottom', labels: { color: '#94a3b8', font: { size: 10 } } },
            tooltip: darkThemeDefaults.plugins.tooltip
          },
          cutout: '65%'
        }
      });
    }

    // 2. Latency Per Region Bar Chart
    const latencyCtx = document.getElementById('overviewLatencyChart');
    if (latencyCtx) {
      if (this.instances.latency) this.instances.latency.destroy();
      this.instances.latency = new Chart(latencyCtx, {
        type: 'bar',
        data: {
          labels: ['Pune', 'Mumbai', 'Delhi', 'Bangalore', 'Hyderabad'],
          datasets: [{
            label: 'Avg Latency (ms)',
            data: [9.5, 7.5, 10.5, 8.9, 21.0],
            backgroundColor: 'rgba(59, 130, 246, 0.65)',
            borderColor: '#3b82f6',
            borderWidth: 1,
            borderRadius: 4
          }]
        },
        options: {
          ...darkThemeDefaults,
          plugins: { ...darkThemeDefaults.plugins, legend: { display: false } },
          scales: {
            ...darkThemeDefaults.scales,
            y: { ...darkThemeDefaults.scales.y, beginAtZero: true, title: { display: true, text: 'ms', color: '#64748b' } }
          }
        }
      });
    }
  },

  updateOverviewTraffic(serverAllocations, originTraffic) {
    if (!this.instances.traffic) return;
    const labels = Object.keys(serverAllocations);
    const data = Object.values(serverAllocations);
    if (originTraffic > 0) {
      labels.push('Origin Fallback');
      data.push(originTraffic);
    }
    this.instances.traffic.data.labels = labels;
    this.instances.traffic.data.datasets[0].data = data;
    this.instances.traffic.update();
  },

  updateOverviewLatency(regionLatencies) {
    if (!this.instances.latency) return;
    this.instances.latency.data.labels = Object.keys(regionLatencies);
    this.instances.latency.data.datasets[0].data = Object.values(regionLatencies);
    this.instances.latency.update();
  },

  // Comparison Charts
  initComparisonCharts(strategiesData) {
    const costCtx = document.getElementById('comparisonCostChart');
    const latCtx = document.getElementById('comparisonLatencyChart');

    if (!costCtx || !latCtx || !strategiesData) return;

    const labels = strategiesData.map(s => s.strategyName || s.strategy || s.name);
    const costData = strategiesData.map(s => s.averageRoutingCostPerReq ?? s.avg_cost_per_request ?? s.avgCost);
    const latData = strategiesData.map(s => s.averageLatencyMs ?? s.avg_latency_ms ?? s.avgLatency);

    if (this.instances.compCost) this.instances.compCost.destroy();
    this.instances.compCost = new Chart(costCtx, {
      type: 'bar',
      data: {
        labels,
        datasets: [{
          label: 'Avg Cost / Req',
          data: costData,
          backgroundColor: ['#2563eb', '#64748b', '#3b82f6', '#f59e0b'],
          borderRadius: 4
        }]
      },
      options: {
        ...darkThemeDefaults,
        plugins: { ...darkThemeDefaults.plugins, legend: { display: false } },
        scales: { ...darkThemeDefaults.scales, y: { ...darkThemeDefaults.scales.y, beginAtZero: true } }
      }
    });

    if (this.instances.compLat) this.instances.compLat.destroy();
    this.instances.compLat = new Chart(latCtx, {
      type: 'bar',
      data: {
        labels,
        datasets: [{
          label: 'Avg Latency (ms)',
          data: latData,
          backgroundColor: ['#10b981', '#64748b', '#3b82f6', '#f59e0b'],
          borderRadius: 4
        }]
      },
      options: {
        ...darkThemeDefaults,
        plugins: { ...darkThemeDefaults.plugins, legend: { display: false } },
        scales: { ...darkThemeDefaults.scales, y: { ...darkThemeDefaults.scales.y, beginAtZero: true } }
      }
    });
  },

  // Forecast Chart
  renderForecastChart(historical, forecast, regionName) {
    const ctx = document.getElementById('forecastChart');
    if (!ctx) return;

    const histLabels = historical.map((_, i) => `T-${historical.length - i}`);
    const foreLabels = forecast.map((_, i) => `T+${i + 1} (Forecast)`);
    const allLabels = [...histLabels, ...foreLabels];

    const histPadded = [...historical, ...new Array(forecast.length).fill(null)];
    const forePadded = [...new Array(historical.length - 1).fill(null), historical[historical.length - 1], ...forecast];

    if (this.instances.forecast) this.instances.forecast.destroy();
    this.instances.forecast = new Chart(ctx, {
      type: 'line',
      data: {
        labels: allLabels,
        datasets: [
          {
            label: 'Historical Demand (req/s)',
            data: histPadded,
            borderColor: '#3b82f6',
            backgroundColor: 'rgba(59, 130, 246, 0.15)',
            borderWidth: 2,
            pointRadius: 4,
            tension: 0.2,
            fill: true
          },
          {
            label: 'Holt Linear Forecast (req/s)',
            data: forePadded,
            borderColor: '#60a5fa',
            backgroundColor: 'rgba(96, 165, 250, 0.1)',
            borderWidth: 2,
            borderDash: [5, 5],
            pointRadius: 4,
            tension: 0.1,
            fill: true
          }
        ]
      },
      options: {
        ...darkThemeDefaults,
        scales: {
          ...darkThemeDefaults.scales,
          y: { ...darkThemeDefaults.scales.y, title: { display: true, text: 'Requests / sec', color: '#64748b' } }
        }
      }
    });
  }
};
