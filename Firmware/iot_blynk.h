#ifndef IOT_BLYNK_H
#define IOT_BLYNK_H

#include <Arduino.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <WiFi.h>
#include "config.h"
#include <BlynkSimpleEsp32.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

// Only the cloud task accesses WiFi/Blynk. The sensor task copies snapshots into
// a one-element queue, so DNS/TCP/cloud outages cannot block local monitoring.
struct CloudTelemetry {
    float temperature, vibration, current;
    int health, rpm, maintenanceScore;
    int normalLED, warningLED, faultLED, buzzer;
    unsigned long alertSequence;
    char fault[20], maintenance[48], alertFault[20];
};

static QueueHandle_t cloudQueue = nullptr;

inline void publishCloudData(const CloudTelemetry &data)
{
    if (!Blynk.connected()) return;
    // -1 represents unavailable values on the dashboard; never transmit NaN.
    Blynk.virtualWrite(V0, isfinite(data.temperature) ? data.temperature : -1);
    Blynk.virtualWrite(V1, isfinite(data.vibration) ? data.vibration : -1);
    Blynk.virtualWrite(V2, isfinite(data.current) ? data.current : -1);
    Blynk.virtualWrite(V3, data.health);
    Blynk.virtualWrite(V4, data.fault);
    Blynk.virtualWrite(V5, data.rpm);
    Blynk.virtualWrite(V6, data.normalLED);
    Blynk.virtualWrite(V7, data.warningLED);
    Blynk.virtualWrite(V8, data.faultLED);
    Blynk.virtualWrite(V9, data.buzzer);
    Blynk.virtualWrite(V10, data.maintenanceScore); // 0-100 points; -1 unavailable.
    Blynk.virtualWrite(V11, data.maintenance);
}

inline void cloudTask(void *)
{
    CloudTelemetry latest = {};
    bool haveData = false;
    unsigned long sentAlert = 0;
    unsigned long wifiAttempt = millis();
    unsigned long cloudAttempt = millis() - CLOUD_RETRY_MS;
    unsigned long published = millis();
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    bool useBlynk = BLYNK_AUTH_TOKEN[0] != '\0' && BLYNK_TEMPLATE_ID[0] != '\0';
    if (useBlynk) Blynk.config(BLYNK_AUTH_TOKEN);
    for (;;) {
        if (xQueueReceive(cloudQueue, &latest, 0) == pdTRUE) haveData = true;
        unsigned long now = millis();
        if (WiFi.status() != WL_CONNECTED) {
            if (useBlynk) Blynk.disconnect();
            if (now - wifiAttempt >= WIFI_RETRY_MS) {
                wifiAttempt = now;
                WiFi.disconnect();
                WiFi.begin(WIFI_SSID, WIFI_PASS);
            }
        } else if (useBlynk) {
            if (!Blynk.connected() && now - cloudAttempt >= CLOUD_RETRY_MS) {
                cloudAttempt = now;
                Blynk.connect(CLOUD_CONNECT_TIMEOUT_MS);
            }
            // A failed attempt is retried explicitly, not through run() each loop.
            if (Blynk.connected()) Blynk.run();
            if (Blynk.connected() && haveData) {
                if (latest.alertSequence != 0 && latest.alertSequence != sentAlert) {
                    char message[80];
                    snprintf(message, sizeof(message), "Motor alert: %s. Inspect motor and sensors.", latest.alertFault);
                    Blynk.logEvent("motor_fault", message);
                    // This records a connected send attempt, not server acknowledgment.
                    if (Blynk.connected()) sentAlert = latest.alertSequence;
                }
                if (now - published >= 2000) {
                    published = now;
                    publishCloudData(latest);
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

inline void beginIoT()
{
    if (WIFI_SSID[0] == '\0') return; // Offline monitoring needs no credentials.
    cloudQueue = xQueueCreate(1, sizeof(CloudTelemetry));
    if (cloudQueue && xTaskCreate(cloudTask, "motor-cloud", 4096, nullptr, 1, nullptr) != pdPASS) {
        vQueueDelete(cloudQueue);
        cloudQueue = nullptr;
        Serial.println("Cloud task unavailable; local monitoring continues");
    }
}

inline void queueTelemetry(CloudTelemetry &data)
{
    static unsigned long alertSequence = 0;
    static bool alertActive = false;
    static char alertFault[20] = "";
    bool critical = (data.health >= 0 && data.health < HEALTH_CRITICAL_LEVEL) ||
                    (strcmp(data.fault, "NORMAL") != 0 && strcmp(data.fault, "STOPPED") != 0);
    if (critical && !alertActive) {
        ++alertSequence;
        if (alertSequence == 0) ++alertSequence;
        snprintf(alertFault, sizeof(alertFault), "%s",
                 strcmp(data.fault, "NORMAL") == 0 || strcmp(data.fault, "STOPPED") == 0
                 ? "LOW_HEALTH" : data.fault);
        alertActive = true;
    } else if (!critical && data.health > 40) {
        alertActive = false;
    }
    data.alertSequence = alertSequence;
    snprintf(data.alertFault, sizeof(data.alertFault), "%s", alertFault);
    // Queue overwrite keeps the latest alert pending even across offline recovery.
    if (cloudQueue) xQueueOverwrite(cloudQueue, &data);
}
#endif
