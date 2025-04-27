#ifndef SENSORTASKS_H
#define SENSORTASKS_H

#include <Arduino.h>
#include <ArduinoJson.h>

#include <HaLow.h>
#include <WiFi.h>
#include <Update.h>
#include <WebServer.h>
#include <PubSubClient.h>

#include <time.h>

#define STATUS_INTERVAL 5000

WiFiClient espClient;
PubSubClient client(espClient);

// WiFi credentials
const char* ssid = "HT-H7608-CC1D";
const char* password = "heltec.org";

// MQTT
const char* mqtt_server = "192.168.1.137";
const int mqtt_port = 1883;

// MQTT Topics
const char* mqtt_topic_data      = "sensor/data";
const char* mqtt_topic_ota_start = "sensor/ota/start";
const char* mqtt_topic_ota_chunk = "sensor/ota/chunk";
const char* mqtt_topic_ota_done  = "sensor/ota/done";

struct SensorData {
    float temperature, humidity, co2, pm25, uv, co, wind_speed, wind_direction;
    int battery_percent;
    int battery_status;
    int battery_voltage;
    int raindrop, wifi_strength;
    String rtc;
};

size_t ota_total_size = 0;
size_t ota_received_size = 0;
bool ota_in_progress = false;
bool ota_reboot_pending = false;

TaskHandle_t sensorTaskHandle;


#endif
