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
  });

  const [currentDistance, setCurrentDistance] = useState('N/A');
  const [appList, setAppList] = useState([]);

  // On component mount, fetch status periodically
  useEffect(() => {
    // Fetch initial settings from backend
    fetch(`${API_URL}/settings/`)
      .then(res => res.json())
      .then(data => setSettings(prev => ({ ...prev, ...data }))) // Merge with defaults
      .catch(err => console.error("Failed to fetch settings:", err));

    // Fetch application list
    fetch(`${API_URL}/applications/`)
      .then(res => res.json())
      .then(data => {
        setAppList(data);
      })
      .catch(err => console.error("Failed to fetch app list:", err));

    const interval = setInterval(() => {
      fetch(`${API_URL}/status/`)
        .then(res => res.json())
        .then(data => setCurrentDistance(data.distance))
        .catch(err => console.error("Failed to fetch status:", err));
    }, 1000);

    // Cleanup interval on unmount
    return () => clearInterval(interval);
  }, []);

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
    .then(data => console.log('Backend updated with scheme:', data))
    .catch(err => console.error("Failed to update backend:", err));
  };

  const toggleHardware = (device) => {
    fetch(`${API_URL}/hardware/${device}/`, { method: 'POST' })
      .then(res => res.json())
       .then(data => alert(`${data.device.toUpperCase()} is now ${data.status}`))
      .catch(err => console.error(`Failed to toggle ${device}:`, err));
  };

  return (
    <div className="App">
      <header className="App-header">
        <h1>StealthGuard</h1>
        <div className="status-display">
          Current Distance: <span>{currentDistance} cm</span>
        </div>
      </header>
      <main className="main-content">
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
              <button onClick={() => toggleHardware('led')} className="secondary">Toggle LED</button>
              <button onClick={() => toggleHardware('buzzer')} className="secondary">Toggle Buzzer</button>
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
