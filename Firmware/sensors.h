#ifndef SENSORS_H
#define SENSORS_H

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

void IRAM_ATTR inline countPulse()
{
    unsigned long now = micros();

    if (now - lastPulseTime > 10000)
    {
        pulseCount++;
        lastPulseTime = now;
    }
}

float inline readRPM()
{
    static unsigned long lastTime = 0;

    unsigned long now = millis();
    unsigned long elapsed = now - lastTime;

    noInterrupts();
    unsigned long count = pulseCount;
    pulseCount = 0;
    interrupts();

    lastTime = now;

    float rawRPM = (count * 60000.0) / elapsed;

    filteredRPM = 0.7 * filteredRPM + 0.3 * rawRPM;

    return filteredRPM;
}

float inline readTemperature()
{
    tempSensor.requestTemperatures();
    return tempSensor.getTempCByIndex(0);
}

void inline readVibration(float &vibMean, float &vibRMS, float &vibStdDev)
{
    float vibSum = 0;
    float vibSumSquares = 0;

    for(int i = 0; i < VIBRATION_SAMPLES; i++)
    {
        sensors_event_t event;
        accel.getEvent(&event);

        float magnitude =
            sqrt(event.acceleration.x * event.acceleration.x +
                 event.acceleration.y * event.acceleration.y +
                 event.acceleration.z * event.acceleration.z);

        vibSum += magnitude;
        vibSumSquares += magnitude * magnitude;
    }

    vibMean = vibSum / VIBRATION_SAMPLES;
    vibRMS = sqrt(vibSumSquares / VIBRATION_SAMPLES);
    vibStdDev = sqrt((vibSumSquares / VIBRATION_SAMPLES) - (vibMean * vibMean));
}

void inline readCurrent(float &currentMean, float &currentRMS, float &currentStdDev)
{
    int validSamples = 0;

    float currentSum = 0;
    float currentSumSquares = 0;

    for(int i = 0; i < CURRENT_SAMPLES; i++)
    {
        int adcValue = analogRead(CURRENT_PIN);

        if(adcValue > 4000 || adcValue < 10)
            continue;

        validSamples++;

        float voltage = (adcValue * 3.3) / 4095.0;
        float current = (voltage - offsetVoltage) / ACS712_SENSITIVITY;

        if(abs(current) < 0.05)
            current = 0;

        currentSum += abs(current);
        currentSumSquares += current * current;
    }

    if(validSamples == 0)
    {
        currentMean = 0;
        currentRMS = 0;
        currentStdDev = 0;
        return;
    }

    currentMean = currentSum / validSamples;
    currentRMS = sqrt(currentSumSquares / validSamples);
    currentStdDev = sqrt((currentSumSquares / validSamples) - (currentMean * currentMean));
}

#endif
