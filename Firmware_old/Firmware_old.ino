#ifndef CONFIG_H
#define CONFIG_H

#define IR_PIN          27
#define CURRENT_PIN     34
#define ONE_WIRE_BUS    4

#define BUZZER_PIN      26
#define LED_NORMAL      18
#define LED_WARNING     19
#define LED_FAULT       33

#define SDA_PIN 21
#define SCL_PIN 22


#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDRESS 0x3C


#define VIBRATION_THRESHOLD   0.35
#define CURRENT_THRESHOLD     0.25
#define TEMPERATURE_THRESHOLD 60.0


#define ACS712_SENSITIVITY 0.1
#define ACS712_OFFSET      2.5


#define LOG_DEBUG   1
#define LOG_INFO    2
#define LOG_DATASET 3


#if __has_include("secrets.h")
#include "secrets.h"
#endif
#ifndef WIFI_SSID
#define WIFI_SSID ""
#endif
#ifndef WIFI_PASS
#define WIFI_PASS ""
#endif
#ifndef BLYNK_TEMPLATE_ID
#define BLYNK_TEMPLATE_ID ""
#endif
#ifndef BLYNK_TEMPLATE_NAME
#define BLYNK_TEMPLATE_NAME "Motor Health Monitoring"
#endif
#ifndef BLYNK_AUTH_TOKEN
#define BLYNK_AUTH_TOKEN ""
#endif


#define RPM_MIN_VALID 100
#define RPM_MAX_VALID 3000

#define HEALTH_WARNING_LEVEL 75
#define HEALTH_CRITICAL_LEVEL 30

#define ABNORMAL_CONFIRM_COUNT 3


#define VIBRATION_SAMPLES 150
#define CURRENT_SAMPLES 500

// Magnitude features retain gravity, matching the recorded training data.
#define VIBRATION_SAMPLE_INTERVAL_MS 10
#define RPM_INTERVAL_MS 1000
#define RPM_PULSES_PER_REVOLUTION 1
#define TEMPERATURE_CONVERSION_MS 750
#define LCD_HEARTBEAT_MS 2000
#define WIFI_RETRY_MS 10000
#define CLOUD_RETRY_MS 10000
#define CLOUD_CONNECT_TIMEOUT_MS 3000

#endif


#ifndef SENSORS_H
#define SENSORS_H

#include <Arduino.h>
#include <math.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL345_U.h>
#include <DallasTemperature.h>
#include <OneWire.h>

extern Adafruit_ADXL345_Unified accel;
extern DallasTemperature tempSensor;
extern volatile unsigned long pulseCount;
extern volatile unsigned long lastPulseTime;
extern float filteredRPM;
extern float offsetVoltage;
extern float tempOffset;
extern unsigned long rpmDebounceUs;
extern bool accelReady;
extern bool currentCalibrationValid;
extern unsigned long temperatureRequestTime;

// Define the ISR once in the sketch: inline IRAM functions can produce invalid
// literal placement when linked by the ESP32 Xtensa toolchain.
void countPulse();

inline float readRPM();
inline float readRPM()
{
    static unsigned long lastTime = 0;
    static bool initialized = false;
    unsigned long now = millis();
    if (!initialized) {
        noInterrupts();
        pulseCount = 0;
        interrupts();
        lastTime = now;
        initialized = true;
        return filteredRPM;
    }
    unsigned long elapsed = now - lastTime;
    if (elapsed < RPM_INTERVAL_MS) return filteredRPM;
    noInterrupts();
    unsigned long count = pulseCount;
    pulseCount = 0;
    interrupts();
    lastTime = now;
    float rawRPM = count * 60000.0f / (elapsed * RPM_PULSES_PER_REVOLUTION);
    if (rawRPM > RPM_MAX_VALID) return NAN;
    filteredRPM = 0.7f * filteredRPM + 0.3f * rawRPM;
    return filteredRPM;
}

// Conversion runs in the background; never wait 750 ms in the sampling loop.
inline float readTemperature();
inline float readTemperature()
{
    static float lastTemperature = NAN;
    if (millis() - temperatureRequestTime >= TEMPERATURE_CONVERSION_MS) {
        float raw = tempSensor.getTempCByIndex(0);
        float corrected = raw + tempOffset;
        lastTemperature = (isfinite(raw) && raw != DEVICE_DISCONNECTED_C &&
                           raw >= -55 && raw <= 125 && isfinite(corrected) &&
                           corrected >= -55 && corrected <= 125) ? corrected : NAN;
        tempSensor.requestTemperatures();
        temperatureRequestTime = millis();
    }
    return lastTemperature;
}

