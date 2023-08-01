import { Config } from './Config';

export const MOCK_CONFIG = 
  new Config({
    mqtt_server: "localhost",
    mqtt_port: 1883,
    mqtt_topic_cat_weight: "home/cat/scale/measured",
    mqtt_topic_current_weight: "home/cat/scale/current",
    scale_calib_value: 1.0,
    scale_calib_weight: 500,
    scale_weight_min: 2000,
    scale_tare_time: 60000,
    scale_tare_thresh: 50
  });