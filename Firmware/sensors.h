#ifndef SENSORS_H
#define SENSORS_H

#include <Arduino.h>
#include <math.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL345_U.h>
#include <DallasTemperature.h>
#include <OneWire.h>
#include "config.h"

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
