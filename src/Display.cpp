#include "Display.h"

namespace weightwhiskers
{

    Display::Display(TwoWire *twi)
    {
        this->twi = twi;
    }

    bool Display::begin()
    {
        display = Adafruit_SSD1306(128, 64, twi);
        display.begin(SSD1306_EXTERNALVCC, 0x3C); // Address 0x3C for 128x32
        display.clearDisplay();
        display.display();

        return true;
    }

    Adafruit_SSD1306 &Display::getDisplay()
    {
        return display;
    }

    void Display::drawBootScreen()
    {
        display.clearDisplay();
        display.setTextSize(DISPLAY_TEXT_SIZE);
        display.setTextColor(WHITE);
        display.drawBitmap(0, 0, icon, 128, 64, WHITE);
        display.display();
    }

    void Display::drawError(String err)
    {
        display.clearDisplay();
        display.setTextSize(DISPLAY_TEXT_SIZE);
        display.setTextColor(WHITE);
        display.setCursor(0, 0);
        display.println(err);
        display.display();
    }

    void Display::drawText(String text)
    {
        display.clearDisplay();
        display.setTextSize(DISPLAY_TEXT_SIZE);
        display.setTextColor(WHITE);
        display.setCursor(0, 0);
        display.println(text);
        display.display();
    }

    void Display::drawWiFi()
    {
        display.clearDisplay();
        display.setTextSize(DISPLAY_TEXT_SIZE);
        display.setTextColor(WHITE);
        display.setCursor(0, 0);
        display.println("WiFi...");
        display.display();
    }

    void Display::drawWiFiAPMode()
    {
        display.clearDisplay();
        display.setTextSize(DISPLAY_TEXT_SIZE);
        display.setTextColor(WHITE);
        display.setCursor(0, 0);
        display.println("AP Mode");
        display.display();
    }

    void Display::drawTare()
    {
        drawText("Tare...");
    }

    void Display::drawCalib(int weight)
    {
        display.clearDisplay();
        display.setTextSize(2);
        display.setTextColor(WHITE);
        display.setCursor(0, 0);
        display.printf("Place %dg\nand press button\n", weight);
        display.display();
    }

    void Display::drawWeightScreen(float weight, float lastWeight)
    {
        display.clearDisplay();
        display.setTextSize(DISPLAY_TEXT_SIZE);
        display.setTextColor(WHITE);
        // draw weight
        display.setCursor(0, 10);
        display.printf("%.0fg", weight);

        // draw last measured weight
        if (lastWeight)
        {
            display.setTextSize(1);
            display.setTextColor(WHITE);
            display.setCursor(0, display.height() - 10);
            display.printf("Last: %.0fg", lastWeight);
        }
        // draw WiFi signal strength bars
        auto rssi = WiFi.RSSI();
        // display.setTextSize(1);
        // display.setCursor(0, display.height() - 10);
        // display.printf("%d", rssi);
        display.setCursor(0, 0);
        size_t bars = 4;
        if (rssi >= -55)
            bars = 4;
        else if (rssi >= -66)
            bars = 3;
        else if (rssi >= -77)
            bars = 2;
        else if (rssi >= -88)
            bars = 1;
        uint16_t barWidth = 2;
        uint16_t barMinHeight = 4;
        uint16_t barHeightStep = 2;
        uint16_t currentHeight = barMinHeight;
        uint16_t startX = display.width() - 4 * barWidth - 4;
        uint16_t startY = display.height();
        for (size_t i = 0; i < bars; i++)
        {
            uint16_t x = startX + i * barWidth + i;
            uint16_t y = startY - currentHeight;
            uint16_t w = barWidth;
            uint16_t h = currentHeight;
            // Serial.printf("Bar start x=%d, y=%d w=%d h=%d (RSSI: %d)\n", x, y, w, h, rssi);
            display.fillRect(x, y, w, h, WHITE);
            currentHeight += barHeightStep;
        }
        // draw WiFi icon
        uint16_t iconWidth = 9;
        uint16_t middleX = startX - iconWidth;
        uint16_t topY = display.height() - iconWidth;
        display.drawLine(middleX, topY, middleX, display.height(), WHITE);
        display.drawLine(middleX - iconWidth / 2, topY, middleX, display.height() - iconWidth / 2, WHITE);
        display.drawLine(middleX + iconWidth / 2, topY, middleX, display.height() - iconWidth / 2, WHITE);

        display.display();
    }

    void Display::drawOTA(float percentage)
    {
        display.clearDisplay();
        display.setTextSize(2);
        display.setTextColor(WHITE);
        display.setCursor(0, 0);
        display.printf("Update:%u%%", (uint8_t)percentage * 100);
        display.writeFillRect(0, display.height() - 10, display.width() * percentage, display.height(), WHITE);
        display.display();
    }
}