inline bool readVibration(float &mean, float &rms, float &stddev);
inline bool readVibration(float &mean, float &rms, float &stddev)
{
    mean = rms = stddev = NAN;
    // The Adafruit driver returns true from getEvent even on I2C read failure.
    // Check the device ID as well, including after a window acquired mid-removal.
    if (!accelReady || accel.getDeviceID() != 0xE5) return false;
    float runningMean = 0, m2 = 0;
    for (int i = 0; i < VIBRATION_SAMPLES; ++i) {
        sensors_event_t event;
        if (!accel.getEvent(&event)) return false;
        float magnitude = sqrtf(event.acceleration.x * event.acceleration.x +
                                event.acceleration.y * event.acceleration.y +
                                event.acceleration.z * event.acceleration.z);
        if (!isfinite(magnitude) || magnitude < 0.1f) return false;
        float delta = magnitude - runningMean;
        runningMean += delta / (i + 1);
        m2 += delta * (magnitude - runningMean);
        // ADXL345 is configured for 100 Hz. Avoid rereading one sample in a burst.
        delay(VIBRATION_SAMPLE_INTERVAL_MS);
    }
    // Keep all features in the same uncorrected magnitude domain as training.
    if (accel.getDeviceID() != 0xE5) return false;
    mean = runningMean;
    float variance = m2 / VIBRATION_SAMPLES;
    variance = variance > 0 ? variance : 0;
    rms = sqrtf(variance + mean * mean);
    stddev = sqrtf(variance);
    return true;
}

inline bool readCurrent(float &mean, float &rms, float &stddev);
inline bool readCurrent(float &mean, float &rms, float &stddev)
{
    mean = rms = stddev = NAN;
    if (!currentCalibrationValid) return false;
    float runningMean = 0, m2 = 0;
    for (int i = 0; i < CURRENT_SAMPLES; ++i) {
        int adc = analogRead(CURRENT_PIN);
        // Dropping clipped peaks biases RMS low: reject the complete window.
        if (adc < 10 || adc > 4000) return false;
        float current = (adc * 3.3f / 4095.0f - offsetVoltage) / ACS712_SENSITIVITY;
        if (fabsf(current) < 0.05f) current = 0;
        float magnitude = fabsf(current);
        float delta = magnitude - runningMean;
        runningMean += delta / (i + 1);
        m2 += delta * (magnitude - runningMean);
    }
    mean = runningMean;
    float variance = m2 / CURRENT_SAMPLES;
    variance = variance > 0 ? variance : 0;
    rms = sqrtf(variance + mean * mean);
    stddev = sqrtf(variance);
    return true;
}
#endif


#ifndef FEATURES_H
#define FEATURES_H


struct MotorFeatures
{
    float vibRMS;
    float vibMean;
    float vibStdDev;

    float currentRMS;
    float currentMean;
    float currentStdDev;

    float rpm;
    float temperature;
};


inline void buildFeatureArray(MotorFeatures &f, float features[8]);
inline void buildFeatureArray(MotorFeatures &f, float features[8])
{
    features[0] = f.vibRMS;
    features[1] = f.vibMean;
    features[2] = f.vibStdDev;

    features[3] = f.currentRMS;
    features[4] = f.currentMean;
    features[5] = f.currentStdDev;

    features[6] = f.rpm;
    features[7] = f.temperature;
}


inline void normalizeFeatures(float features[8]);
inline void normalizeFeatures(float features[8])
{
}

#endif


#ifndef AI_ENGINE_H
#define AI_ENGINE_H

#include <Arduino.h>
#include <math.h>

#include "motor_model.h"

extern Eloquent::ML::Port::DecisionTree model;
extern int abnormalCount;

inline int runAI(float features[8]);
inline int runAI(float features[8])
{
    for (int i = 0; i < 8; ++i)
        if (!isfinite(features[i])) return -1;
    return model.predict(features);
}

inline String classifyFault(int prediction);
inline String classifyFault(int prediction)
{
    switch (prediction) {
        case 0: return "NORMAL";
        case 1: return "OVERCURRENT";
        case 2: return "OVERHEAT";
        case 3: return "STOPPED";
        case 4: return "UNBALANCE";
        default: return "SENSOR_ERROR";
    }
}

inline String updateFaultState(int prediction, float rpm,
                               float currentRMS = 0, float temperature = 0);
