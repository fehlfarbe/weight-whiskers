import React, { useState, useEffect } from "react";
import { LoadingImage } from "./Loading";

export interface SystemStateFlash {
  total: number | undefined;
  used: number | undefined;
  config: number | undefined;
  measurements: number | undefined;
}

export interface SystemStateWifi {
  rssi: number | undefined;
}

export interface SystemStateSystem {
  cpu: string | undefined;
  freq: number | undefined;
  heapSize: number | undefined;
  heapFree: number | undefined;
  heapMin: number | undefined;
  heapMax: number | undefined;
}

export interface SystemState {
  flash: SystemStateFlash | undefined;
  wifi: SystemStateWifi | undefined;
  system: SystemStateSystem | undefined;
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
    wifi: undefined,
    system: undefined
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
          ? <LoadingImage></LoadingImage>
          : <><div>
            <h2>Disk usage</h2>
            <ul>
              <li>System disk usage {state.flash?.used}/{state.flash?.total} bytes
                <progress value={state.flash?.used} max={state.flash?.total} className="primary"></progress>
              </li>
              <li>Config file size: {state.flash?.config} bytes</li>
              <li>Measurements file size: {state.flash?.measurements} bytes</li>
            </ul>
          </div>
            <div>
              <h2>WiFi</h2>
              <span>WiFi RSSI: {state.wifi?.rssi} dB</span>
            </div>
            <div>
              <h2>System</h2>
              <ul>
                <li>CPU: {state.system?.cpu} @ {state.system?.freq} MHz</li>
                <li>Heap {(state.system?.heapSize ?? 0) - (state.system?.heapFree ?? 0)}/{state.system?.heapSize} bytes
                  <progress value={(state.system?.heapSize ?? 0) - (state.system?.heapFree ?? 0)} max={state.system?.heapSize} className="primary"></progress>
                </li>
              </ul>
            </div>
          </>
        }
      </div>
    </>
  );
}

export default SystemInfoComponent;