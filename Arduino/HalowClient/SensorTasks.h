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
    float temperature = 0.0;
    float humidity = 0.0;
    float co2 = 0.0;
    float pm25 = 0.0;
    float uv = 0.0;
    float co = 0.0;
    float wind_speed = 0.0;
    float wind_direction = 0.0;
    int battery_percent = 0;
    int battery_status = 0;
    int raindrop = 0;
    int wifi_strength = 0;
};

enum SensorType {
    SENSOR_DEVICE_INFO
};

struct SensorMessage {
    SensorType type;
    SensorData data;
};

size_t ota_total_size = 0;
size_t ota_received_size = 0;
bool ota_in_progress = false;
bool ota_reboot_pending = false;

// FreeRTOS task handles
TaskHandle_t mqttTaskHandle;
TaskHandle_t deviceInfoTaskHandle;
TaskHandle_t sensorPublishTaskHandle;

// FreeRTOS queue
QueueHandle_t sensorQueue;

#endif // SENSORTASKS_H
