#include <Arduino.h>
#include <Wire.h>
#include <HX711.h>
#include <LittleFS.h>
#include <Bounce2.h>
#include <AiEsp32RotaryEncoder.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <ESPAsyncWebServer.h>
#include <ESPAsyncWiFiManager.h>
#include <ESPmDNS.h>
#include <PubSubClient.h>
#include <FastLED.h>
#include <Filters.h>
#include <ArduinoOTA.h>
#include <AsyncJson.h>
#include <ArduinoJson.h>
#include <melody_player.h>
#include <melody_factory.h>
#include "Display.h"

// Button
#define ENCODER_BTN 9
#define ENCODER_A 7
#define ENCODER_B 8
// LED
#define LED_EXTRA 6
#define LED_NUM 1
// HX711 circuit wiring
#define LOADCELL_DOUT_PIN 10
#define LOADCELL_SCK_PIN 11
// Display
#define SDA 42
#define SCL 41
// #define SDA 34
// #define SCL 33
#define SSD1306_NO_SPLASH
// Buzzer
#define BUZZER 45
// constants
#define SCALE_DELAY_MS 100
#define SCALE_WS_DELAY_MS 500
#define BUFSIZE 55
#define JSON_BUFFER 2048

using namespace weightwhiskers;

// display
Display display(&Wire);

// buttons
AiEsp32RotaryEncoder encoder(ENCODER_B, ENCODER_A, ENCODER_BTN, -1, 4, true);

// LED
CRGBArray<LED_NUM> leds;

// buzzer
MelodyPlayer buzzer(BUZZER, 0, LOW);

// scale
HX711 scale;
long scaleLastTimestamp = 0;
long scaleLastWSTimestamp = 0;
long scaleLastTareThreshTimestamp = 0;
FilterOnePole weightLowPass = FilterOnePole(LOWPASS, 0.5, 0);

struct CatMeasurement
{
  time_t time = 0;
  uint16_t weight = 0.;
  float sigma = 0.;
  float duration = 0.;
};
CatMeasurement measuredCatWeightsBuffer[20];
int measuredCatWeightsCounter = 0;

String measurementsFile = "/measurements.csv";
String configFile = "/config.json";

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
  // presence detection
  float presence_time_min = 5.f;
};

Config config;

// WiFi
WiFiClient espClient;
PubSubClient mqtt(espClient);
DNSServer dns;
AsyncWebServer server(80);
AsyncWiFiManager wifiManager(&server, &dns);
AsyncWebSocket ws("/ws");

