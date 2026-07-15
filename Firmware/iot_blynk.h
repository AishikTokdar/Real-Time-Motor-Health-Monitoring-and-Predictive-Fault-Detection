#ifndef IOT_BLYNK_H
#define IOT_BLYNK_H

#include <WiFi.h>
#include "config.h"
#include <BlynkSimpleEsp32.h>
#include "lcd_serial.h"

extern BlynkTimer timer;

extern float temperature;
extern float vibration;
extern float current;
extern int motorhealth;
extern int Rpm;
extern int RUL;

extern int normalLED;
extern int warningLED;
extern int faultLED;
extern int buzzer;

extern String fault;
extern String maintenance;

extern bool alertSent;

unsigned long lastWifiCheck = 0;
const unsigned long wifiRetryInterval = 10000;
bool wifiConnected = false;

void sendDataToBlynk()
{
    Blynk.virtualWrite(V0, temperature);
    Blynk.virtualWrite(V1, vibration);
    Blynk.virtualWrite(V2, current);
    Blynk.virtualWrite(V3, motorhealth);
    Blynk.virtualWrite(V4, fault);
    Blynk.virtualWrite(V5, Rpm);

    Blynk.virtualWrite(V6, normalLED);
    Blynk.virtualWrite(V7, warningLED);
    Blynk.virtualWrite(V8, faultLED);
    Blynk.virtualWrite(V9, buzzer);

    Blynk.virtualWrite(V10, RUL);
    Blynk.virtualWrite(V11, maintenance);
}

void checkMotorAlert(float motorHealth)
{
    if (motorHealth < HEALTH_CRITICAL_LEVEL && !alertSent)
    {
        Blynk.logEvent("motor_fault", "Motor health critical! Maintenance required.");
        alertSent = true;
    }

    if (motorHealth > 40)
    {
        alertSent = false;
    }
}

void manageWiFi()
{
    if (WiFi.status() == WL_CONNECTED)
    {
        if (!wifiConnected)
        {
            wifiConnected = true;

            sendLCD("WIFI:CONNECTED");

            Blynk.config(BLYNK_AUTH_TOKEN);
            if(!Blynk.connected())
            {
               Blynk.connect();
            }

            sendLCD("BLYNK:ONLINE");
        }
    }
    else
    {
        wifiConnected = false;

        sendLCD("BLYNK:OFFLINE");

        if (millis() - lastWifiCheck > wifiRetryInterval)
        {
            lastWifiCheck = millis();

            sendLCD("WIFI:RETRY");

            WiFi.disconnect();
            WiFi.begin(WIFI_SSID, WIFI_PASS);
        }
    }
}

#endif
