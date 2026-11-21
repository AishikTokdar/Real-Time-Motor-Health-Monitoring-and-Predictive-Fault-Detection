#ifndef CONFIG_H
#define CONFIG_H

#define IR_PIN          27
#define CURRENT_PIN     34
#define ONE_WIRE_BUS    4

#define BUZZER_PIN      26
#define LED_NORMAL      18
#define LED_WARNING     19
#define LED_FAULT       33

#define SDA_PIN 21
#define SCL_PIN 22

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDRESS 0x3C

#define VIBRATION_THRESHOLD   0.35
#define CURRENT_THRESHOLD     0.25
#define TEMPERATURE_THRESHOLD 60.0

#define ACS712_SENSITIVITY 0.1
#define ACS712_OFFSET      2.5

#define LOG_DEBUG   1
#define LOG_INFO    2
#define LOG_DATASET 3

#if __has_include("secrets.h")
#include "secrets.h"
#endif
#ifndef WIFI_SSID
#define WIFI_SSID ""
#endif
#ifndef WIFI_PASS
#define WIFI_PASS ""
#endif
#ifndef BLYNK_TEMPLATE_ID
#define BLYNK_TEMPLATE_ID ""
#endif
#ifndef BLYNK_TEMPLATE_NAME
#define BLYNK_TEMPLATE_NAME "Motor Health Monitoring"
#endif
#ifndef BLYNK_AUTH_TOKEN
#define BLYNK_AUTH_TOKEN ""
#endif

#define RPM_MIN_VALID 100
#define RPM_MAX_VALID 3000

#define HEALTH_WARNING_LEVEL 75
#define HEALTH_CRITICAL_LEVEL 30

#define ABNORMAL_CONFIRM_COUNT 3

#define VIBRATION_SAMPLES 150
#define CURRENT_SAMPLES 500

// Magnitude features retain gravity, matching the recorded training data.
#define VIBRATION_SAMPLE_INTERVAL_MS 10
#define RPM_INTERVAL_MS 1000
#define RPM_PULSES_PER_REVOLUTION 1
#define TEMPERATURE_CONVERSION_MS 750
#define LCD_HEARTBEAT_MS 2000
#define WIFI_RETRY_MS 10000
#define CLOUD_RETRY_MS 10000
#define CLOUD_CONNECT_TIMEOUT_MS 3000

#endif
