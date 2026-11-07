/**
 * Event Logging Module
 * Maintains the timestamped audit log of routing decisions, failovers, and simulator events.
 */

export const EventLogManager = {
  logs: [],
  listeners: [],

  init() {
    this.add('SYSTEM', 'Adaptive CDN Router NOC initialized in simulation environment', 'INFO');
  },

  add(category, message, level = 'INFO', details = null) {
    const entry = {
      id: Date.now() + Math.random().toString(36).substring(2, 7),
      timestamp: new Date().toLocaleTimeString('en-US', { hour12: false, hour: '2-digit', minute: '2-digit', second: '2-digit' }),
      category,
      message,
      level, // INFO, WARN, ERROR, REROUTE
      details
    };
    this.logs.unshift(entry);
    if (this.logs.length > 500) this.logs.pop();
    this.notify();
    return entry;
  },

  subscribe(callback) {
    this.listeners.push(callback);
    callback(this.logs);
  },

  notify() {
    this.listeners.forEach(fn => fn(this.logs));
  },

  filter(category = 'ALL', level = 'ALL', search = '') {
    return this.logs.filter(item => {
      const matchCat = category === 'ALL' || item.category === category;
      const matchLevel = level === 'ALL' || item.level === level;
      const matchSearch = !search || item.message.toLowerCase().includes(search.toLowerCase()) || item.category.toLowerCase().includes(search.toLowerCase());
      return matchCat && matchLevel && matchSearch;
    });
  }
};

