#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include "icon.h"

#define DISPLAY_TEXT_SIZE 4

namespace weightwhiskers
{

    class Display
    {
    public:
        Display(TwoWire *twi);
        bool begin();
        Adafruit_SSD1306& getDisplay();
        
        void drawBootScreen();
        void drawError(String err);
        void drawText(String text);
        void drawWiFi();
        void drawWiFiAPMode();
        void drawTare();
        void drawCalib(int weight);
        void drawWeightScreen(int weight = 0, int lastWeight = 0);
        void drawOTA(float percentage = 0);

    protected:
        TwoWire* twi = nullptr;
        Adafruit_SSD1306 display;
    };

}