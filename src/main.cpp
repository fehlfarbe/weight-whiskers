#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <HX711.h>
#include <LittleFS.h>
#include <Bounce2.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <IotWebConf.h>
#include <IotWebConfOptionalGroup.h>
#include <PubSubClient.h>
#include <FastLED.h>
#include <RunningAverage.h>
#include <RunningMedian.h>
#include <Filters.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>

// Button
#define BUTTON 9
// LED
#define LED_EXTRA 6
#define LED_NUM 1
// HX711 circuit wiring
#define LOADCELL_DOUT_PIN 10
#define LOADCELL_SCK_PIN 11
#define SDA 34
#define SCL 33
// constants
// #define CALIB_WEIGHT 500 // weight in grams
// #define SCALE_MIN_WEIGHT 2000 // weight in grams
#define SCALE_DELAY_MS 100
// #define SCALE_DELTA 10
#define BUFSIZE 55
#define DISPLAY_TEXT_SIZE 4

enum ScaleState
{
  EMPTY,
  OCCUPIED
};

ScaleState state = EMPTY;

// display
Adafruit_SSD1306 display = Adafruit_SSD1306(128, 64, &Wire);

// buttons
Bounce btn = Bounce();

// LED
CRGBArray<LED_NUM> leds;

// scale
HX711 scale;
long scaleLastTimestamp = 0;
long scaleLastTareThreshTimestamp = 0;
// RunningAverage weightAverage(BUFSIZE);
// RunningMedian weightMedian(BUFSIZE);
FilterOnePole weightLowPass = FilterOnePole(LOWPASS, 0.5, 0);

struct CatMeasurement
{
  float weight = 0.;
  float sigma = 0.;
  float variance = 0.;
};
CatMeasurement measuredCatWeightsBuffer[20];
int measuredCatWeightsCounter = 0;

// Config
struct Config
{
  // MQTT
  String mqtt_server = "";
  int mqtt_port = 1883;
  String mqtt_topic_cat_weight = "home/cat/scale/measured";
  String mqtt_topic_current_weight = "home/cat/scale/current";
  // scale
  float scale_calib_value = 1.f;
  int scale_calib_weight = 500; // gram
  int scale_weight_min = 2000;  // gram
  // scale tare triggers
  int scale_tare_time = 60000; // millis
  int scale_tare_thresh = 50;  // gram
};

Config config;

// WiFi
WiFiClient espClient;
PubSubClient mqtt(espClient);

DNSServer dnsServer;
WebServer server(80);

#define CONFIG_VERSION "opt1"
#define STRING_LEN 128

char conf_mqtt_server[STRING_LEN];
char conf_mqtt_port[6];
char conf_mqtt_topic_cat_weight[STRING_LEN];
char conf_mqtt_topic_current_weight[STRING_LEN];

IotWebConf iotWebConf("catscale", &dnsServer, &server, "catscale1234", CONFIG_VERSION);
iotwebconf::OptionalParameterGroup mqttSetupGroup = iotwebconf::OptionalParameterGroup("mqtt", "MQTT", false);
iotwebconf::TextParameter mqttServerParam = iotwebconf::TextParameter("Server", "mqtt_server", conf_mqtt_server, STRING_LEN);
iotwebconf::NumberParameter mqttPortParam = iotwebconf::NumberParameter("Port", "mqtt_port", conf_mqtt_port, 6);
iotwebconf::TextParameter mqttTopicCatWeightParam = iotwebconf::NumberParameter("Topic cat weight", "mqtt_topic_cat_weight", conf_mqtt_topic_cat_weight, STRING_LEN);
iotwebconf::TextParameter mqttTopicCurrentWeightParam = iotwebconf::NumberParameter("Topic current weight", "mqtt_topic_current_weight", conf_mqtt_topic_current_weight, STRING_LEN);

// -- An instance must be created from the class defined above.
iotwebconf::OptionalGroupHtmlFormatProvider optionalGroupHtmlFormatProvider;

// declarations
void loadConfig();
void saveConfig();
void printConfig();
void tare(int count);
void calibrate(float weight);
void displayWeight(float weight);
void handleRoot();
void saveConfigCallback();
void setupMQTT();
void sendMQTTCatWeights();