inline String updateFaultState(int prediction, float rpm,
                               float currentRMS, float temperature)
{
    static String candidate = "NORMAL";
    String detected = classifyFault(prediction);
    if (!isfinite(rpm) || !isfinite(currentRMS) || !isfinite(temperature) ||
        rpm < 0 || currentRMS < 0 || temperature < -55 || temperature > 125 ||
        prediction < 0 || prediction > 4) {
        candidate = "NORMAL";
        abnormalCount = 0;
        return "SENSOR_ERROR";
    }
    // Thermal/electrical limits apply even when the model says stopped/normal.
    if (temperature >= TEMPERATURE_THRESHOLD) detected = "OVERHEAT";
    else if (currentRMS >= CURRENT_THRESHOLD) detected = "OVERCURRENT";
    bool abnormal = detected != "NORMAL" && detected != "STOPPED";
    if (abnormal) {
        if (candidate != detected) {
            candidate = detected;
            abnormalCount = 0;
        }
        if (abnormalCount < ABNORMAL_CONFIRM_COUNT) ++abnormalCount;
        if (abnormalCount >= ABNORMAL_CONFIRM_COUNT) return detected;
    } else {
        candidate = "NORMAL";
        abnormalCount = 0;
    }
    return rpm < RPM_MIN_VALID ? "STOPPED" : "NORMAL";
}
#endif


#ifndef HEALTH_MODEL_H
#define HEALTH_MODEL_H


#include <math.h>

extern float previousHealth;


inline float computeMotorHealth(
    float vibStdDev,
    float currentRMS,
    float temperature);
inline float computeMotorHealth(
    float vibStdDev,
    float currentRMS,
    float temperature)
{

    if (!isfinite(vibStdDev) || !isfinite(currentRMS) || !isfinite(temperature) ||
        vibStdDev < 0 || currentRMS < 0 || temperature < -55 || temperature > 125)
        return -1;

    float vibRatio = (VIBRATION_THRESHOLD > 0) ? (vibStdDev / VIBRATION_THRESHOLD) : 0;
    float currentRatio = (CURRENT_THRESHOLD > 0) ? (currentRMS / CURRENT_THRESHOLD) : 0;
    float tempRatio = (TEMPERATURE_THRESHOLD > 0) ? (temperature / TEMPERATURE_THRESHOLD) : 0;

    float vibStress = 0;
    float currentStress = 0;
    float tempStress = 0;

    if(vibRatio > 0.6)
        vibStress = (vibRatio - 0.6) * 50;

    if(currentRatio > 0.6)
        currentStress = (currentRatio - 0.6) * 30;

    if(tempRatio > 0.7)
        tempStress = (tempRatio - 0.7) * 20;

    float rawHealth = 100 - (vibStress + currentStress + tempStress);

    float filteredHealth = 0.3 * rawHealth + 0.7 * previousHealth;

    if(filteredHealth > 100) filteredHealth = 100;
    if(filteredHealth < 0) filteredHealth = 0;
    previousHealth = filteredHealth;

    return filteredHealth;
}


// Rescaled condition indicator; does not predict remaining operating time.
inline int computeMaintenanceScore(float motorHealth);
inline int computeMaintenanceScore(float motorHealth)
{
    if (!isfinite(motorHealth) || motorHealth < 0) return -1;
    int maintenanceScore = 1.25f * (motorHealth - 20);

    if(maintenanceScore > 100)
        maintenanceScore = 100;
    if(maintenanceScore < 0)
        maintenanceScore = 0;

    return maintenanceScore;
}


inline String maintenanceAdvice(int maintenanceScore);
inline String maintenanceAdvice(int maintenanceScore)
{
    if (maintenanceScore < 0)
        return "Sensor check required";
    if (maintenanceScore > 80)
        return "Healthy";

    else if (maintenanceScore > 30)
        return "Service Soon";

    else
        return "Immediate Maintenance Required";
}

#endif


#ifndef SYK_OLED_BITMAP_H
#define SYK_OLED_BITMAP_H

#include <Arduino.h>

