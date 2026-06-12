export interface Config {
  mqttEnabled: boolean | undefined;
  mqttServer: string | undefined;
  mqttPort: number | undefined;
  mqttUser: string | undefined;
  mqttPass: string | undefined;
  mqttTopicCatWeight: string | undefined;
  mqttTopicCurrentWeight: string | undefined;
  scaleCalibValue: number | undefined;
  scaleCalibWeight: number | undefined;
  scaleWeightMin: number | undefined;
  scaleTareTime: number | undefined;
  scaleTareThresh: number | undefined;
  scaleWeightDeviationPercent: number | undefined;
}