void setup()
{

  // Serial
  Serial.begin(115200);
  Serial.println("Cat scale");

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  // display
  Wire.begin(SDA, SCL);
  display.begin(SSD1306_EXTERNALVCC, 0x3C); // Address 0x3C for 128x32
  display.display();
  display.clearDisplay();
  display.setTextSize(DISPLAY_TEXT_SIZE);
  display.setTextColor(WHITE);

  // LED
  FastLED.addLeds<WS2811, LED_EXTRA, RGB>(leds, LED_NUM);
  FastLED.setBrightness(50);
  leds[0] = CRGB::Black;
  FastLED.show();

  // setup button button
  btn.attach(BUTTON, INPUT_PULLUP);
  btn.interval(5);

  // Filesystem and config
  if (!LittleFS.begin(true))
  {
    Serial.println("An Error has occurred while mounting LittleFS");
    while(true){
      digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
      delay(500);
    }
  }
  delay(5000);
  loadConfig();
  printConfig();

  // WiFi / WebConf
  mqttSetupGroup.addItem(&mqttServerParam);
  mqttSetupGroup.addItem(&mqttPortParam);
  mqttSetupGroup.addItem(&mqttTopicCatWeightParam);
  mqttSetupGroup.addItem(&mqttTopicCurrentWeightParam);

  iotWebConf.setHtmlFormatProvider(&optionalGroupHtmlFormatProvider);
  iotWebConf.addParameterGroup(&mqttSetupGroup);
  iotWebConf.setConfigSavedCallback(&saveConfigCallback);

  // leds[0] = CRGB::Orange;
  // FastLED.show();
  display.println("WiFi...");
  display.display();

  // Initializing the configuration.
  iotWebConf.init();

  // Set up required URL handlers on the web server.
  server.on("/", handleRoot);
  server.on("/config", []
            { iotWebConf.handleConfig(); });
  server.onNotFound([]()
                    { iotWebConf.handleNotFound(); });

  iotWebConf.goOnLine();

  // set MQTT
  setupMQTT();

  // setup OTA
  // ArduinoOTA.setHostname("catscale");
  // ArduinoOTA.setPassword("catscale");

  // ArduinoOTA.onStart([]()
  //                    {
  //   Serial.println("Start");
  //   display.clearDisplay();
  //   display.setCursor(0, 20);
  //   display.print("OTA Update...");
  //   display.display(); });
  // ArduinoOTA.onEnd([]()
  //                  {
  //   Serial.println("OTA Finished!");
  //   display.clearDisplay();
  //   display.setCursor(0, 20);
  //   display.print("OTA Finished!");
  //   display.display(); });
  // ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
  //                       {
  //   Serial.printf("Update:%u%%\r", (progress / (total / 100)));
  //   display.clearDisplay();
  //   display.setTextSize(2);
  //   display.setCursor(0, 0);
  //   display.printf("Update:%u%%", (progress / (total / 100)));
  //   display.writeFillRect(0, display.height()-10, (display.width()/(float)total)*progress, display.height(), WHITE);
  //   display.display(); });

  // ArduinoOTA.onError([](ota_error_t error)
  //                    {
  //   Serial.printf("Error[%u]: ", error);
  //   display.clearDisplay();
  //   display.setCursor(0, 20);
  //   display.print("OTA Finished!");
  //   display.display();
  //   if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
  //   else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
  //   else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
  //   else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
  //   else if (error == OTA_END_ERROR) Serial.println("End Failed"); });
  // ArduinoOTA.begin();

  // setup scale
  display.print("Setup scale");
  display.display();
  // leds[0] = CRGB::Yellow;
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  scale.set_scale(config.scale_calib_value); // set calibrated scale value from config
  tare(10);
  // leds[0] = CRGB::Green;
  // FastLED.show();

  // uint32_t realSize = ESP.getFlashChipRealSize();
  // uint32_t ideSize = ESP.getFlashChipSize();
  // FlashMode_t ideMode = ESP.getFlashChipMode();

  // Serial.printf("Flash real id:   %08X\n", ESP.getFlashChipId());
  // Serial.printf("Flash real size: %u bytes\n\n", realSize);

  // Serial.printf("Flash ide  size: %u bytes\n", ideSize);
  // Serial.printf("Flash ide speed: %u Hz\n", ESP.getFlashChipSpeed());
  // Serial.printf("Flash ide mode:  %s\n", (ideMode == FM_QIO ? "QIO" : ideMode == FM_QOUT ? "QOUT"
  //                                                                   : ideMode == FM_DIO  ? "DIO"
  //                                                                   : ideMode == FM_DOUT ? "DOUT"
  //                                                                                        : "UNKNOWN"));
  Serial.println("Setup finished!");
  delay(1000);
}

