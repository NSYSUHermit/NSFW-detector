import React, { useState, useEffect } from 'react';
import './App.css';

// Django API base URL
const API_URL = 'http://127.0.0.1:8000/api';

function App() {
  // A single state object for all settings
  const [settings, setSettings] = useState({
    safe: { volume: 80, brightness: 16 },
    warning: { volume: 40, brightness: 8 },
    danger: { volume: 0, brightness: 0, target_app: 'Microsoft Excel' },
    warning_threshold: 100,
    danger_threshold: 50,
    led_alerts_enabled: true,
    buzzer_alerts_enabled: true,
  });

  // New state to hold the currently active (saved) configuration
  const [activeConfig, setActiveConfig] = useState({
    safe: { volume: 80, brightness: 16 },
    warning: { volume: 40, brightness: 8 },
    danger: { volume: 0, brightness: 0, target_app: 'Microsoft Excel' },
    warning_threshold: 100,
    danger_threshold: 50,
    led_alerts_enabled: true,
    buzzer_alerts_enabled: true,
  });

  const [currentDistance, setCurrentDistance] = useState('N/A');
  const [appList, setAppList] = useState([]);

  const fetchAndUpdateSettings = () => {
    fetch(`${API_URL}/settings/`)
      .then(res => res.json())
      .then(data => {
        setSettings(prev => ({ ...prev, ...data }));
        setActiveConfig(prev => ({ ...prev, ...data }));
      })
      .catch(err => console.error("Failed to fetch settings:", err));
  };

  // On component mount, fetch status periodically
  useEffect(() => {
    fetchAndUpdateSettings(); // Fetch initial settings

    // Fetch application list
    fetch(`${API_URL}/applications/`)
      .then(res => res.json())
      .then(data => {
        setAppList(data);
      })
      .catch(err => console.error("Failed to fetch app list:", err));

    const distanceInterval = setInterval(() => {
      fetch(`${API_URL}/status/`)
        .then(res => res.json())
        .then(data => setCurrentDistance(data.distance))
        .catch(err => console.error("Failed to fetch status:", err));
    }, 1000);
    
    // A smarter way to sync: re-fetch settings when the user focuses on the window.
    // This avoids overwriting their input while they are typing.
    const handleFocus = () => {
      console.log("Window focused, re-syncing settings...");
      fetchAndUpdateSettings();
    };

    window.addEventListener('focus', handleFocus);

    // Cleanup interval on unmount
    return () => {
      clearInterval(distanceInterval);
      window.removeEventListener('focus', handleFocus);
    };
  }, []); // The empty dependency array ensures this runs only once on mount.

  const handleInputChange = (e) => {
    const { name, value, dataset } = e.target;
    const { state } = dataset; // e.g., "safe", "warning", "danger"

    if (state) {
      // It's a state-specific setting (volume, brightness, etc.)
      setSettings(prev => ({
        ...prev,
        [state]: { ...prev[state], [name]: value }
      }));
    } else {
      // It's a global setting (thresholds)
      setSettings(prev => ({ ...prev, [name]: value }));
    }
  };

  const saveSettings = () => {
    fetch(`${API_URL}/settings/`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(settings)
    })
    .then(res => res.json())
    .then(data => {
      console.log('Backend updated with settings:', data);
      setActiveConfig(data); // On successful save, update the active config to match the form
    })
    .catch(err => console.error("Failed to update backend:", err));
  };

  const toggleHardware = (device) => {
    fetch(`${API_URL}/hardware/${device}/`, { method: 'POST' })
      .then(res => res.json())
       .then(data => {
         console.log(data.status);
         fetchAndUpdateSettings(); // Re-fetch settings to sync UI after toggle
       })
      .catch(err => console.error(`Failed to toggle ${device}:`, err));
  };

  const getCurrentStatus = () => {
    if (currentDistance === 'N/A' || activeConfig.warning_threshold === undefined) {
      return { name: 'STANDBY', color: 'var(--text-secondary)' };
    }
    const dist = Number(currentDistance);
    if (dist <= Number(activeConfig.danger_threshold)) {
      return { name: 'DANGER', color: 'var(--danger-color)' };
    }
    if (dist <= Number(activeConfig.warning_threshold)) {
      return { name: 'WARNING', color: 'var(--asu-gold)' };
    }
    return { name: 'SAFE', color: '#28a745' }; // A nice green color
  };

  const status = getCurrentStatus();

  return (
    <div className="App">
      <header className="App-header">
        <img 
          src="https://brandguide.asu.edu/profiles/contrib/webspark/modules/asu_brand/node_modules/@asu/component-header-footer/dist/assets/img/arizona-state-university-logo.png" 
          alt="Arizona State University Logo" 
          className="asu-logo"
        />
        <div className="config-status-bar">
          <strong>Active Config ➔</strong>
          <span className={`status-indicator ${activeConfig.led_alerts_enabled ? 'enabled' : 'disabled'}`}>
            💡 LED: {activeConfig.led_alerts_enabled ? 'ON' : 'OFF'}
          </span>
          <span className={`status-indicator ${activeConfig.buzzer_alerts_enabled ? 'enabled' : 'disabled'}`}>
            🔊 Buzzer: {activeConfig.buzzer_alerts_enabled ? 'ON' : 'OFF'}
          </span>
          <span>✅ Safe: &gt; {activeConfig.warning_threshold}cm (Vol: {activeConfig.safe.volume}, Bright: {activeConfig.safe.brightness})</span>
          <span>⚠️ Warning: &lt; {activeConfig.warning_threshold}cm (Vol: {activeConfig.warning.volume}, Bright: {activeConfig.warning.brightness})</span>
          <span>🚨 Danger: &lt; {activeConfig.danger_threshold}cm (Vol: {activeConfig.danger.volume}, Bright: {activeConfig.danger.brightness}, App: {activeConfig.danger.target_app})</span>
        </div>
      </header>
      <main className="main-content">
        {/* New Main Status Display */}
        <div className="main-status-display" style={{ color: status.color }}>
          <div className="distance-label">Current Distance</div>
          <div className="distance-value">
            {currentDistance}<span>cm</span>
          </div>
          <div className="status-name">{status.name}</div>
        </div>

        <div className="panels-container">
          {/* Safe State Panel */}
          <div className="panel">
            <h2>✅ Safe State</h2>
            <div className="form-group">
              <label>Volume (0-100):</label>
              <input type="number" name="volume" data-state="safe" value={settings.safe.volume} onChange={handleInputChange} />
            </div>
            <div className="form-group">
              <label>Brightness (0-16):</label>
              <input type="number" name="brightness" data-state="safe" value={settings.safe.brightness} onChange={handleInputChange} />
            </div>
          </div>

          {/* Warning State Panel */}
          <div className="panel">
            <h2>⚠️ Warning State</h2>
            <div className="form-group">
              <label>Trigger if distance &lt; (cm):</label>
              <input type="number" name="warning_threshold" value={settings.warning_threshold} onChange={handleInputChange} />
            </div>
            <div className="form-group">
              <label>Volume (0-100):</label>
              <input type="number" name="volume" data-state="warning" value={settings.warning.volume} onChange={handleInputChange} />
            </div>
            <div className="form-group">
              <label>Brightness (0-16):</label>
              <input type="number" name="brightness" data-state="warning" value={settings.warning.brightness} onChange={handleInputChange} />
            </div>
          </div>

          {/* Danger State Panel */}
          <div className="panel">
            <h2>🚨 Danger State</h2>
            <div className="form-group">
              <label>Trigger if distance &lt; (cm):</label>
              <input type="number" name="danger_threshold" value={settings.danger_threshold} onChange={handleInputChange} />
            </div>
            <div className="form-group">
              <label>Volume (0-100):</label>
              <input type="number" name="volume" data-state="danger" value={settings.danger.volume} onChange={handleInputChange} />
            </div>
            <div className="form-group">
              <label>Brightness (0-16):</label>
              <input type="number" name="brightness" data-state="danger" value={settings.danger.brightness} onChange={handleInputChange} />
            </div>
            <div className="form-group">
              <label>Switch to Application:</label>
              <select name="target_app" data-state="danger" value={settings.danger.target_app} onChange={handleInputChange}>
                {appList.map(app => <option key={app} value={app}>{app}</option>)}
              </select>
            </div>
          </div>

          {/* Hardware Control Panel */}
          <div className="panel">
            <h2>Hardware Control</h2>
            <div className="hardware-controls">
              <button onClick={() => toggleHardware('led')} className={`secondary ${activeConfig.led_alerts_enabled ? 'active' : ''}`}>Toggle LED ({activeConfig.led_alerts_enabled ? 'ON' : 'OFF'})</button>
              <button onClick={() => toggleHardware('buzzer')} className={`secondary ${activeConfig.buzzer_alerts_enabled ? 'active' : ''}`}>Toggle Buzzer ({activeConfig.buzzer_alerts_enabled ? 'ON' : 'OFF'})</button>
            </div>
          </div>
        </div>
        <div className="save-button-container">
          <button onClick={saveSettings} className="primary">Save All Settings</button>
        </div>
      </main>
    </div>
  );
}

export default App;
