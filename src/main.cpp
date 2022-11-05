#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <HX711.h>
#include <OneButton.h>
#include <Smoothed.h>
#include <LittleFS.h>

// Button
# define BUTTON D6
// HX711 circuit wiring
#define LOADCELL_DOUT_PIN D2
#define LOADCELL_SCK_PIN D3
// constants
#define CALIB_WEIGHT 500 // weight in grams
#define SCALE_DELTA 50 // weight in grams
#define BUFSIZE 100


enum ScaleState {
  EMPTY,
  OCCUPIED
};

ScaleState state = EMPTY;

// display
Adafruit_SSD1306 display = Adafruit_SSD1306(128, 32, &Wire);

// buttons
OneButton btn(BUTTON, true, true);

// scale
HX711 scale;

// declarations
void loadScaleValue();
void writeScaleValue(float value);
void clickCalibrate();
void clickTare();
void calibrate(int weight);
void displayWeight(long weight);


void setup() {
  Serial.begin(115200);
  Serial.println("Cat scale");

  // display
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C); // Address 0x3C for 128x32
  display.display();

  // setup button button
  btn.attachClick(clickTare);
  // btn.attachDoubleClick(doubleclick1);
  btn.attachLongPressStop(clickCalibrate);

  //
  if(!LittleFS.begin()){
    Serial.println("An Error has occurred while mounting LittleFS");
  }

  // setup scale
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  loadScaleValue();
  scale.tare(10);
}

void loop() {
  btn.tick();

  long weight = 0;

  if (scale.wait_ready_timeout(1000)) {
    weight = scale.read();
    Serial.print("HX711 reading: ");
    Serial.println(weight);
  } else {
    Serial.println("HX711 not found.");
    return;
  }

  if(weight > SCALE_DELTA && state == EMPTY){
    // cat sits on the throne
    state = OCCUPIED;
    Smoothed <long> values;
    values.begin(SMOOTHED_AVERAGE, BUFSIZE);	
    while(weight > SCALE_DELTA)
    {
      weight = scale.read();
      values.add(weight);
      displayWeight(weight);
      delay(100);
    }

    // cat left the throne
    displayWeight(values.get());
    delay(1000);

    // @todo: send MQTT
  }

  // write weight to display
  displayWeight(weight);

  delay(100);
}


void loadScaleValue(){
  if(LittleFS.exists("/scale.txt")){
    File file = LittleFS.open("/scale.txt", "r");
    float value = file.readString().toFloat();
    file.close();
    Serial.printf("Reading scale value of %f\n", value);
    scale.set_scale(value);
  }
}

void writeScaleValue(float value){
  File file = LittleFS.open("/scale.txt", "w");
  file.printf("%f", value);
  file.close();
}

void clickTare(){
  scale.tare(10);
}

void clickCalibrate(){
  calibrate(CALIB_WEIGHT);
}

void calibrate(int weight){
  // setup scale
  scale.set_scale();
  scale.tare();

  // place weight
  display.clearDisplay();
  display.setCursor(0,0);
  display.printf("Place %dg\nand press", weight);
  display.display();
  // wait for button press
  while (digitalRead(BUTTON) != LOW)
  {
    delay(10);
  }
  
  // measure and calculate scale
  float value = scale.get_units(10) / weight;
  scale.set_scale(value);

  // write to file
  writeScaleValue(value);

  // show ok!
  display.clearDisplay();
  display.printf("Ok!", weight);
  display.display();
}

void displayWeight(long weight){
  Serial.printf("Weight: %dg\n", weight);
  display.clearDisplay();
  display.setCursor(0,0);
  display.printf("%dg", weight);
  display.display();
}