void loop()
{
  // web interface
  iotWebConf.doLoop();

  // over the air update
  ArduinoOTA.handle();

  // MQTT
  mqtt.loop();

  // button handling
  btn.update();
  // Serial.printf("Button state %d, changed %d, duration %d\n", btn.read(), btn.changed(), btn.currentDuration());
  if (btn.changed())
  {
    int deboucedInput = btn.read();
    if (deboucedInput == HIGH)
    {
      if (btn.previousDuration() > 1000)
      {
        Serial.println("Calibrate...");
        calibrate(500);
      }
      else
      {
        tare(10);
      }
    }
  }

  // measure weight
  auto current = millis();
  if (scale.wait_ready_timeout(1000))
  {
    weightLowPass.input(scale.get_units(2));
  }
  else
  {
    Serial.println("HX711 not found.");
    return;
  }

  if (current - scaleLastTimestamp > SCALE_DELAY_MS)
  {
    // scaleLastTimestamp = millis();
    // float weight = 0.f;

    // if (scale.wait_ready_timeout(1000)) {
    //   weight = scale.get_units(2);
    // } else {
    //   Serial.println("HX711 not found.");
    //   return;
    // }

    if (weightLowPass.output() > config.scale_weight_min && state == EMPTY)
    {
      // cat sits on the throne
      state = OCCUPIED;
      leds[0] = CRGB::Yellow;
      FastLED.show();
      auto statistics = RunningStatistics();
      statistics.setWindowSecs(30);
      statistics.setInitialValue(weightLowPass.output());
      while (weightLowPass.output() > config.scale_weight_min)
      {
        weightLowPass.input(scale.get_units(2));
        statistics.input(weightLowPass.output());
        displayWeight(weightLowPass.output());
        delay(10);
      }

      // cat left the throne
      leds[0] = CRGB::Green;
      FastLED.show();
      CatMeasurement measurement;
      measurement.weight = statistics.mean();
      measurement.sigma = statistics.sigma();
      measurement.variance = statistics.variance();
      measuredCatWeightsBuffer[measuredCatWeightsCounter] = measurement; // save value to send via mqtt
      measuredCatWeightsCounter++;
      displayWeight(statistics.mean());
      delay(5000);

      // reset state
      state = EMPTY;
      // weight = scale.get_units(2);
      // displayWeight(weight);
      delay(1000);
      tare(10);
    }

    // write weight to display
    displayWeight(weightLowPass.output());

    // tare if necessary
    if (abs(weightLowPass.output()) < config.scale_tare_thresh)
    {
      scaleLastTareThreshTimestamp = 0;
    }
    else
    {
      if (!scaleLastTareThreshTimestamp)
      {
        scaleLastTareThreshTimestamp = current;
      }
      else if (current - scaleLastTareThreshTimestamp > config.scale_tare_time)
      {
        tare(10);
      }
    }
  }

  // send via MQTT
  if (WiFi.isConnected())
  {
    sendMQTTCatWeights();
  }

  // reset LED
  leds[0] = CRGB::Black;
  FastLED.show();
}

void tare(int count = 10)
{
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Tare...");
  display.display();
  scale.tare(count);
}

void calibrate(float weight)
{
  // setup scale
  scale.set_scale();
  scale.tare();

  // place weight
  display.clearDisplay();
  display.setCursor(0, 0);
  display.printf("Place %.0fg\nand press\n", weight);
  display.display();
  Serial.printf("Place %.0fg\nand press\n", weight);
  // wait for button press
  while (btn.read() != LOW)
  {
    btn.update();
    delay(10);
  }
  while (btn.read() != HIGH)
  {
    btn.update();
    delay(10);
  }

  // measure and calculate scale
  float value = scale.get_units(10) / weight;
  scale.set_scale(value);

  // write to file
  Serial.printf("Write scale %f to file\n", value);
  config.scale_calib_value = value;
  saveConfig();

  // show ok!
  display.clearDisplay();
  display.printf("Ok!");
  display.display();
  Serial.printf("Calibrated!\n");
  delay(1000);
}

void displayWeight(float weight)
{
  // Serial.printf("Weight: %.2fg\n", weight);
  display.clearDisplay();
  display.setTextSize(4);
  display.setCursor(0, 20);
  display.printf("%.0fg", weight);
  display.display();
}

void saveConfigCallback()
{
  Serial.println("Save config");

  if (strlen(conf_mqtt_server))
  {
    config.mqtt_server = String(conf_mqtt_server);
  }
  if (strlen(conf_mqtt_port))
  {
    config.mqtt_port = String(conf_mqtt_port).toInt();
  }
  if (strlen(conf_mqtt_topic_cat_weight))
  {
    config.mqtt_topic_cat_weight = String(conf_mqtt_topic_cat_weight);
  }
  if (strlen(conf_mqtt_topic_current_weight))
  {
    config.mqtt_topic_current_weight = String(conf_mqtt_topic_current_weight);
  }

  saveConfig();
  setupMQTT();
}

