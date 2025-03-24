import React, { useState } from 'react';

const App = () => {
  const [response, setResponse] = useState(null); 
  const [loading, setLoading] = useState(false);  
  const [error, setError] = useState(null);       

  const handleGetRequest = (endpoint) => {
    setLoading(true); 
    setError(null);   

    fetch(`http://localhost:3000/${endpoint}`)
      .then((response) => {
        if (!response.ok) {
          throw new Error('Network response was not ok');
        }
        return response.json();
      })
      .then((data) => {
        setResponse(data); 
        setLoading(false); 
      })
      .catch((error) => {
        setError('Failed to fetch message from the server.');
        setLoading(false);
        console.error('Error fetching data:', error);
      });
  };

  return (
    <div className="App">
      <h1>Test options</h1>
      <div>
        <button onClick={() => handleGetRequest('load?name=datapackage&path=../Data/owid-deaths/datapackage.json')}>Load owid</button>
        <button onClick={() => handleGetRequest('unload?name=datapackage')}>Unload</button>
        <button onClick={() => handleGetRequest('stop')}>Stop server</button>
      </div>

      {loading && <p>Loading...</p>}

      {error && <p style={{ color: 'red' }}>{error}</p>}

      {response && (
        <div>
          <h2>Response:</h2>
          <pre>{JSON.stringify(response, null, 2)}</pre>
        </div>
      )}
    </div>
  );
};

export default App;
