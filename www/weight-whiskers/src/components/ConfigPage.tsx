import React, { useState, useEffect } from "react";
import { Config } from "./Config";
import { RJSFSchema } from '@rjsf/utils';
import validator from '@rjsf/validator-ajv8';
import Form, { IChangeEvent } from '@rjsf/core';

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


const ConfigPage = () => {
  console.log("Render config page");
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
  // console.log("Render config page", {config: new Config()});
  const [state, setState] = useState<Config>(initConfig);

  // function onSubmit(data: z.infer<typeof ConfigSchema>) {
  const onSubmit = ({formData} : IChangeEvent<any, RJSFSchema, any>, e: React.FormEvent) => {
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
        // ...
      })
      .catch((error) => {
        // ...
      })
  }

  // load config
  useEffect(() => {
    request<Config>('/api/config').then(config => {
      console.log("Got config", config);
      setState(config);
      console.log("new state: ", state);
    })
  }, [])

  console.log("Render with state: ", state);
  const log = (type: string) => console.log.bind(console, type);

  return (
    <>
      <h1>Config</h1>
      <Form
        schema={schema}
        validator={validator}
        onChange={log('changed')}
        onSubmit={onSubmit}
        onError={log('errors')}
        formData={state}
      />
    </>
  );
}

export default ConfigPage;