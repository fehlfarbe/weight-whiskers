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
}