// declarations
bool loadConfig();
bool saveConfig();
void printConfig();
void applyConfig();
void listDir(fs::FS &fs, const char *dirname, uint8_t levels);
void setupScale();
void tare(int count);
void calibrate(long weight);
void apCallback(AsyncWiFiManager *mgr);
void handleConfig(AsyncWebServerRequest *request);
void handleConfigUpdate(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
void handeMeasurementsUpdate(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
void handleMeasurements(AsyncWebServerRequest *request);
void handleSystem(AsyncWebServerRequest *request);
void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
void setupMQTT();
void sendMQTTCatWeights();
bool writeMeasurement(CatMeasurement &m);
void playToneStart();
void playToneSuccess();

void IRAM_ATTR readEncoderISR()
{
  encoder.readEncoder_ISR();
}

void setup()
{

  // Serial
  Serial.begin(115200);
  Serial.println("Cat scale");

  // setup pins
  pinMode(BUZZER, OUTPUT);
  digitalWrite(BUZZER, LOW);

  // setup encoder
  encoder.begin();
  encoder.setup(readEncoderISR);
  encoder.disableAcceleration();

  // display
  Wire.begin(SDA, SCL);
  display.begin();
  display.drawBootScreen();

  // LED
  FastLED.addLeds<NEOPIXEL, LED_EXTRA>(leds, LED_NUM);
  FastLED.setBrightness(50);
  leds[0] = CRGB::Yellow;
  FastLED.show();

  // start sound
  playToneStart();

  // Filesystem and config
  if (!LittleFS.begin(true))
  {
    Serial.println("An Error has occurred while mounting LittleFS");
    display.drawError("FS Error");
    while (true)
    {
      leds[0] = CRGB::Red;
      FastLED.show();
      delay(500);
      leds[0] = CRGB::Black;
      FastLED.show();
      delay(500);
    }
  }

  // load config or create config file with default values if not existing
  listDir(LittleFS, "/", 4);
  if (!loadConfig())
  {
    saveConfig();
  }
  printConfig();

  // Setup WiFi
  Serial.println("WiFi autoconnect...");
  display.drawWiFi();
  wifiManager.setAPCallback(apCallback);
  if (!wifiManager.autoConnect("weight-whiskers"))
  {
    Serial.println("failed to connect, we should reset as see if it connects");
  }

  // setup mDNS
  MDNS.begin("weight-whiskers");
  Serial.println("Visit the config site at http://weight-whiskers.local");

  // Set up required URL handlers on the web server.
  // static files, cached for 1 month
  server.serveStatic("/", LittleFS, "/www/").setDefaultFile("index.html").setCacheControl("max-age=2678400");
  server.serveStatic("/config", LittleFS, "/www/index.html").setDefaultFile("index.html").setCacheControl("max-age=2678400");
  server.serveStatic("/live", LittleFS, "/www/index.html").setDefaultFile("index.html").setCacheControl("max-age=2678400");
  server.on("/api/config", HTTP_POST, handleConfig, nullptr, handleConfigUpdate);
  server.on("/api/measurements", HTTP_POST, handleMeasurements, nullptr, handeMeasurementsUpdate);
  server.on("/api/raw", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(LittleFS, "/rawvalues.csv", "text/csv"); });
  server.on("/api/system", HTTP_GET, handleSystem);
  // attach AsyncWebSocket
  ws.onEvent(onEvent);
  server.addHandler(&ws);
  server.begin();

  // setup time
  display.drawText("NTP...");
  configTime(0, 0, "pool.ntp.org");

  // setup MQTT
  setupMQTT();
  // setup scale
  setupScale();

  // setup OTA
  ArduinoOTA.setHostname("weight-whiskers");
  ArduinoOTA.setPassword("weight-whiskers");

  ArduinoOTA.onStart([]()
                     {
    Serial.println("Start");
    display.drawOTA(0); });
  ArduinoOTA.onEnd([]()
                   {
    Serial.println("OTA Finished!");
    display.drawOTA(1.); });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
                        {
    float percentage = progress / (total / 100);
    Serial.printf("Update:%u%%\r", percentage);
    display.drawOTA(percentage); });
  ArduinoOTA.onError([](ota_error_t error)
                     {
    Serial.printf("Error[%u]: ", error);
    display.drawOTA(0);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed"); });
  ArduinoOTA.begin();

  // success!
  leds[0] = CRGB::Green;
  FastLED.show();
  Serial.println("Setup finished!");
}

void loop()
{
  // clear ws clients
  ws.cleanupClients();

  // over the air update
  ArduinoOTA.handle();

  // MQTT
  mqtt.loop();

  // button handling
  if (encoder.isEncoderButtonClicked())
  {
    tare(10);
  }
  else
  {
    auto beforeDown = millis();
    while (encoder.isEncoderButtonDown())
    {
      auto now = millis();
      if ((now - beforeDown) > 1000)
      {
        calibrate(config.scale_calib_weight);
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
    if (current - scaleLastWSTimestamp > SCALE_WS_DELAY_MS)
    {
      ws.printfAll("{\"timestamp\": %lu, \"weight\": %f}", current, weightLowPass.output());
      scaleLastWSTimestamp = current;
    }
  }
  else
  {
    Serial.println("HX711 not found.");
    return;
  }

  if (current - scaleLastTimestamp > SCALE_DELAY_MS)
  {
    if (weightLowPass.output() > config.scale_weight_min)
    {
      // debug: write values to file
      File rawValues = LittleFS.open("/rawvalues.csv", FILE_WRITE);
      rawValues.println("time,raw,mean,sigma");
      // cat sits on the throne
      leds[0] = CRGB::Yellow;
      FastLED.show();
      auto occupationStart = current;
      auto statistics = RunningStatistics();
      statistics.setWindowSecs(10);
      statistics.setInitialValue(weightLowPass.output());
      while (weightLowPass.output() > config.scale_weight_min)
      {
        auto value = scale.get_units(2);
        weightLowPass.input(value);
        statistics.input(value);
        rawValues.printf("%lu,%.2f,%.2f,%.2f\n", millis(), value, statistics.mean(), statistics.sigma());
        display.drawWeightScreen(weightLowPass.output());
        delay(10);
      }
      rawValues.close();
      // cat left the throne
      auto duration = (millis() - occupationStart) / 1000.;
      if (duration > config.presence_time_min)
      {
        leds[0] = CRGB::Green;
        FastLED.show();
        CatMeasurement measurement;
        time(&measurement.time); // Get current timestamp
        measurement.weight = statistics.mean();
        measurement.sigma = statistics.sigma();
        measurement.duration = duration;
        measuredCatWeightsBuffer[measuredCatWeightsCounter] = measurement; // save value to send via mqtt
        measuredCatWeightsCounter++;
        writeMeasurement(measurement);
        display.drawWeightScreen(statistics.mean());
        playToneSuccess();
        delay(5000);
      }
      delay(1000);
      tare(10);
    }

    // write weight to display
    display.drawWeightScreen(weightLowPass.output());

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

void setupScale()
{
  display.drawText("Setup scale");
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  scale.set_scale(config.scale_calib_value); // set calibrated scale value from config
  tare(10);
}

void tare(int count = 10)
{
  display.drawTare();
  scale.tare(count);
}

void calibrate(long weight)
{
  Serial.println("Calibrate...");

  // setup scale
  scale.set_scale();
  scale.tare();

  // place weight
  display.drawCalib(weight);
  Serial.printf("Place %ld and press\n", weight);
  // wait for button release
  while (encoder.isEncoderButtonDown())
  {
    delay(10);
  }
  // wait for button press
  encoder.reset();
  long difference = 0;
  while (!encoder.isEncoderButtonDown())
  {
    difference = encoder.readEncoder();
    Serial.printf("Difference %ld %d\n", difference, encoder.encoderChanged());
    display.drawCalib(weight + difference);
    Serial.printf("Place %ld and press\n", weight + difference);
  }
  Serial.println("clicked");
  encoder.reset();
  while (encoder.isEncoderButtonDown())
  {
    delay(10);
  }
  weight += difference;

  // show state
  display.drawText("Calibrating...");

  // measure and calculate scale
  float value = scale.get_units(10) / weight;
  scale.set_scale(value);

  // write to file
  Serial.printf("Write scale %f to file\n", value);
  config.scale_calib_value = value;
  saveConfig();

  // show ok!
  display.drawText("Calibrated!");
  Serial.printf("Calibrated!\n");
  delay(1000);
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
  display.drawWiFiAPMode();
  leds[0] = CRGB::Blue;
  FastLED.show();
}

void handleConfig(AsyncWebServerRequest *request)
{
  printConfig();
  request->send(LittleFS, configFile, "application/json");
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
    File file = LittleFS.open(configFile, FILE_WRITE);

    // Serialize JSON to file
    if (serializeJson(doc, file) == 0)
    {
      Serial.println(F("Failed to write to file"));
      request->send(500, "text/html", "Failed to write to file");
      return;
    }

    // Close the file
    file.close();

    // load config and apply new settings
    loadConfig();
    applyConfig();
  }
}

void handeMeasurementsUpdate(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
{
  Serial.printf("Replace measurements file. CSV size: %d/%d bytes\n", len, total);
  // open new file if index is zero, else append data
  File file = LittleFS.open(measurementsFile, index == 0 ? FILE_WRITE : FILE_APPEND);

  // write data
  file.write(data, len);

  // Close the file
  file.close();
}

void handleMeasurements(AsyncWebServerRequest *request)
{
  request->send(LittleFS, measurementsFile, "text/csv");
}

void handleSystem(AsyncWebServerRequest *request)
{
  StaticJsonDocument<512> doc;
  auto flash = doc.createNestedObject("flash");
  flash["total"] = LittleFS.totalBytes();
  flash["used"] = LittleFS.usedBytes();
  flash["config"] = LittleFS.open(configFile, FILE_READ).size();
  flash["measurements"] = LittleFS.open(measurementsFile, FILE_READ).size();
  auto wifi = doc.createNestedObject("wifi");
  wifi["rssi"] = WiFi.RSSI();
  auto system = doc.createNestedObject("system");
  system["cpu"] = ESP.getChipModel();
  system["freq"] = ESP.getCpuFreqMHz();
  system["heapSize"] = ESP.getHeapSize();
  system["heapFree"] = ESP.getFreeHeap();
  system["heapMin"] = ESP.getMinFreeHeap();
  system["heapMax"] = ESP.getMaxAllocHeap();

  // create repsonse
  AsyncResponseStream *response = request->beginResponseStream("application/json");
  serializeJson(doc, *response);
  request->send(response);
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
  if (type == WS_EVT_CONNECT)
  {
    // client connected
    Serial.printf("ws[%s][%u] connect\n", server->url(), client->id());
    // client->printf("{message: 'Hi!'}", client->id());
    client->ping();
  }
  else if (type == WS_EVT_DISCONNECT)
  {
    // client disconnected
    Serial.printf("ws[%s][%u] disconnect: %u\n", server->url(), client->id());
  }
  else if (type == WS_EVT_ERROR)
  {
    // error was received from the other end
    Serial.printf("ws[%s][%u] error(%u): %s\n", server->url(), client->id(), *((uint16_t *)arg), (char *)data);
  }
  else if (type == WS_EVT_PONG)
  {
    // pong message was received (in response to a ping request maybe)
    Serial.printf("ws[%s][%u] pong[%u]: %s\n", server->url(), client->id(), len, (len) ? (char *)data : "");
  }
  else if (type == WS_EVT_DATA)
  {
    Serial.println("Got WS data!");
  }
}

bool loadConfig()
{
  File file = LittleFS.open(configFile, FILE_READ);

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
  File file = LittleFS.open(configFile, FILE_WRITE);
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
  File file = LittleFS.open(configFile, FILE_READ);
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

void applyConfig()
{
  setupMQTT();
  setupScale();
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
      String msg = "sensors,device=cat_scale,field=cat_weight value=" + String(measuredCatWeightsBuffer[i].weight) + ",sigma=" + String(measuredCatWeightsBuffer[i].sigma) + ",duration=" + String(measuredCatWeightsBuffer[i].duration);
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

  File f = LittleFS.open(measurementsFile, FILE_APPEND, true);

  if (!f)
  {
    Serial.printf("Cannot open measurements file %s\n", measurementsFile);
    return false;
  }

  if (writeHeader)
  {
    f.printf("time,weight,sigma,duration\n");
  }

  f.printf("%ld,%hu,%.2f,%.2f\n", m.time, m.weight, m.sigma, m.duration);
  f.close();

  return true;
}

void playToneStart()
{
  String notes1[] = {"C3", "G3", "C4"};
  Melody melody1 = MelodyFactory.load("Start Melody", 250, notes1, 3);
  buzzer.playAsync(melody1);
}

void playToneSuccess()
{
  String notes1[] = {"C4", "G3", "G3", "A3", "G3", "SILENCE", "B3", "C4"};
  Melody melody1 = MelodyFactory.load("Nice Melody", 250, notes1, 8);
  buzzer.playAsync(melody1);
}