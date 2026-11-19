// ESP32 GPIO25 TX -> pin 2 RX, shared GND.
#include <Wire.h>
#include <hd44780.h>
#include <hd44780ioClass/hd44780_I2Cexp.h>
#include <SoftwareSerial.h>
#include <string.h>

hd44780_I2Cexp lcd;
SoftwareSerial espSerial(2, 3);
char packet[40];
uint8_t packetLength = 0;
bool discarding = false;
char lastDisplayed[17] = "";
unsigned long lastPacketTime = 0;
unsigned long lastByteTime = 0;
bool lcdReady = false;

void displayStatus(const char *message)
{
    char line[17];
    if (strcmp(message, "FAULT:OVERCURRENT") == 0) message = "FLT:OVERCURRENT";
    snprintf(line, sizeof(line), "%-16.16s", message);
    if (strcmp(line, lastDisplayed) != 0) {
        strcpy(lastDisplayed, line);
        if (lcdReady) {
            lcd.setCursor(0, 1);
            lcd.print(line);
        }
    }
}

bool validStatus(const char *message)
{
    const char *known[] = {"STATUS:STARTING", "STATUS:NORMAL", "STATUS:STOPPED",
        "STATUS:WARNING", "FAULT:OVERHEAT", "FAULT:UNBALANCE", "FAULT:OVERCURRENT",
        "FLT:OVERCURRENT", "SENSOR ERROR", "MAINTENANCE!"};
    for (unsigned int i = 0; i < sizeof(known) / sizeof(known[0]); ++i)
        if (strcmp(message, known[i]) == 0) return true;
    return false;
}

void setup()
{
    Serial.begin(9600);
    espSerial.begin(9600);
    lcdReady = lcd.begin(16, 2) == 0;
    if (lcdReady) {
        lcd.backlight();
        lcd.setCursor(0, 0);
        lcd.print("Motor Health v1");
    }
    lastPacketTime = millis();
    displayStatus("WAITING FOR ESP32");
}

void loop()
{
    while (espSerial.available() > 0) {
        char c = espSerial.read();
        lastByteTime = millis();
        if (c == '\n') {
            packet[packetLength] = '\0';
            if (!discarding && packetLength > 0 && validStatus(packet)) {
                lastPacketTime = millis();
                displayStatus(packet);
            }
            packetLength = 0;
            discarding = false;
        } else if (c != '\r' && !discarding) {
            if (packetLength < sizeof(packet) - 1) packet[packetLength++] = c;
            else discarding = true;
        }
    }
    if ((packetLength || discarding) && millis() - lastByteTime > 500) {
        packetLength = 0;
        discarding = false;
    }
    if (millis() - lastPacketTime > 7000) displayStatus("ESP32 LINK LOST");
}
