#ifndef AI_ENGINE_H
#define AI_ENGINE_H

#include "config.h"
#include "motor_model.h"

extern Eloquent::ML::Port::DecisionTree model;

extern int abnormalCount;
extern String fault;

inline int runAI(float features[8])
{
    return model.predict(features);
}

inline String classifyFault(int aiPrediction)
{
    switch(aiPrediction)
    {
        case 4:
            return "UNBALANCE";

        case 1:
            return "OVERCURRENT";

        case 2:
            return "OVERHEAT";

        case 3:
            return "STOPPED";

        default:
            return "NORMAL";
    }
}

inline String updateFaultState(int aiPrediction, float rpm)
{
    bool abnormal = false;
    String detectedFault = "NORMAL";

    if(rpm < RPM_MIN_VALID)
    {
        abnormalCount = 0;
        return "STOPPED";
    }

    if(rpm >= RPM_MIN_VALID)
    {
        detectedFault = classifyFault(aiPrediction);

        if(detectedFault != "NORMAL" && detectedFault != "STOPPED")
            abnormal = true;
    }

    if(abnormal)
        abnormalCount++;
    else
        abnormalCount = 0;

    if(abnormalCount >= ABNORMAL_CONFIRM_COUNT)
    {
        return detectedFault;
    }
    return "NORMAL";
}

#endif
