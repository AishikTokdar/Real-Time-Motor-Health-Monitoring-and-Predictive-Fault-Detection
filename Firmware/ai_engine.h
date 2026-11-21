#ifndef AI_ENGINE_H
#define AI_ENGINE_H

#include <Arduino.h>
#include <math.h>
#include "config.h"
#include "motor_model.h"

extern Eloquent::ML::Port::DecisionTree model;
extern int abnormalCount;

inline int runAI(float features[8])
{
    for (int i = 0; i < 8; ++i)
        if (!isfinite(features[i])) return -1;
    return model.predict(features);
}

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
                               float currentRMS = 0, float temperature = 0)
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