void handleRoot()
{
  // -- Let IotWebConf test and handle captive portal requests.
  if (iotWebConf.handleCaptivePortal())
  {
    // -- Captive portal request were already served.
    return;
  }
  String s = "<!DOCTYPE html><html lang=\"en\"><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1, user-scalable=no\"/>";
  s += "<title>IotWebConf 13 Optional Group</title></head><div>Status page of ";
  s += iotWebConf.getThingName();
  s += ".</div>";

  if (mqttSetupGroup.isActive())
  {
    s += "<div>MQTT</div>";
    s += "<ul>";
    s += "<li>MQTT Server: ";
    s += config.mqtt_server;
    s += "<li>Port: ";
    s += config.mqtt_port;
    s += "<li>Cat weight topic: ";
    s += config.mqtt_topic_cat_weight;
    s += "<li>Current weight topic: ";
    s += config.mqtt_topic_current_weight;
    s += "</ul>";
  }

  s += "Go to <a href='config'>configure page</a> to change values.";
  s += "</body></html>\n";

  server.send(200, "text/html", s);
}

void loadConfig()
{
  File file = LittleFS.open("/config.json", "r");
  
  if(!file) {
    Serial.println("config.json does not exist!");
    return;
  }

  // Allocate a temporary JsonDocument
  // Don't forget to change the capacity to match your requirements.
  // Use https://arduinojson.org/v6/assistant to compute the capacity.
  StaticJsonDocument<512> doc;

  // Deserialize the JSON document
  DeserializationError error = deserializeJson(doc, file);
  if (error)
    Serial.println(F("Failed to read file, using default configuration"));

  // Copy values from the JsonDocument to the Config
  config.mqtt_server = doc["mqtt_server"] | config.mqtt_server;
  config.mqtt_port = doc["mqtt_port"] | config.mqtt_port;
  config.mqtt_topic_current_weight = doc["mqtt_topic_current_weight"] | config.mqtt_topic_current_weight;
  config.mqtt_topic_cat_weight = doc["mqtt_topic_cat_weight"] | config.mqtt_topic_cat_weight;

  config.scale_calib_value = doc["scale_calib_value"] | config.scale_calib_value;
  config.scale_calib_weight = doc["scale_calib_weight"] | config.scale_calib_weight;
  config.scale_weight_min = doc["scale_weight_min"] | config.scale_weight_min;

  // Close the file (Curiously, File's destructor doesn't close the file)
  file.close();
}

void saveConfig()
{
  // Open file for writing
  File file = LittleFS.open("/config.json", "w");
  if (!file)
  {
    Serial.println(F("Failed to create file"));
    return;
  }

  // Allocate a temporary JsonDocument
  // Don't forget to change the capacity to match your requirements.
  // Use https://arduinojson.org/assistant to compute the capacity.
  StaticJsonDocument<512> doc;

  // Set the values in the document
  doc["mqtt_server"] = config.mqtt_server;
  doc["mqtt_port"] = config.mqtt_port;
  doc["mqtt_topic_cat_weight"] = config.mqtt_topic_cat_weight;
  doc["mqtt_topic_current_weight"] = config.mqtt_topic_current_weight;

  doc["scale_calib_value"] = config.scale_calib_value;
  doc["scale_calib_weight"] = config.scale_calib_weight;
  doc["scale_weight_min"] = config.scale_weight_min;

  // Serialize JSON to file
  if (serializeJson(doc, file) == 0)
  {
    Serial.println(F("Failed to write to file"));
  }

  // Close the file
  file.close();
}

void printConfig()
{
  // Open file for reading
  File file = LittleFS.open("/config.json", "r");
  if (!file)
  {
    Serial.println(F("Failed to read file"));
    return;
  }

  // Extract each characters by one by one
  while (file.available())
  {
    Serial.print((char)file.read());
  }
  Serial.println();

  // Close the file
  file.close();
}

void setupMQTT()
{
  mqtt.setServer(config.mqtt_server.c_str(), config.mqtt_port);
}

void sendMQTTCatWeights()
{

  if (measuredCatWeightsCounter)
  {

    for (size_t i = 0; i < measuredCatWeightsCounter; i++)
    {
      // reconnect WiFi
      if (!WiFi.isConnected())
      {
        Serial.println("WiFI currently not connected!");
        return;
      }

      // reconnect to MQTT
      if (!mqtt.connected())
      {
        auto connected = mqtt.connect("cat_scale");
        Serial.printf("Connected to MQTT %d\n", connected);
        if (!connected)
        {
          return;
        }
      }

      // send MQTT
      String msg = "sensors,device=cat_scale,field=cat_weight value=" + String(measuredCatWeightsBuffer[i].weight) + ",sigma=" + String(measuredCatWeightsBuffer[i].sigma) + ",variance=" + String(measuredCatWeightsBuffer[i].variance);
      Serial.printf("Send MQTT message on topic %s: %s\n", config.mqtt_topic_cat_weight.c_str(), msg.c_str());
      mqtt.publish(config.mqtt_topic_cat_weight.c_str(), msg.c_str());
    }

    // disconnect from MQTT
    mqtt.disconnect();

    // reset counter
    measuredCatWeightsCounter = 0;
  }
}