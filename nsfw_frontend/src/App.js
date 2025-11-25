import React, { useState, useEffect } from 'react';
import './App.css';

// Django API base URL
const API_URL = 'http://127.0.0.1:8000/api';

function App() {
  // State for the list of schemes
  const [schemes, setSchemes] = useState([]);
  
  // State for the "Add New Scheme" form
  const [newScheme, setNewScheme] = useState({
    distance_threshold: 50,
    target_volume: 0,
    brightness_steps: 15,
    target_app: 'Microsoft Excel',
  });

  const [currentDistance, setCurrentDistance] = useState('N/A');
  const [appList, setAppList] = useState([]); // 新增 state 來儲存應用程式列表

  // On component mount, fetch status periodically
  useEffect(() => {
    // 獲取應用程式列表
    fetch(`${API_URL}/applications/`)
      .then(res => res.json())
      .then(data => {
        setAppList(data);
        // 將預設選擇的 app 設為列表中的第一個
        if (data.length > 0) {
          setNewScheme(prev => ({ ...prev, target_app: data[0] }));
        }
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
    const { name, value } = e.target;
    setNewScheme(prev => ({ ...prev, [name]: value }));
  };

  const addScheme = (e) => {
    e.preventDefault();
    // Add the new scheme with a unique ID (timestamp)
    const schemeToAdd = { ...newScheme, id: Date.now() };
    const updatedSchemes = [...schemes, schemeToAdd];
    setSchemes(updatedSchemes);
    
    // Find the most sensitive scheme (lowest distance) to send to the backend
    const mostSensitiveScheme = updatedSchemes.reduce((min, s) => 
      s.distance_threshold < min.distance_threshold ? s : min, updatedSchemes[0]
    );

    // Update the backend with the most sensitive scheme
    if (mostSensitiveScheme) {
      updateBackend(mostSensitiveScheme);
    }
  };

  const removeScheme = (idToRemove) => {
    const updatedSchemes = schemes.filter(s => s.id !== idToRemove);
    setSchemes(updatedSchemes);

    // If there are any schemes left, find the new most sensitive one and update backend
    if (updatedSchemes.length > 0) {
      const mostSensitiveScheme = updatedSchemes.reduce((min, s) => 
        s.distance_threshold < min.distance_threshold ? s : min, updatedSchemes[0]
      );
      updateBackend(mostSensitiveScheme);
    } else {
      // Optional: if all schemes are removed, you might want to send a default "safe" state
      // For now, we do nothing, the backend keeps its last setting.
    }
  };

  const updateBackend = (scheme) => {
    fetch(`${API_URL}/settings/`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(scheme)
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
          <div className="panel">
            <h2>Add New Scheme</h2>
            <form onSubmit={addScheme}>
              <div className="form-group">
                <label>Trigger Distance (cm):</label>
                <input type="number" name="distance_threshold" value={newScheme.distance_threshold} onChange={handleInputChange} required />
              </div>
              <div className="form-group">
                <label>Target Volume (0-100):</label>
                <input type="number" name="target_volume" value={newScheme.target_volume} onChange={handleInputChange} required />
              </div>
              <div className="form-group">
                <label>Brightness Reduction Steps:</label>
                <input type="number" name="brightness_steps" value={newScheme.brightness_steps} onChange={handleInputChange} required />
              </div>
              <div className="form-group">
                <label>Switch to Application:</label>
                <div className="select-wrapper">
                  <select name="target_app" value={newScheme.target_app} onChange={handleInputChange} required>
                    {appList.length === 0 ? (
                      <option disabled>Loading apps...</option>
                    ) : (
                      appList.map(app => <option key={app} value={app}>{app}</option>)
                    )}
                  </select>
                </div>
              </div>
              <button type="submit" className="primary">Add Scheme</button>
            </form>
          </div>

          <div className="panel">
            <h2>Active Schemes</h2>
            <ul className="scheme-list">
              {schemes.length > 0 ? schemes.map(s => (
                <li key={s.id} className="scheme-item">
                  <div className="scheme-details">
                    <strong>IF</strong> distance &lt; <strong>{s.distance_threshold} cm</strong>, <br/>
                    <span>THEN switch to <strong>{s.target_app}</strong> (Volume: {s.target_volume}, Brightness: {s.brightness_steps} steps)</span>
                  </div>
                  <button onClick={() => removeScheme(s.id)} className="remove-btn">Remove</button>
                </li>
              )) : (
                <p>
                  No schemes configured. <br/>
                  Add one to get started!
                </p>
              )}
            </ul>
          </div>

          <div className="panel">
            <h2>Hardware Control</h2>
            <div className="hardware-controls">
              <button onClick={() => toggleHardware('led')} className="secondary">Toggle LED</button>
              <button onClick={() => toggleHardware('buzzer')} className="secondary">Toggle Buzzer</button>
            </div>
          </div>
        </div>
      </main>
    </div>
  );
}

export default App;
