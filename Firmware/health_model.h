#ifndef HEALTH_MODEL_H
#define HEALTH_MODEL_H

#include "config.h"

extern float previousHealth;

inline float computeMotorHealth(
    float vibStdDev,
    float currentRMS,
    float temperature)
{

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

    previousHealth = filteredHealth;

    if(filteredHealth > 100) filteredHealth = 100;
    if(filteredHealth < 0) filteredHealth = 0;

    return filteredHealth;
}

inline int estimateRUL(float motorHealth)
{
    int RUL = motorHealth * 1.2;

    if(RUL > 100)
        RUL = 100;
    if(RUL < 0)
        RUL = 0;

    return RUL;
}

inline String maintenanceAdvice(int RUL)
{
    if (RUL > 80)
        return "Healthy";

    else if (RUL > 50)
        return "Service Soon";

    else
        return "Immediate Maintenance Required";
}

#endif
