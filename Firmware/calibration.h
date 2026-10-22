#ifndef CALIBRATION_H
#define CALIBRATION_H

#include "config.h"

extern float offsetVoltage;
extern float vibBaseline;
extern float tempOffset;

inline void calibrateCurrentSensor()
{
    Serial.println("Calibrating Current Sensor...");

    float sum = 0;
    for (int i = 0; i < 500; i++)
    {
        float voltage = (analogRead(CURRENT_PIN) * 3.3) / 4095.0;
        sum += voltage;
        delay(2);
    }

    offsetVoltage = sum / 500.0;
    Serial.print("Current Offset Voltage: ");
    Serial.println(offsetVoltage);
}

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

inline void calibrateTemperatureOffset(float offset)
{
    tempOffset = offset;
    Serial.print("Temperature Offset Set To: ");
    Serial.println(tempOffset);
}

#endif
