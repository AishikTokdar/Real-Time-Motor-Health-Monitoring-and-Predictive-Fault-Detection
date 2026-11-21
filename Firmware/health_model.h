#ifndef HEALTH_MODEL_H
#define HEALTH_MODEL_H

#include "config.h"
#include <math.h>

extern float previousHealth;

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
