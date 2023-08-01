import React, {useState} from 'react';
import logo from './logo.svg';
import '../node_modules/mini.css/dist/mini-default.min.css';
import './App.css';
import ConfigPage from './components/ConfigPage';
import { MOCK_CONFIG } from './components/MockConfig';
import { Config } from './components/Config';


export interface GlobalState {
  config: Config
}

const App: React.FC = () => {
  const [value, setValue] = useState<GlobalState>({config: MOCK_CONFIG});
  // const [value, setValue] = useState<GlobalState>({config: new Config()});

  return (
    <>
      <header>
      <img src={logo} className="App-logo" alt="logo" />
        <a href="#" className="button">Home</a>
        <a href="#" className="button">Config</a>
      </header>
      <div className="container">
        <ConfigPage config={value.config} />
      </div>
    </>
  );
}

export default App;
