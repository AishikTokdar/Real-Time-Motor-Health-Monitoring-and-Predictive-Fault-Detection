#ifndef CALIBRATION_H
#define CALIBRATION_H

#include "config.h"

extern float offsetVoltage;

void calibrateCurrentSensor()
{
    Serial.println("Calibrating Current Sensor...");

    float sum = 0;

    for(int i = 0; i < 500; i++)
    {
        float voltage = (analogRead(CURRENT_PIN) * 3.3) / 4095.0;
        sum += voltage;

        delay(2);
    }

    offsetVoltage = sum / 500.0;

    Serial.print("Current Offset Voltage: ");
    Serial.println(offsetVoltage);
}

#endif
