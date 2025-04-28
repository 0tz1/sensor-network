#ifndef SENSORTASKS_H
#define SENSORTASKS_H

#include <Arduino.h>
#include <ArduinoJson.h>

#include <HaLow.h>
#include <WiFi.h>
#include <Update.h>
#include <WebServer.h>
#include <PubSubClient.h>
#include <SensirionI2cScd4x.h>
#include <Adafruit_BME680.h>

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
    float pressure = 0.0;
    float iaq = 0.0;
    float wind_speed = 0.0;
    float wind_direction = 0.0;
    int battery_percent = 0;
    int battery_status = 0;
    int raindrop = 0;
    int wifi_strength = 0;
};

enum SensorType {
    SENSOR_DEVICE_INFO,
    SENSOR_SCD40,
    SENSOR_BME680
};

struct SensorMessage {
    SensorType type;
    SensorData data;
};

size_t ota_total_size = 0;
size_t ota_received_size = 0;
bool ota_in_progress = false;
bool ota_reboot_pending = false;

// ===== Fusion Weights =====
#define HUMI_WEIGHT_SCD40   0.8
#define HUMI_WEIGHT_BME680  0.2

// ===== Dummy Data Constants =====
#define PM25_DUMMY_VALUE    45
#define UV_DUMMY_VALUE      5.2
#define WIND_BASE           3.0
#define WIND_VARIATION      1.5
#define RAINDROP_ANALOG_PIN 34

// FreeRTOS task handles
TaskHandle_t mqttTaskHandle;
TaskHandle_t deviceInfoTaskHandle;
TaskHandle_t sensorPublishTaskHandle;
TaskHandle_t scd40TaskHandle;
TaskHandle_t bme680TaskHandle;

SensirionI2cScd4x scd4x;
Adafruit_BME680 bme;

// FreeRTOS queue
QueueHandle_t sensorQueue;

#endif // SENSORTASKS_H
