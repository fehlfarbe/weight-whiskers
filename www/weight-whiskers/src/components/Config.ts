export interface Config {
  mqttServer: string | undefined;
  mqttPort: number | undefined;
  mqttTopicCatWeight: string | undefined;
  mqttTopicCurrentWeight: string | undefined;
  scaleCalibValue: number | undefined;
  scaleCalibWeight: number | undefined;
  scaleWeightMin: number | undefined;
  scaleTareTime: number | undefined;
  scaleTareThresh: number | undefined;

  // get scaleTareTimeSeconds(): number {
  //   if (this.scaleTareTime)
  //     return this.scaleTareTime / 1000.;
  //   return 0;
  // }

  // constructor(initializer?: any) {
  //   if (!initializer) return;
  //   if (initializer.mqtt_server) this.mqttServer = initializer.mqtt_server;
  //   if (initializer.mqtt_port) this.mqttPort = initializer.mqtt_port;
  //   if (initializer.mqtt_topic_cat_weight) this.mqttTopicCatWeight = initializer.mqtt_topic_cat_weight;
  //   if (initializer.mqtt_topic_current_weight) this.mqttTopicCurrentWeight = initializer.mqtt_topic_current_weight;
  //   if (initializer.scale_calib_value) this.scaleCalibValue = initializer.scale_calib_value;
  //   if (initializer.scale_calib_weight) this.scaleCalibWeight = initializer.scale_calib_weight;
  //   if (initializer.scale_weight_min) this.scaleWeightMin = initializer.scale_weight_min;
  //   if (initializer.scale_tare_time) this.scaleTareTime = initializer.scale_tare_time;
  //   if (initializer.scale_tare_thresh) this.scaleTareThresh = initializer.scale_tare_thresh;
  // }
}