import React, { useState, useEffect } from "react";
import { Config } from "./Config";
import { RJSFSchema, RJSFValidationError } from '@rjsf/utils';
import validator from '@rjsf/validator-ajv8';
import Form, { IChangeEvent } from '@rjsf/core';
import SystemInfoComponent from "./SystemInfo";
import { LoadingImage } from "./Loading";

const schema: RJSFSchema = {
  "title": "Config",
  "type": "object",
  "required": [],
  "properties": {
    "mqttServer": {
      "type": "string",
      "title": "MQTT Server"
    },
    "mqttPort": {
      "type": "integer",
      "title": "MQTT Port"
    },
    "mqttTopicCatWeight": {
      "type": "string",
      "title": "MQTT topic cat weight"
    },
    "mqttTopicCurrentWeight": {
      "type": "string",
      "title": "MQTT topic current weight"
    },
    "scaleCalibValue": {
      "type": "number",
      "title": "Scale calib value (is calculated)"
    },
    "scaleCalibWeight": {
      "type": "integer",
      "title": "Scale calib weight (gram)"
    },
    "scaleWeightMin": {
      "type": "integer",
      "title": "Scale minimum weight (gram)"
    },
    "scaleTareTime": {
      "type": "integer",
      "title": "Scale tare time (ms)"
    },
    "scaleTareThresh": {
      "type": "integer",
      "title": "Scale auto tare threshold (gram)"
    }
  }
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

class InfoMessage {
  error: boolean = false;
  message: string = "";
}


const ConfigPage = () => {
  const initConfig = {
    mqttServer: "",
    mqttPort: 0,
    mqttTopicCatWeight: "",
    mqttTopicCurrentWeight: "",
    scaleCalibValue: 1,
    scaleCalibWeight: 500,
    scaleWeightMin: 0,
    scaleTareTime: 0,
    scaleTareThresh: 0
  }

  const initInfo = {
    error: false,
    message: ""
  }

  // console.log("Render config page", {config: new Config()});
  const [config, setConfig] = useState<Config>(initConfig);
  const [configLoaded, setConfigLoaded] = useState(false);
  const [info, setInfo] = useState<InfoMessage>(initInfo);

  const onSubmit = ({ formData }: IChangeEvent<any, RJSFSchema, any>, e: React.FormEvent) => {
    // retrieve type-safe data when the form is submitted
    e.preventDefault();
    console.log("Update config", formData);

    fetch("/api/config", {
      method: "POST",
      headers: {
        "Content-Type": "application/json",
      },
      body: JSON.stringify(formData),
    })
      .then((response) => {
        let info = new InfoMessage();
        if (response.status != 200) {
          info.error = true;
          info.message = "Cannot save config! " + response.status + " " + response.statusText;
          setInfo(info);
          console.log("Error");
        } else {
          info.error = false;
          info.message = "Config saved!";
          setInfo(info);
          console.log("Saved");
        }
      })
      .catch((error) => {
        let info = new InfoMessage();
        info.error = true;
        info.message = "Cannot save config! " + error;
        setInfo(info);
        console.log("Error");
      })
  }

  const onError = (errors: RJSFValidationError[]) => {
    console.log("Error saving config " + errors);
  }

  // load config
  useEffect(() => {
    console.log("useEffect");
    request<Config>('/api/config').then(cfg => {
      setConfig(cfg);
      setConfigLoaded(true);
    })
  }, [info, configLoaded])

  const log = (type: string) => console.log.bind(console, type);

  return (
    <>
      <h1>Config</h1>
      {configLoaded
        ? <Form
          schema={schema}
          validator={validator}
          onChange={log('changed')}
          onSubmit={onSubmit}
          onError={onError}
          formData={config}
        />
        : <LoadingImage></LoadingImage>
      }
      <div>
        {info.message
          ? <span className="toast secondary">{info.message}</span>
          : <></>
        }
      </div>
      <div>
        <SystemInfoComponent></SystemInfoComponent>
      </div>
    </>
  );
}

export default ConfigPage;