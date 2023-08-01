import React, { useState } from "react";
import { GlobalState } from "../App";
import { Config } from "./Config"; import { z } from "zod"
import TypeSafeForm from "./TypeSafeForm";

const ConfigSchema = z.object({
  mqttServer: z.string().describe("MQTT server"),
  mqttPort: z.number().describe("MQTT port"),
  mqttTopicCatWeight: z.string().describe("MQTT topic cat weight"),
  mqttTopicCurrentWeight: z.string().describe("MQTT topic current weight"),
  scaleCalibValue: z.number().describe("Scale calib value (is calculated)"),
  scaleCalibWeight: z.number().describe("Scale calib weight (gram)"),
  scaleWeightMin: z.number().describe("Scale minimum weight (gram)"),
  scaleTareTime: z.number().describe("Scale tare time (ms)"),
  scaleTareThresh: z.number().describe("Scale auto tare threshold (gram)")
})

function ConfigPage({ config }: GlobalState) {
  console.log("Render config page", config);
  const [formData, setFormData] = React.useState<Config>(config);

  function onSubmit(data: z.infer<typeof ConfigSchema>) {
    // retrieve type-safe data when the form is submitted
    console.log("Update config");
    const requestBody = {
      mqtt_server: data.mqttServer,
      mqtt_port: data.mqttPort,
      mqtt_topic_cat_weight: data.mqttTopicCatWeight,
      mqtt_topic_current_weight: data.mqttTopicCurrentWeight,
      scale_calib_value: data.scaleCalibValue,
      scale_calib_weight: data.scaleCalibWeight,
      scale_weight_min: data.scaleWeightMin,
      scale_tare_time: data.scaleTareTime,
      scale_tare_thresh: data.scaleTareThresh
    }

    fetch("/config", {
      method: "POST",
      headers: {
        "Content-Type": "application/json",
      },
      body: JSON.stringify(requestBody),
    })
      .then((response) => {
        // ...
      })
      .catch((error) => {
        // ...
      })
  }

  return (
    <>
      <h1>Config</h1>
      <TypeSafeForm
        schema={ConfigSchema}
        onSubmit={onSubmit}
        // add the Submit button to the form
        renderAfter={() => <input type="submit" value="Update config" />}
        defaultValues={{
          mqttServer: formData.mqttServer,
          mqttPort: formData.mqttPort,
          mqttTopicCatWeight: formData.mqttTopicCatWeight,
          mqttTopicCurrentWeight: formData.mqttTopicCurrentWeight,
          scaleCalibValue: formData.scaleCalibValue,
          scaleCalibWeight: formData.scaleCalibWeight,
          scaleWeightMin: formData.scaleWeightMin,
          scaleTareTime: formData.scaleTareTime,
          scaleTareThresh: formData.scaleTareThresh
        }}
      />
    </>
  );
}

export default ConfigPage;