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
#include <ESPAsyncWebServer.h>
#include <ESPAsyncWiFiManager.h>
#include <ESPmDNS.h>
#include <PubSubClient.h>
#include <FastLED.h>
#include <RunningAverage.h>
#include <RunningMedian.h>
#include <Filters.h>
#include <ArduinoOTA.h>
#include <AsyncJson.h>
#include <ArduinoJson.h>

// Button
#define BUTTON 9
// LED
#define LED_EXTRA 6
#define LED_NUM 1
// HX711 circuit wiring
#define LOADCELL_DOUT_PIN 10
#define LOADCELL_SCK_PIN 11
// Display
#define SDA 42
#define SCL 41
// constants
#define SCALE_DELAY_MS 100
#define BUFSIZE 55
#define DISPLAY_TEXT_SIZE 4
#define JSON_BUFFER 2048

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
  time_t time = 0;
  float weight = 0.;
  float sigma = 0.;
  float variance = 0.;
};
CatMeasurement measuredCatWeightsBuffer[20];
int measuredCatWeightsCounter = 0;
String measurementsFile = "/measurements.csv";

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
DNSServer dns;
AsyncWebServer server(80);
AsyncWiFiManager wifiManager(&server, &dns);

// declarations
bool loadConfig();
bool saveConfig();
void printConfig();
void listDir(fs::FS &fs, const char *dirname, uint8_t levels);
void tare(int count);
void calibrate(float weight);
void displayWeight(float weight);
void apCallback(AsyncWiFiManager *mgr);
void handleConfig(AsyncWebServerRequest *request);
void handleConfigUpdate(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
void handleMeasurements(AsyncWebServerRequest *request);
void setupMQTT();
void sendMQTTCatWeights();
bool writeMeasurement(CatMeasurement &m);

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
  FastLED.addLeds<WS2812B, LED_EXTRA, RGB>(leds, LED_NUM);
  FastLED.setBrightness(50);
  leds[0] = CRGB::Aqua;
  FastLED.show();

  // setup button button
  btn.attach(BUTTON, INPUT_PULLUP);
  btn.interval(5);

  // Filesystem and config
  if (!LittleFS.begin(true))
  {
    Serial.println("An Error has occurred while mounting LittleFS");
    while (true)
    {
      digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
      delay(500);
    }
  }
  delay(5000);

  // load config or create config file with default values if not existing
  listDir(LittleFS, "/", 4);
  if (!loadConfig())
  {
    saveConfig();
  }
  printConfig();

  // Setup WiFi
  Serial.println("WiFi autoconnect...");
  display.println("WiFi...");
  display.display();
  wifiManager.setAPCallback(apCallback);
  if (!wifiManager.autoConnect("weight-whiskers"))
  {
    Serial.println("failed to connect, we should reset as see if it connects");
  }

  // setup mDNS
  MDNS.begin("weight-whiskers");
  Serial.println("Visit the config site at http://weight-whiskers.local");

  // Set up required URL handlers on the web server.
  // server.on("/", HTTP_GET, handleRoot);
  server.serveStatic("/", LittleFS, "/www/").setDefaultFile("index.html");
  // server.on("/api/config", HTTP_GET, handleConfig);
  server.on("/api/config", HTTP_POST, handleConfig, nullptr, handleConfigUpdate);
  server.on("/api/measurements", HTTP_GET, handleMeasurements);
  server.begin();

  // setup time
  display.println("NTP...");
  display.display();
  configTime(0, 0, "pool.ntp.org");

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
  // over the air update
  // ArduinoOTA.handle();

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
    auto weight = scale.get_units(2);
    auto raw = scale.get_value();
    Serial.printf("Current measurement: %fg raw: %f\n", weight, raw);
    weightLowPass.input(weight);
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
      time(&measurement.time); // Get current timestamp
      measurement.weight = statistics.mean();
      measurement.sigma = statistics.sigma();
      measurement.variance = statistics.variance();
      measuredCatWeightsBuffer[measuredCatWeightsCounter] = measurement; // save value to send via mqtt
      measuredCatWeightsCounter++;
      writeMeasurement(measurement);
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

void listDir(fs::FS &fs, const char *dirname, uint8_t levels)
{
  Serial.printf("Listing directory: %s\n", dirname);

  File root = fs.open(dirname, FILE_READ, false);
  if (!root)
  {
    Serial.println("Failed to open directory");
    return;
  }
  if (!root.isDirectory())
  {
    Serial.println("Not a directory");
    return;
  }

  File file = root.openNextFile();
  while (file)
  {
    if (file.isDirectory())
    {
      Serial.print("  DIR : ");
      Serial.println(file.name());
      if (levels)
      {
        listDir(fs, (String(dirname) + String("/") + String(file.name())).c_str(), levels - 1);
      }
    }
    else
    {
      Serial.print("  FILE: ");
      Serial.print(file.name());
      Serial.print("  SIZE: ");
      Serial.println(file.size());
    }
    file = root.openNextFile();
  }
}

void apCallback(AsyncWiFiManager *mgr)
{
  Serial.println("Started AP");
  display.println("AP Mode");
  display.display();
}

void handleConfig(AsyncWebServerRequest *request)
{
  printConfig();
  request->send(LittleFS, "/config.json", "application/json");
}

void handleConfigUpdate(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
{
  Serial.printf("Update config. Config size: %d/%d bytes\n", len, total);
  if (len == total)
  {
    DynamicJsonDocument doc(JSON_BUFFER);
    DeserializationError error = deserializeJson(doc, data);

    // Test if parsing succeeds.
    if (error)
    {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.c_str());
      request->send(500, "text/html", error.c_str());
      return;
    }

    // open file
    File file = LittleFS.open("/config.json", FILE_WRITE);

    // Serialize JSON to file
    if (serializeJson(doc, file) == 0)
    {
      Serial.println(F("Failed to write to file"));
      request->send(500, "text/html", "Failed to write to file");
      return;
    }

    // Close the file
    file.close();
  }
}

void handleMeasurements(AsyncWebServerRequest *request)
{
  request->send(LittleFS, measurementsFile, "text/csv");
}

bool loadConfig()
{
  File file = LittleFS.open("/config.json", "r");

  if (!file)
  {
    Serial.println("config.json does not exist!");
    return false;
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
  config.mqtt_server = doc["mqttServer"] | config.mqtt_server;
  config.mqtt_port = doc["mqttPort"] | config.mqtt_port;
  config.mqtt_topic_current_weight = doc["mqttTopicCurrentWeight"] | config.mqtt_topic_current_weight;
  config.mqtt_topic_cat_weight = doc["mqttTopicCatWeight"] | config.mqtt_topic_cat_weight;

  config.scale_calib_value = doc["scaleCalibValue"] | config.scale_calib_value;
  config.scale_calib_weight = doc["scaleCalibWeight"] | config.scale_calib_weight;
  config.scale_weight_min = doc["scaleWeightMin"] | config.scale_weight_min;

  config.scale_tare_time = doc["scaleTareTime"] | config.scale_tare_time;
  config.scale_tare_thresh = doc["scaleTareThresh"] | config.scale_tare_thresh;

  // Close the file (Curiously, File's destructor doesn't close the file)
  file.close();

  return true;
}

bool saveConfig()
{
  // Open file for writing
  File file = LittleFS.open("/config.json", "w");
  if (!file)
  {
    Serial.println(F("Failed to create file"));
    return false;
  }

  // Allocate a temporary JsonDocument
  // Don't forget to change the capacity to match your requirements.
  // Use https://arduinojson.org/assistant to compute the capacity.
  StaticJsonDocument<512> doc;

  // Set the values in the document
  doc["mqttServer"] = config.mqtt_server;
  doc["mqttPort"] = config.mqtt_port;
  doc["mqttTopicCurrentWeight"] = config.mqtt_topic_cat_weight;
  doc["mqttTopicCatWeight"] = config.mqtt_topic_current_weight;

  doc["scaleCalibValue"] = config.scale_calib_value;
  doc["scaleCalibWeight"] = config.scale_calib_weight;
  doc["scaleWeightMin"] = config.scale_weight_min;

  doc["scaleTareTime"] = config.scale_tare_time;
  doc["scaleTareThresh"] = config.scale_tare_thresh;

  // Serialize JSON to file
  if (serializeJson(doc, file) == 0)
  {
    Serial.println(F("Failed to write to file"));
  }

  // Close the file
  file.close();

  return true;
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

bool writeMeasurement(CatMeasurement &m)
{

  bool writeHeader = false;
  if (!LittleFS.exists(measurementsFile))
  {
    writeHeader = true;
  }

  File f = LittleFS.open(measurementsFile, "a", true);

  if (!f)
  {
    Serial.printf("Cannot open measurements file %s\n", measurementsFile);
    return false;
  }

  if (writeHeader)
  {
    f.printf("time,weight,sigma,variance\n");
  }

  f.printf("%g,%d,%.2f,%.2f\n", m.time, m.weight, m.sigma, m.variance);
  f.close();

  return true;
}