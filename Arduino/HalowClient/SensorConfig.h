#ifndef SENSORCONFIG_H
#define SENSORCONFIG_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <time.h>

#include <HaLow.h>
#include <WiFi.h>
#include <Update.h>
#include <WebServer.h>
#include <PubSubClient.h>
#include <SensirionI2cScd4x.h>
#include <Adafruit_BME680.h>

// ===== Pins =====
#define BATTERY_ADC_PIN 1
#define CHARGING_STATUS_PIN 15
#define MQ_PIN 10
#define I2C_SDA_PIN 15
#define I2C_SCL_PIN 16

// ===== Dummy Data Constants =====
#define PM25_DUMMY_VALUE    45
#define UV_DUMMY_VALUE      5.2
#define WIND_BASE           3.0
#define WIND_VARIATION      1.5
#define RAINDROP_ANALOG_PIN 34

#define STATUS_INTERVAL 5000

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
    float pressure = 0.0;
    float iaq = 0.0;
    float wind_speed = 0.0;
    float wind_direction = 0.0;
    int co = 0;
    int battery_percent = 0;
    int battery_status = 0;
    int raindrop = 0;
    int wifi_strength = 0;
};

enum SensorType {
    SENSOR_DEVICE_INFO,
    SENSOR_SCD40,
    SENSOR_BME680,
    SENSOR_MQ
};

struct SensorMessage {
    SensorType type;
    SensorData data;
};

#endif // SENSORCONFIG_H
