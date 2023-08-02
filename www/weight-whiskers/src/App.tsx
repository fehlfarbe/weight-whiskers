import React, { useState } from 'react';
import logo from './logo.svg';
import '../node_modules/mini.css/dist/mini-default.min.css';
import './App.css';
import ConfigPage from './components/ConfigPage';


const App: React.FC = () => {
  return (
    <>
      <header>
        <img src={logo} className="App-logo" alt="logo" />
        <a href="#" className="button">Home</a>
        <a href="#" className="button">Config</a>
      </header>
      <div className="container">
        <ConfigPage />
      </div>
    </>
  );
}

export default App;
