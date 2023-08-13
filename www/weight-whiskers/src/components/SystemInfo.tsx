import React, { useState, useEffect } from "react";
import logo from '../logo.svg';

export interface SystemStateFlash {
  total: number | undefined;
  used: number | undefined;
  config: number | undefined;
  measurements: number | undefined;
}

export interface SystemStateWifi {
  rssi: number | undefined;
}

export interface SystemState {
  flash: SystemStateFlash | undefined;
  wifi: SystemStateWifi | undefined;
}


function request<TResponse>(
  url: string,
  // `RequestInit` is a type for configuring 
  // a `fetch` request. By default, an empty object.
  config: RequestInit = {}
): Promise<TResponse> {

  // Inside, we call the `fetch` function with 
  // a URL and config given:
  return fetch(url, config)
    // When got a response call a `json` method on it
    .then((response) => response.json())
    // and return the result data.
    .then((data) => data as TResponse);
}

const SystemInfoComponent = () => {
  const initState = {
    flash: undefined,
    wifi: undefined
  };

  const [state, setState] = useState<SystemState>(initState);

  useEffect(() => {
    console.log("useEffect");
    // load system state
    request<SystemState>('/api/system').then(systemState => {
      console.log("system state: ", systemState);
      setState(systemState);
    })
  }, [])

  return (
    <>
      <div>
        <h1>System info</h1>
        {state.flash === undefined
          ? <img src={logo} className="App-loading" alt="loading" />
          : <><div>
            <h2>Disk usage</h2>
            <span>System disk usage {state.flash?.used}/{state.flash?.total} bytes</span>
            <progress value={state.flash?.used} max={state.flash?.total} className="primary"></progress>
            <div>Config file size: {state.flash?.config} bytes</div>
            <div>Measurements file size: {state.flash?.measurements} bytes</div>
          </div>
            <div>
              <h2>WiFi</h2>
              <span>WiFi RSSI: {state.wifi?.rssi} dB</span>
            </div>
          </>
        }
      </div>
    </>
  );
}

export default SystemInfoComponent;