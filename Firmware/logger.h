#ifndef LOGGER_H
#define LOGGER_H

#include "config.h"

extern int logMode;
static String datasetLabel = "UNLABELED";

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
