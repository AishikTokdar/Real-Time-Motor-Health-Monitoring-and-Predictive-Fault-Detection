#ifndef FEATURES_H
#define FEATURES_H

#include "config.h"

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

void buildFeatureArray(MotorFeatures &f, float features[8])
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

void normalizeFeatures(float features[8])
{
}

#endif