const unsigned char syk_logo [] PROGMEM = {
0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xF7, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xC9, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xDD, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
0xFF, 0xFF, 0xFE, 0x00, 0x00, 0x1D, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFC, 0x3F, 0xFF,
0xFF, 0xFF, 0xFC, 0x00, 0x07, 0xC9, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xF9, 0x9F, 0xFF,
0xFF, 0xFF, 0xF9, 0xFF, 0xFF, 0xE3, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFB, 0xDF, 0xFF,
0xFF, 0xFF, 0xF3, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xF9, 0x9F, 0xFF,
0xFF, 0xFF, 0xEF, 0x00, 0x00, 0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xF0, 0x1F, 0xFF,
0xFF, 0xFF, 0x9E, 0x00, 0x00, 0x3E, 0x01, 0xFF, 0xFF, 0x01, 0xC1, 0xFF, 0xF8, 0xE2, 0x7F, 0xFF,
0xFF, 0xFF, 0x3C, 0x00, 0x00, 0x1F, 0x00, 0xFF, 0xFE, 0x03, 0xC1, 0xFF, 0xF1, 0xCF, 0xFF, 0xFF,
0xFF, 0xFE, 0x78, 0x00, 0x00, 0x0F, 0x8C, 0x7F, 0xFC, 0xC7, 0xC1, 0xFF, 0xC7, 0x1F, 0xFF, 0xFF,
0xFF, 0xFC, 0xF0, 0x3F, 0xFF, 0xFF, 0xC6, 0x3F, 0xF9, 0x8F, 0xC1, 0xFF, 0x8E, 0x3F, 0xFF, 0xFF,
0xFF, 0xFC, 0xE0, 0xFF, 0xFF, 0xFF, 0xE3, 0x9F, 0xF3, 0x1F, 0xC1, 0xFE, 0x1C, 0x7F, 0xFF, 0xFF,
0xFF, 0xFC, 0xE1, 0xFF, 0xFF, 0xFF, 0xF9, 0xCF, 0xEE, 0x7F, 0xC1, 0xFC, 0x31, 0xFF, 0xFF, 0xFF,
0xFF, 0xFC, 0xE0, 0xFF, 0xFF, 0xFF, 0xFC, 0xE7, 0xDC, 0xFF, 0xC1, 0xF8, 0xE3, 0xFF, 0xFF, 0xFF,
0xFF, 0xFC, 0xE0, 0x7F, 0xFF, 0xFF, 0xFE, 0x73, 0x39, 0xFF, 0xC1, 0xE1, 0x87, 0xFF, 0xFF, 0xFF,
0xFF, 0xFC, 0xE0, 0x00, 0x00, 0x0F, 0xFF, 0x38, 0x73, 0xFF, 0xC0, 0x03, 0x0F, 0xFF, 0xFF, 0xFF,
0xFF, 0xFC, 0x70, 0x00, 0x00, 0x03, 0xFF, 0x9C, 0xE7, 0xFF, 0xC0, 0x0E, 0x3F, 0xFF, 0xFF, 0xFF,
0xFF, 0xFC, 0x38, 0x00, 0x00, 0x01, 0xFF, 0xCF, 0xCF, 0xFF, 0xC1, 0xFC, 0x7F, 0xFF, 0xFF, 0xFF,
0xFF, 0xFE, 0x1F, 0xFF, 0xFF, 0xC1, 0xFF, 0xE3, 0x9F, 0xFF, 0xC0, 0x07, 0x3F, 0xFF, 0xFF, 0xFF,
0xFF, 0xFF, 0x8F, 0xFF, 0xFF, 0xE1, 0xFF, 0xF3, 0x1F, 0xFF, 0xC0, 0x03, 0x8F, 0xFF, 0xFF, 0xFF,
0xFF, 0xFF, 0xC3, 0xFF, 0xFF, 0xE1, 0xFF, 0xF3, 0x3F, 0xFF, 0xC1, 0xC1, 0xC7, 0xFF, 0xFF, 0xFF,
0xFF, 0xFF, 0x98, 0x00, 0x00, 0x61, 0xFF, 0xF3, 0x3F, 0xFF, 0xC1, 0xF0, 0xE3, 0xFF, 0xFF, 0xFF,
0xFF, 0xFF, 0xD3, 0xFF, 0xFE, 0x61, 0xFF, 0xF3, 0x1F, 0xFF, 0xC1, 0xF8, 0x71, 0xFF, 0xFF, 0xFF,
0xFF, 0xFF, 0xE7, 0xFF, 0xFE, 0x61, 0xFF, 0xF3, 0x1F, 0xF1, 0xC1, 0xFE, 0x18, 0x7F, 0xFF, 0xFF,
0xFF, 0xFF, 0xFF, 0xFF, 0xFE, 0x61, 0xFF, 0xF3, 0x1F, 0xEC, 0xC1, 0xFF, 0x0C, 0x3F, 0xFF, 0xFF,
0xFF, 0xFE, 0x1F, 0xFF, 0xFC, 0xE1, 0xFF, 0xF3, 0x1F, 0xED, 0xC1, 0xFF, 0x82, 0x1F, 0xFF, 0xFF,
0xFF, 0xFC, 0x8F, 0xFF, 0xF9, 0xE1, 0xFF, 0xF3, 0x1F, 0xC1, 0xC1, 0xFF, 0xE0, 0x07, 0xFF, 0xFF,
0xFF, 0xF9, 0xC0, 0x03, 0xE3, 0x83, 0xFF, 0xE0, 0x1F, 0x9F, 0xC1, 0xFF, 0xF0, 0x03, 0xFF, 0xFF,
0xFF, 0xF9, 0xE0, 0x00, 0x07, 0x07, 0xFF, 0xE0, 0x1F, 0x3F, 0x83, 0xFF, 0xF8, 0x01, 0xFF, 0xFF,
0xFF, 0xF8, 0xCF, 0xFF, 0xFE, 0x0F, 0xFF, 0xFF, 0xFE, 0xF8, 0x07, 0xFF, 0xFC, 0x00, 0xFF, 0xFF,
0xFF, 0xFC, 0x0F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFD, 0xF3, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
0xFF, 0xFF, 0xE7, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xF3, 0xE7, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
0xFF, 0xFF, 0xF3, 0x80, 0x00, 0x00, 0x00, 0x00, 0x07, 0x8F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
0xFF, 0xFF, 0xF8, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x1F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
0xFF, 0xFF, 0xFC, 0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE, 0x3F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
0xFF, 0xFF, 0xFE, 0x00, 0x00, 0x00, 0x1F, 0xFF, 0xFC, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE, 0x07, 0xFE, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x80, 0x03, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x83, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

#endif


#ifndef DISPLAY_OLED_H
#define DISPLAY_OLED_H

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>


extern Adafruit_SSD1306 display;

inline void showStartupScreen();
inline void showStartupScreen()
{
    display.clearDisplay();
    display.drawBitmap(0, 0, syk_logo, 128, 64, WHITE);
    display.display();
    delay(3000);

    display.clearDisplay();
    display.setTextColor(WHITE);

    display.setTextSize(1);
    display.setCursor(24,10);
    display.println("Edge AI Based");

    display.setCursor(22,20);
    display.println("Motor Health");

    display.setCursor(28,30);
    display.println("Monitoring");

    display.setCursor(26,40);
    display.println("System - v1.26");

    display.setCursor(32,52);
    display.println("Starting...");

    display.display();

    delay(2500);
}


inline void showSensorErrorScreen();
inline void showSensorErrorScreen()
{
    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setTextSize(1);
    display.setCursor(0, 16);
    display.println("SENSOR ERROR");
    display.setCursor(0, 32);
    display.println("Check sensors/wiring");
    display.display();
}

inline void showStoppedScreen();
inline void showStoppedScreen()
{
    display.clearDisplay();
    display.setTextColor(WHITE);

    display.setTextSize(2);
    display.setCursor(10,20);
    display.println("STOPPED");

    display.display();
}


inline void drawHealthBar(int motorHealth);
inline void drawHealthBar(int motorHealth)
{
    int barWidth = map(motorHealth, 0, 100, 0, 40);
    barWidth = constrain(barWidth, 0, 40);

    display.fillRect(80,20,40,6,BLACK);
    display.drawRect(80,20,40,6,WHITE);
    display.fillRect(80,20,barWidth,6,WHITE);
}


inline void displayMotorData(
        float vib_rms,
        float current_rms,
        float rpm,
        float tempC,
        int motorHealth,
        String fault);
inline void displayMotorData(
        float vib_rms,
        float current_rms,
        float rpm,
        float tempC,
        int motorHealth,
        String fault)
{

    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setTextSize(1);

    display.setCursor(0,0);
    display.println("Motor Data Monitor");

    display.setCursor(0,12);
    display.print("VIB: ");
    display.println(vib_rms);

    display.setCursor(0,22);
    display.print("CUR: ");
    display.println(current_rms);

    display.setCursor(0,32);
    display.print("RPM: ");
    display.println((int)rpm);

    display.setCursor(0,42);
    display.print("TEMP: ");
    display.print(tempC);
    display.println("C");

    display.setCursor(80,12);
    display.print("HLT:");
    display.print(motorHealth);
    display.println("%");

    drawHealthBar(motorHealth);

    display.setCursor(0,54);
    display.print("ST: ");
    display.println(fault);

    display.display();
}

#endif


#ifndef IOT_BLYNK_H
#define IOT_BLYNK_H

#include <Arduino.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <WiFi.h>

#include <BlynkSimpleEsp32.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

// Only the cloud task accesses WiFi/Blynk. The sensor task copies snapshots into
// a one-element queue, so DNS/TCP/cloud outages cannot block local monitoring.
struct CloudTelemetry {
    float temperature, vibration, current;
    int health, rpm, maintenanceScore;
    int normalLED, warningLED, faultLED, buzzer;
    unsigned long alertSequence;
    char fault[20], maintenance[48], alertFault[20];
};

static QueueHandle_t cloudQueue = nullptr;

inline void publishCloudData(const CloudTelemetry &data);
inline void publishCloudData(const CloudTelemetry &data)
{
    if (!Blynk.connected()) return;
    // -1 represents unavailable values on the dashboard; never transmit NaN.
    Blynk.virtualWrite(V0, isfinite(data.temperature) ? data.temperature : -1);
    Blynk.virtualWrite(V1, isfinite(data.vibration) ? data.vibration : -1);
    Blynk.virtualWrite(V2, isfinite(data.current) ? data.current : -1);
    Blynk.virtualWrite(V3, data.health);
    Blynk.virtualWrite(V4, data.fault);
    Blynk.virtualWrite(V5, data.rpm);
    Blynk.virtualWrite(V6, data.normalLED);
    Blynk.virtualWrite(V7, data.warningLED);
    Blynk.virtualWrite(V8, data.faultLED);
    Blynk.virtualWrite(V9, data.buzzer);
    Blynk.virtualWrite(V10, data.maintenanceScore);
    Blynk.virtualWrite(V11, data.maintenance);
}

inline void cloudTask(void *);
inline void cloudTask(void *)
{
    CloudTelemetry latest = {};
    bool haveData = false;
    unsigned long sentAlert = 0;
    unsigned long wifiAttempt = millis();
    unsigned long cloudAttempt = millis() - CLOUD_RETRY_MS;
    unsigned long published = millis();
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    bool useBlynk = BLYNK_AUTH_TOKEN[0] != '\0' && BLYNK_TEMPLATE_ID[0] != '\0';
    if (useBlynk) Blynk.config(BLYNK_AUTH_TOKEN);
    for (;;) {
        if (xQueueReceive(cloudQueue, &latest, 0) == pdTRUE) haveData = true;
        unsigned long now = millis();
        if (WiFi.status() != WL_CONNECTED) {
            if (useBlynk) Blynk.disconnect();
            if (now - wifiAttempt >= WIFI_RETRY_MS) {
                wifiAttempt = now;
                WiFi.disconnect();
                WiFi.begin(WIFI_SSID, WIFI_PASS);
            }
        } else if (useBlynk) {
            if (!Blynk.connected() && now - cloudAttempt >= CLOUD_RETRY_MS) {
                cloudAttempt = now;
                Blynk.connect(CLOUD_CONNECT_TIMEOUT_MS);
            }
            // A failed attempt is retried explicitly, not through run() each loop.
            if (Blynk.connected()) Blynk.run();
            if (Blynk.connected() && haveData) {
                if (latest.alertSequence != 0 && latest.alertSequence != sentAlert) {
                    char message[80];
                    snprintf(message, sizeof(message), "Motor alert: %s. Inspect motor and sensors.", latest.alertFault);
                    Blynk.logEvent("motor_fault", message);
                    // This records a connected send attempt, not server acknowledgment.
                    if (Blynk.connected()) sentAlert = latest.alertSequence;
                }
                if (now - published >= 2000) {
                    published = now;
                    publishCloudData(latest);
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

inline void beginIoT();
inline void beginIoT()
{
    if (WIFI_SSID[0] == '\0') return;
    cloudQueue = xQueueCreate(1, sizeof(CloudTelemetry));
    if (cloudQueue && xTaskCreate(cloudTask, "motor-cloud", 4096, nullptr, 1, nullptr) != pdPASS) {
        vQueueDelete(cloudQueue);
        cloudQueue = nullptr;
        Serial.println("Cloud task unavailable; local monitoring continues");
    }
}

inline void queueTelemetry(CloudTelemetry &data);
inline void queueTelemetry(CloudTelemetry &data)
{
    static unsigned long alertSequence = 0;
    static bool alertActive = false;
    static char alertFault[20] = "";
    bool critical = (data.health >= 0 && data.health < HEALTH_CRITICAL_LEVEL) ||
                    (strcmp(data.fault, "NORMAL") != 0 && strcmp(data.fault, "STOPPED") != 0);
    if (critical && !alertActive) {
        ++alertSequence;
        if (alertSequence == 0) ++alertSequence;
        snprintf(alertFault, sizeof(alertFault), "%s",
                 strcmp(data.fault, "NORMAL") == 0 || strcmp(data.fault, "STOPPED") == 0
                 ? "LOW_HEALTH" : data.fault);
        alertActive = true;
    } else if (!critical && data.health > 40) {
        alertActive = false;
    }
    data.alertSequence = alertSequence;
    snprintf(data.alertFault, sizeof(data.alertFault), "%s", alertFault);
    // Queue overwrite keeps the latest alert pending even across offline recovery.
    if (cloudQueue) xQueueOverwrite(cloudQueue, &data);
}
#endif


#ifndef ALERTS_H
#define ALERTS_H


extern int normalLED;
extern int warningLED;
extern int faultLED;
extern int buzzer;


inline bool checkWarningState(float motorHealth);
inline bool checkWarningState(float motorHealth)
{
    if(motorHealth < HEALTH_WARNING_LEVEL && motorHealth >= 0)
        return true;

    return false;
}


inline void updateAlerts(String fault, bool warningState);
inline void updateAlerts(String fault, bool warningState)
{

    if(fault == "STOPPED")
    {
        digitalWrite(LED_NORMAL, LOW);
        digitalWrite(LED_WARNING, LOW);
        digitalWrite(LED_FAULT, LOW);
        digitalWrite(BUZZER_PIN, LOW);

        normalLED = 0;
        warningLED = 0;
        faultLED = 0;
        buzzer = 0;
    }

    else if(fault != "NORMAL")
    {
        digitalWrite(LED_NORMAL, LOW);
        digitalWrite(LED_WARNING, LOW);
        digitalWrite(LED_FAULT, HIGH);
        digitalWrite(BUZZER_PIN, HIGH);

        normalLED = 0;
        warningLED = 0;
        faultLED = 1;
        buzzer = 1;
    }

    else if(warningState)
    {
        digitalWrite(LED_NORMAL, LOW);
        digitalWrite(LED_WARNING, HIGH);
        digitalWrite(LED_FAULT, LOW);
        digitalWrite(BUZZER_PIN, LOW);

        normalLED = 0;
        warningLED = 1;
        faultLED = 0;
        buzzer = 0;
    }

    else
    {
        digitalWrite(LED_NORMAL, HIGH);
        digitalWrite(LED_WARNING, LOW);
        digitalWrite(LED_FAULT, LOW);
        digitalWrite(BUZZER_PIN, LOW);

        normalLED = 1;
        warningLED = 0;
        faultLED = 0;
        buzzer = 0;
    }
}

#endif


#ifndef LOGGER_H
#define LOGGER_H


extern int logMode;
static String datasetLabel = "UNLABELED";


inline void handleSerialCommands();
inline void handleSerialCommands()
{
    if(Serial.available())
    {
        char cmd = Serial.read();

        if(cmd == 'd')
        {
            logMode = LOG_DEBUG;
            Serial.println("Switched to DEBUG mode");
        }

        else if(cmd == 'i')
        {
            logMode = LOG_INFO;
            Serial.println("Switched to INFO mode");
        }

        else if(cmd == 'c')
        {
            logMode = LOG_DATASET;
            datasetLabel = "UNLABELED";
            Serial.println("time_ms,vib_rms,vib_mean,vib_std,current_rms,current_mean,current_std,rpm,temp_C,status");
        }
        else if (cmd >= '0' && cmd <= '4')
        {
            const char *labels[] = {"NORMAL", "OVERCURRENT", "OVERHEAT", "STOPPED", "UNBALANCE"};
            datasetLabel = labels[cmd - '0'];
        }
        else if (cmd == 'u') datasetLabel = "UNLABELED";
    }
}


inline void logDebug(
    float vibRMS,
    float vibMean,
    float vibStdDev,
    float currentRMS,
    float currentMean,
    float currentStdDev,
    float rpm,
    float temp,
    int aiClass,
    float motorHealth,
    String fault);
inline void logDebug(
    float vibRMS,
    float vibMean,
    float vibStdDev,
    float currentRMS,
    float currentMean,
    float currentStdDev,
    float rpm,
    float temp,
    int aiClass,
    float motorHealth,
    String fault)
{

    Serial.println("\n====== DEBUG ======");

    Serial.print("VibRMS: "); Serial.println(vibRMS);
    Serial.print("VibMean: "); Serial.println(vibMean);
    Serial.print("VibStdDev: "); Serial.println(vibStdDev);

    Serial.print("CurrentRMS: "); Serial.println(currentRMS);
    Serial.print("CurrentMean: "); Serial.println(currentMean);
    Serial.print("CurrentStdDev: "); Serial.println(currentStdDev);

    Serial.print("RPM: "); Serial.println(rpm);
    Serial.print("Temp: "); Serial.println(temp);

    Serial.print("AI Class: "); Serial.println(aiClass);
    Serial.print("Confirmation count: "); Serial.println(abnormalCount);

    Serial.print("Status: "); Serial.println(fault);

    Serial.print("Health: ");
    Serial.print(motorHealth);
    Serial.println("%");

    Serial.println("===================");
}


inline void logInfo(float rpm, float temp, float health, String fault);
inline void logInfo(float rpm, float temp, float health, String fault)
{
    Serial.print("RPM:");
    Serial.print(rpm);

    Serial.print(" | Temp:");
    Serial.print(temp);

    Serial.print(" | Health:");
    Serial.print(health);
    Serial.print("%");

    Serial.print(" | Status:");
    Serial.println(fault);
}


inline void logDataset(
    unsigned long time,
    float features[8]);
inline void logDataset(
    unsigned long time,
    float features[8])
{
    for (int i = 0; i < 8; ++i)
        if (!isfinite(features[i])) return; // Invalid windows are never training rows.

    Serial.print(time); Serial.print(",");

    Serial.print(features[0]); Serial.print(",");
    Serial.print(features[1]); Serial.print(",");
    Serial.print(features[2]); Serial.print(",");
    Serial.print(features[3]); Serial.print(",");
    Serial.print(features[4]); Serial.print(",");
    Serial.print(features[5]); Serial.print(",");
    Serial.print(features[6]); Serial.print(",");
    Serial.print(features[7]); Serial.print(",");

    Serial.println(datasetLabel); // Ground truth selected by operator, not model output.
}


inline void runLogger(
    float vibRMS,
    float vibMean,
    float vibStdDev,
    float currentRMS,
    float currentMean,
    float currentStdDev,
    float rpm,
    float temp,
    float motorHealth,
    int aiPrediction,
    float features[8],
    String fault);
inline void runLogger(
    float vibRMS,
    float vibMean,
    float vibStdDev,
    float currentRMS,
    float currentMean,
    float currentStdDev,
    float rpm,
    float temp,
    float motorHealth,
    int aiPrediction,
    float features[8],
    String fault)
{

    handleSerialCommands();

    if(logMode == LOG_DEBUG)
    {
        logDebug(
            vibRMS,
            vibMean,
            vibStdDev,
            currentRMS,
            currentMean,
            currentStdDev,
            rpm,
            temp,
            aiPrediction,
            motorHealth,
            fault
        );
    }

    else if(logMode == LOG_INFO)
    {
        logInfo(rpm, temp, motorHealth, fault);
    }

    else if(logMode == LOG_DATASET)
    {
        logDataset(millis(), features);
    }
}

#endif


#ifndef CALIBRATION_H
#define CALIBRATION_H


extern float offsetVoltage;
extern float vibBaseline;
extern float tempOffset;
extern unsigned long rpmDebounceUs;
extern bool currentCalibrationValid;


inline void calibrateCurrentSensor();
inline void calibrateCurrentSensor()
{
    Serial.println("Calibrating Current Sensor...");

    float sum = 0;
    currentCalibrationValid = false;
    for (int i = 0; i < 500; i++)
    {
        int adc = analogRead(CURRENT_PIN);
        if (adc < 10 || adc > 4000) {
            Serial.println("Current calibration failed: ADC at rail");
            return;
        }
        float voltage = (adc * 3.3) / 4095.0;
        sum += voltage;
        delay(2);
    }

    offsetVoltage = sum / 500.0;
    currentCalibrationValid = true;
    Serial.print("Current Offset Voltage: ");
    Serial.println(offsetVoltage);
}


inline void calibrateVibrationBaseline();
inline void calibrateVibrationBaseline()
{
    Serial.println("Calibrating Vibration Baseline...");

    float sum = 0;
    for (int i = 0; i < 100; i++)
    {
        sensors_event_t event;
        accel.getEvent(&event);

        float magnitude = sqrt(event.acceleration.x * event.acceleration.x +
                               event.acceleration.y * event.acceleration.y +
                               event.acceleration.z * event.acceleration.z);
        sum += magnitude;
        delay(10);
    }

    vibBaseline = sum / 100.0;
    Serial.print("Vibration Baseline: ");
    Serial.println(vibBaseline);
}


inline void calibrateTemperatureOffset(float offset);
inline void calibrateTemperatureOffset(float offset)
{
    tempOffset = offset;
    Serial.print("Temperature Offset Set To: ");
    Serial.println(tempOffset);
}


inline void calibrateRPMTiming(unsigned long debounceUs);
inline void calibrateRPMTiming(unsigned long debounceUs)
{
    rpmDebounceUs = debounceUs;
    Serial.print("RPM Timing Debounce (us): ");
    Serial.println(rpmDebounceUs);
}

#endif


#ifndef LCD_SERIAL_H
#define LCD_SERIAL_H

extern String lastLCDmsg;

inline void sendLCD(String msg);
inline void sendLCD(String msg)
{
    static unsigned long lastSent = 0;
    if(msg != lastLCDmsg || millis() - lastSent >= LCD_HEARTBEAT_MS)
    {
        Serial2.println(msg);
        lastLCDmsg = msg;
        lastSent = millis();
    }
}

#endif


void setup();
void loop();

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL345_U.h>
#include <DallasTemperature.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>


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
