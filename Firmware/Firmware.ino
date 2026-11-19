#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL345_U.h>
#include <DallasTemperature.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "config.h"
#include "sensors.h"
#include "features.h"
#include "ai_engine.h"
#include "health_model.h"
#include "display_oled.h"
#include "iot_blynk.h"
#include "alerts.h"
#include "logger.h"
#include "calibration.h"
#include "lcd_serial.h"

Adafruit_ADXL345_Unified accel(12345);
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature tempSensor(&oneWire);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
Eloquent::ML::Port::DecisionTree model;

volatile unsigned long pulseCount = 0;
volatile unsigned long lastPulseTime = 0;
float filteredRPM = 0;
float previousHealth = 100;
float offsetVoltage = ACS712_OFFSET;
float vibBaseline = 0; // Calibration diagnostic only; not subtracted from ML features.
float tempOffset = 0;
unsigned long rpmDebounceUs = 10000;
unsigned long temperatureRequestTime = 0;
bool accelReady = false;
bool currentCalibrationValid = false;
bool displayReady = false;
int abnormalCount = 0;
String fault = "STOPPED";
String lastLCDmsg = "";
int normalLED = 0, warningLED = 0, faultLED = 0, buzzer = 0;
int logMode = LOG_DEBUG;
unsigned long lastSampleTime = 0;

void IRAM_ATTR countPulse()
{
    unsigned long now = micros();
    if (now - lastPulseTime > rpmDebounceUs) {
        ++pulseCount;
        lastPulseTime = now;
    }
}

void setup()
{
    Serial.begin(115200);
    Serial2.begin(9600, SERIAL_8N1, -1, 25);
    Wire.begin(SDA_PIN, SCL_PIN);
    Wire.setTimeOut(50);
    pinMode(LED_NORMAL, OUTPUT);
    pinMode(LED_WARNING, OUTPUT);
    pinMode(LED_FAULT, OUTPUT);
    pinMode(BUZZER_PIN, OUTPUT);
    updateAlerts("STOPPED", false);
    pinMode(IR_PIN, INPUT);
    analogReadResolution(12);
    analogSetPinAttenuation(CURRENT_PIN, ADC_11db);

    accelReady = accel.begin();
    if (accelReady) {
        accel.setRange(ADXL345_RANGE_2_G);
        accel.setDataRate(ADXL345_DATARATE_100_HZ);
    } else Serial.println("ADXL345 not detected; sensor alarm will remain active");
    tempSensor.begin();
    tempSensor.setResolution(12);
    tempSensor.setWaitForConversion(false);
    tempSensor.requestTemperatures();
    temperatureRequestTime = millis();

    // Motor must be off during zero-current calibration.
    calibrateCurrentSensor();
    displayReady = display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS);
    if (displayReady) showStartupScreen();
    else Serial.println("OLED initialization failed; monitoring continues");

    readRPM(); // Establish pulse baseline after the startup delays.
    attachInterrupt(digitalPinToInterrupt(IR_PIN), countPulse, RISING);
    lastSampleTime = millis();
    beginIoT();
    sendLCD("STATUS:STARTING");
}

void loop()
{
    handleSerialCommands();
    if (millis() - lastSampleTime < RPM_INTERVAL_MS) {
        delay(1);
        return;
    }
    lastSampleTime = millis();
    MotorFeatures f;
    bool vibrationValid = readVibration(f.vibMean, f.vibRMS, f.vibStdDev);
    bool currentValid = readCurrent(f.currentMean, f.currentRMS, f.currentStdDev);
    f.rpm = readRPM();
    f.temperature = readTemperature();
    float features[8];
    buildFeatureArray(f, features);
    int prediction = runAI(features);
    bool valid = vibrationValid && currentValid && prediction >= 0;
    fault = updateFaultState(valid ? prediction : -1, f.rpm, f.currentRMS, f.temperature);
    float health = valid ? computeMotorHealth(f.vibStdDev, f.currentRMS, f.temperature) : -1;
    if (health >= 0 && health < HEALTH_CRITICAL_LEVEL &&
        (fault == "NORMAL" || fault == "STOPPED")) fault = "LOW_HEALTH";
    int maintenanceScore = computeMaintenanceScore(health);
    String maintenance = maintenanceAdvice(maintenanceScore);
    updateAlerts(fault, checkWarningState(health));

    if (displayReady) {
        if (!valid) showSensorErrorScreen();
        else if (fault == "STOPPED") showStoppedScreen();
        else displayMotorData(f.vibRMS, f.currentRMS, f.rpm, f.temperature, health, fault);
    }

    if (fault == "SENSOR_ERROR") sendLCD("SENSOR ERROR");
    else if (fault == "OVERCURRENT") sendLCD("FLT:OVERCURRENT");
    else if (fault == "OVERHEAT") sendLCD("FAULT:OVERHEAT");
    else if (fault == "UNBALANCE") sendLCD("FAULT:UNBALANCE");
    else if (health >= 0 && health < HEALTH_CRITICAL_LEVEL) sendLCD("MAINTENANCE!");
    else if (fault == "STOPPED") sendLCD("STATUS:STOPPED");
    else if (checkWarningState(health)) sendLCD("STATUS:WARNING");
    else sendLCD("STATUS:NORMAL");

    CloudTelemetry data = {};
    data.temperature = f.temperature;
    data.vibration = f.vibRMS;
    data.current = f.currentRMS;
    data.rpm = isfinite(f.rpm) ? (int)f.rpm : -1;
    data.health = (int)health;
    data.maintenanceScore = maintenanceScore;
    data.normalLED = normalLED;
    data.warningLED = warningLED;
    data.faultLED = faultLED;
    data.buzzer = buzzer;
    snprintf(data.fault, sizeof(data.fault), "%s", fault.c_str());
    snprintf(data.maintenance, sizeof(data.maintenance), "%s", maintenance.c_str());
    queueTelemetry(data);
    runLogger(f.vibRMS, f.vibMean, f.vibStdDev, f.currentRMS, f.currentMean,
              f.currentStdDev, f.rpm, f.temperature, health, prediction, features, fault);
}
