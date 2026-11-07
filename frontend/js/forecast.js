/**
 * Forecasting & Cache View Controller
 * Visualizes Holt Linear Smoothing traffic forecasts & 0/1 Knapsack cache placement.
 */

import { ApiService } from './api.js';
import { ChartManager } from './charts.js';

export const ForecastAndCache = {
  currentRegion: 'Mumbai',
  cacheBudget: 350,

  init() {
    const regionSelect = document.getElementById('forecastRegionSelect');
    if (regionSelect) {
      regionSelect.addEventListener('change', (e) => {
        this.currentRegion = e.target.value;
        this.loadForecast(this.currentRegion);
      });
    }

    const budgetSelect = document.getElementById('cacheBudgetInput');
    if (budgetSelect) {
      budgetSelect.addEventListener('change', (e) => {
        this.cacheBudget = parseInt(e.target.value, 10) || 350;
        this.loadCache(this.cacheBudget);
      });
    }

    this.loadForecast(this.currentRegion);
    this.loadCache(this.cacheBudget);
  },

  async loadForecast(region) {
    try {
      const data = await ApiService.getForecast(region, 5);
      const historical = data.historicalDemand || data.historical || [];
      const forecast = data.forecastSteps || data.forecast || [];
      ChartManager.renderForecastChart(historical, forecast, data.region || region);

      const trendEl = document.getElementById('forecastTrendIndicator');
      if (trendEl) {
        const trend = data.finalTrend !== undefined ? data.finalTrend : 12.5;
        const isUp = trend > 0;
        trendEl.innerHTML = `
          <span class="badge ${isUp ? 'badge-amber' : 'badge-emerald'}">
            ${isUp ? 'EXPANDING' : 'CONTRACTING'} (${isUp ? '+' : ''}${trend.toFixed(1)} req/s/period)
          </span>
        `;
      }
    } catch (err) {
      console.error('Failed to load forecast:', err);
    }
  },

  async loadCache(budgetMb) {
    try {
      const data = await ApiService.getCachePlacement(budgetMb);
      this.renderKnapsack(data);
    } catch (err) {
      console.error('Failed to load cache knapsack:', err);
    }
  },

  renderKnapsack(data) {
    const totalCap = data.cacheBudgetMB || data.capacity_gb || 350;
    const usedCap = data.totalSizeUsedMB !== undefined ? data.totalSizeUsedMB : (data.total_weight_used_gb || 0);
    const totalVal = data.totalPopularityValue !== undefined ? data.totalPopularityValue : (data.total_value || 0);
    const utilPct = data.utilizationPct !== undefined ? data.utilizationPct : (totalCap > 0 ? (usedCap * 100 / totalCap) : 0);

    // Selected item IDs set
    const selectedIds = new Set((data.selectedItems || []).map(it => it.id));
    const catalog = data.availableCatalog || data.items || [];

    // KPI Summary
    const capEl = document.getElementById('cacheUsedCapacity');
    const valEl = document.getElementById('cacheTotalValue');
    const hitEl = document.getElementById('cacheEstimatedHitRate');

    if (capEl) capEl.textContent = `${usedCap} / ${totalCap} MB`;
    if (valEl) valEl.textContent = `${totalVal.toLocaleString()} pts`;
    if (hitEl) {
      const hitPct = Math.min(94, Math.round(utilPct * 0.95));
      hitEl.textContent = `${hitPct}%`;
    }

    // Visual Knapsack Bar
    const barEl = document.getElementById('cacheKnapsackBar');
    if (barEl) {
      const colors = ['#2563eb', '#3b82f6', '#10b981', '#f59e0b', '#8b5cf6', '#ec4899'];
      const selectedList = data.selectedItems || [];
      barEl.innerHTML = selectedList.map((it, idx) => {
        const size = it.sizeMB || it.size_gb || it.size || 1;
        const widthPct = Math.max(8, (size / totalCap) * 100);
        return `
          <div class="knapsack-segment" style="width: ${widthPct}%; background-color: ${colors[idx % colors.length]};" title="${it.name} (${size} MB)">
            ${it.name}
          </div>
        `;
      }).join('');
    }

    // Items Table
    const tbody = document.getElementById('cacheItemsTableBody');
    if (tbody) {
      tbody.innerHTML = catalog.map(it => {
        const isSelected = selectedIds.has(it.id);
        const size = it.sizeMB !== undefined ? it.sizeMB : (it.size_gb || it.size || 0);
        const val = it.popularityValue !== undefined ? it.popularityValue : (it.value || 0);
        const ratio = size > 0 ? (val / size).toFixed(2) : '—';

        return `
          <tr class="${isSelected ? 'strong' : ''}">
            <td>${it.name}</td>
            <td class="mono">${size} MB</td>
            <td class="mono">${val}</td>
            <td class="mono">${ratio}</td>
            <td>
              ${isSelected
                ? '<span class="badge badge-emerald"><span class="badge-dot"></span>CACHED AT EDGE</span>'
                : '<span class="badge badge-gray"><span class="badge-dot"></span>ORIGIN BYPASS</span>'}
            </td>
          </tr>
        `;
      }).join('');
    }
  }
};
