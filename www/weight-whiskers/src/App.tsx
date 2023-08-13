import React, { useState } from 'react';
import { Routes, Route } from 'react-router-dom';
import logo from './logo.svg';
import '../node_modules/mini.css/dist/mini-default.min.css';
import './App.css';
import ConfigPage from './components/ConfigPage';
import MeasurementHistory from './components/MeasurementHistory';
import LiveMeasurements from './components/LiveMeasurements';


const App: React.FC = () => {
  return (
    <>
      <header>
        <img src={logo} className="App-logo" alt="logo" />
        <a href="/" className="button">Overview</a>
        <a href="/config" className="button">Config</a>
        <a href="/live" className="button">Live data</a>
        
      </header>
      <div className="container">
      <Routes>
          <Route path='/' element={<MeasurementHistory />} />
          <Route path='/config' element={<ConfigPage />} />
          <Route path='/live' element={<LiveMeasurements />} />
        </Routes>
      </div>
    </>
  );
}

export default App;
