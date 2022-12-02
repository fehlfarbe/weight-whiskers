#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <HX711.h>
#include <LittleFS.h>
#include <Bounce2.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include <FastLED.h>
#include <RunningAverage.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>


// Button
# define BUTTON D7
// LED
#define LED_EXTRA D8
#define LED_NUM 1
// HX711 circuit wiring
#define LOADCELL_DOUT_PIN D2
#define LOADCELL_SCK_PIN D1
// constants
// #define CALIB_WEIGHT 500 // weight in grams
// #define SCALE_MIN_WEIGHT 2000 // weight in grams
#define SCALE_DELAY_MS 100
// #define SCALE_DELTA 10
#define BUFSIZE 20
#define DISPLAY_TEXT_SIZE 4

enum ScaleState {
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
long scale_last = 0;
RunningAverage weightAverage(BUFSIZE);

// WiFi
WiFiManager wifiManager;
WiFiClient espClient;
PubSubClient mqtt(espClient);

// COnfig
struct Config {
  // MQTT
  String mqtt_server;
  int mqtt_port = 1883;
  String mqtt_topic_cat_weight = "home/cat/scale/measured";
  String mqtt_topic_current_weight = "home/cat/scale/current";
  // scale
  float scale_calib_value = 1.f;
  int scale_calib_weight = 500;
  int scale_weight_min = 2000;
};

Config config;
bool shouldSavecConfig = false;
 
// void loadScaleValue();
// void writeScaleValue(float value);
void loadConfig();
void saveConfig();
void printConfig();
void calibrate(float weight);
void displayWeight(float weight);
void saveConfigCallback();


void setup() {
  Serial.begin(115200);
  Serial.println("Cat scale");

  // display
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  Wire.begin(D4, D5);
  display.begin(SSD1306_EXTERNALVCC, 0x3C); // Address 0x3C for 128x32
  // display.begin(SSD1306_EXTERNALVCC, 0x3C); // Address 0x3C for 128x32
  display.display();
  delay(2000);
  display.clearDisplay();
  display.setTextSize(DISPLAY_TEXT_SIZE);
  display.setTextColor(WHITE);

  // LED
  FastLED.addLeds<WS2812B, LED_EXTRA, RGB>(leds, LED_NUM);
  FastLED.setBrightness(50);
  leds[0] = CRGB::Red;
  FastLED.show();

  // setup button button
  btn.attach(BUTTON, INPUT_PULLUP);
  btn.interval(5);


  // Filesystem and config
  if(!LittleFS.begin()){
    Serial.println("An Error has occurred while mounting LittleFS");
  }
  loadConfig();
  printConfig();

  // WiFi
  char mqtt_server[40];
  char mqtt_port[6];
  WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);
  WiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqtt_port, 6);

  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);

  leds[0] = CRGB::Orange;
  FastLED.show();
  display.println("WiFi...");
  display.display();

  if(btn.read() == LOW) {
    Serial.println("Start config portal...");
    wifiManager.startConfigPortal();
  }

  if(!wifiManager.autoConnect("CatScale")){
    display.clearDisplay();
    display.printf("AP: catscale");
    display.display();
  }

  // check if config has changed
  if(shouldSavecConfig){
    saveConfig();
    printConfig();
  }

  // copy config from portal
  if(custom_mqtt_server.getValueLength()) {
    strcpy(mqtt_server, custom_mqtt_server.getValue());
    config.mqtt_server = String(mqtt_server);
  }
  if(custom_mqtt_port.getValueLength()){
    strcpy(mqtt_port, custom_mqtt_port.getValue());
    config.mqtt_port = String(mqtt_port).toInt();
  }

  // set MQTT
  mqtt.setServer(config.mqtt_server.c_str(), config.mqtt_port);
  display.print("Connected!");
  display.display();

  // setup OTA
  ArduinoOTA.setHostname("catscale");
  // ArduinoOTA.setPassword("catscale");

  ArduinoOTA.onStart([]() {
    Serial.println("Start");
    display.clearDisplay();
    display.setCursor(0, 20);
    display.print("OTA Update...");
    display.display();
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("OTA Finished!");
    display.clearDisplay();
    display.setCursor(0, 20);
    display.print("OTA Finished!");
    display.display();
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Update:%u%%\r", (progress / (total / 100)));
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(0, 0);
    display.printf("Update: %u%%", (progress / (total / 100)));
    display.writeFillRect(0, display.height()-10, (display.width()/(float)total)*progress, display.height(), WHITE);
    display.display();
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    display.clearDisplay();
    display.setCursor(0, 20);
    display.print("OTA Finished!");
    display.display();
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();


  // setup scale
  display.print("Setup scale");
  display.display();
  leds[0] = CRGB::Yellow;
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  scale.set_scale(config.scale_calib_value); // set calibrated value
  scale.tare(10); // tare scale
  leds[0] = CRGB::Green;
  FastLED.show();

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
}

void loop() {
  ArduinoOTA.handle();

  if(WiFi.isConnected() && !mqtt.connected()){
    mqtt.connect("cat_scale");
    Serial.printf("Connected to MQTT %d\n", mqtt.connected());
  }

// button handling
  btn.update();
  if (btn.changed() ) {
    int deboucedInput = btn.read();
    if ( deboucedInput == HIGH ) {
      if ( btn.previousDuration() > 1000 ) {
        Serial.println("Calibrate...");
        calibrate(500);
      } else {
        Serial.println("Tare...");
        display.print("WiFi...");
        display.display();
        scale.tare(10);
      }
    } 
  } 

  // measure weight
  if(millis()-scale_last > SCALE_DELAY_MS){
    scale_last = millis();
    float weight = 0.f;

    if (scale.wait_ready_timeout(1000)) {
      weight = scale.get_units(2);
      // Serial.print("HX711 reading: ");
      // Serial.println(weight);
    } else {
      Serial.println("HX711 not found.");
      return;
    }

    if(weight > config.scale_weight_min && state == EMPTY){
      // cat sits on the throne
      state = OCCUPIED;
      leds[0] = CRGB::Yellow;
      FastLED.show();
      weightAverage.clear();
      while(weight > config.scale_weight_min)
      {
        weight = scale.get_units(2);
        weightAverage.addValue(weight);
        displayWeight(weight);
        delay(100);
      }

      // cat left the throne
      leds[0] = CRGB::Green;
      FastLED.show();
      weight = weightAverage.getAverage();
      displayWeight(weight);
      delay(5000);

      // send MQTT
      String msg = "sensors,device=cat_scale,field=cat_weight value=" + String(weight);
      Serial.println(msg);
      mqtt.publish(config.mqtt_topic_cat_weight.c_str(), msg.c_str());

      // reset state
      state = EMPTY;
      weight = scale.get_units(2);
      displayWeight(weight);
      delay(1000);
      scale.tare(10);
    }

    // write weight to display
    displayWeight(weight);

    // publish to MQTT
    // if(!mqtt.publish(MQTT_TOPIC_CURRENT, String(weight).c_str())){
    //   Serial.println("Cannot publish to MQTT");
    // }

    // if(abs(weight) > SCALE_DELTA){
    //   scale.tare(10);
    // }
  }

  // reset LED
  leds[0] = CRGB::Black;
  FastLED.show();
}


// void loadScaleValue(){
//   if(LittleFS.exists("/scale.txt")){
//     File file = LittleFS.open("/scale.txt", "r");
//     float value = file.readString().toFloat();
//     file.close();
//     Serial.printf("Reading scale value of %f\n", value);
//     scale.set_scale(value);
//   }
// }

// void writeScaleValue(float value){
//   File file = LittleFS.open("/scale.txt", "w");
//   file.printf("%f", value);
//   file.close();
// }

void clickTare(){
  scale.tare(10);
}

void clickCalibrate(){
  calibrate(config.scale_calib_weight);
}

void calibrate(float weight){
  // setup scale
  scale.set_scale();
  scale.tare();

  // place weight
  display.clearDisplay();
  display.setCursor(0,0);
  display.printf("Place %.0fg\nand press", weight);
  display.display();
  Serial.printf("Place %.0fg\nand press", weight);
  // wait for button press
  while (btn.read() != LOW)
  {
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
}

void displayWeight(float weight){
  Serial.printf("Weight: %fg\n", weight);
  display.clearDisplay();
  display.setCursor(0, 20);
  display.printf("%.0fg", weight);
  display.display();
}

void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSavecConfig = true;
}

void loadConfig(){
  File file = LittleFS.open("config.json", "r");

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

void saveConfig(){
// Open file for writing
  File file = LittleFS.open("config.json", "w");
  if (!file) {
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
  if (serializeJson(doc, file) == 0) {
    Serial.println(F("Failed to write to file"));
  }

  // Close the file
  file.close();

  shouldSavecConfig = false;
}

void printConfig(){
  // Open file for reading
  File file = LittleFS.open("config.json", "r");
  if (!file) {
    Serial.println(F("Failed to read file"));
    return;
  }

  // Extract each characters by one by one
  while (file.available()) {
    Serial.print((char)file.read());
  }
  Serial.println();

  // Close the file
  file.close();
}