#include "SensorTasks.h"

const int node_id = 1;

// --- Battery ADC Pin ---
#define BATTERY_ADC_PIN 1

// --- Charging Status Pin (connect CHG pin here) ---
#define CHARGING_STATUS_PIN 15

// ======= RTC Init =======
void initRTC() {
    struct tm t = { 0 };
    t.tm_year = 2025 - 1900;
    t.tm_mon  = 3  - 1;
    t.tm_mday = 26;
    t.tm_hour = 14;
    t.tm_min  = 30;
    t.tm_sec  = 0;
    time_t now = mktime(&t);
    struct timeval now_val = { .tv_sec = now };
    settimeofday(&now_val, NULL);
}

// ===== Battery Functions =====
float readBatteryVoltageSmoothed() {
    const int samples = 10;
    long total = 0;
    for (int i = 0; i < samples; i++) {
        total += analogRead(BATTERY_ADC_PIN);
        delay(5);
    }
    float avg = float(total) / samples;
    float v   = (avg / 4095.0f) * 3.3f;
    return v * 2.0f;  // voltage divider factor
}

int batteryPercentage(float voltage) {
    if (voltage >= 4.2f) return 100;
    if (voltage <= 3.3f) return   0;
    return int((voltage - 3.3f) * 100.0f / (4.2f - 3.3f));
}

int getBatteryStatus() {
    return (digitalRead(CHARGING_STATUS_PIN) == LOW) ? 1 : 0;
}

// ===== Device Info Task =====
void deviceInfoTask(void* pvParameters) {
    while (true) {
        SensorMessage msg;
        msg.type                 = SENSOR_DEVICE_INFO;
        float v                  = readBatteryVoltageSmoothed();
        msg.data.battery_percent = batteryPercentage(v);
        msg.data.battery_status  = getBatteryStatus();
        msg.data.wifi_strength   = HaLow.RSSI();
        xQueueSend(sensorQueue, &msg, portMAX_DELAY);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

// ===== SCD40 Sensor Task =====
void scd40Task(void* pvParameters) {
    uint16_t co2;
    float temperature, humidity;
    while (true) {
        if (scd4x.readMeasurement(co2, temperature, humidity) == 0) {
            SensorMessage msg;
            msg.type             = SENSOR_SCD40;
            msg.data.co2         = float(co2);
            msg.data.temperature = temperature;
            msg.data.humidity    = humidity;
            xQueueSend(sensorQueue, &msg, portMAX_DELAY);
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

// ===== BME680 Sensor Task =====
void bme680Task(void* pvParameters) {
    while (true) {
        if (!bme.performReading()) {
            Serial.println("❌ BME680 reading failed");
        } else {
            SensorMessage msg;
            msg.type              = SENSOR_BME680;
            msg.data.pressure     = bme.pressure / 100.0f;   // hPa
            float gas             = bme.gas_resistance / 1000.0f; // kΩ
            if (gas > 50.0f) gas = 50.0f;
            msg.data.iaq          = (1.0f - (gas / 50.0f)) * 500.0f;
            xQueueSend(sensorQueue, &msg, portMAX_DELAY);
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

// ===== MQTT Callback & Reconnect =====
void mqttCallback(char* topic, byte* payload, unsigned int length) {
    if (strcmp(topic, mqtt_topic_ota_start) == 0) {
        ota_total_size   = atoi((char*)payload);
        ota_received_size = 0;
        ota_in_progress  = true;
        ota_reboot_pending = false;
        if (!Update.begin(ota_total_size)) {
            Serial.println("❌ OTA begin failed.");
            ota_in_progress = false;
        } else {
            Serial.printf("🛠️ OTA start: %u bytes\n", ota_total_size);
        }
        return;
    }
    if (strcmp(topic, mqtt_topic_ota_chunk) == 0 && ota_in_progress) {
        size_t written = Update.write(payload, length);
        ota_received_size += written;
        if ((ota_received_size % 20480) < length) Serial.print("*");
        if (written != length) {
            Serial.println("\n❌ Chunk write failed.");
            ota_in_progress = false;
            Update.abort();
        }
        return;
    }
    if (strcmp(topic, mqtt_topic_ota_done) == 0) {
        Serial.println("\n📦 OTA done message received.");
        if (!ota_in_progress) {
            Serial.println("⚠️ OTA was not in progress. Ignoring.");
            return;
        }
        if (Update.end(true)) {
            Serial.printf("✅ OTA finished: %u bytes written.\n", ota_received_size);
            ota_reboot_pending = true;
        } else {
            Serial.printf("❌ OTA end failed. Error: %s\n", Update.errorString());
        }
        ota_in_progress = false;
        return;
    }
}

void reconnectMQTT() {
    while (!client.connected()) {
        Serial.print("🔌 Connecting to MQTT...");
        if (client.connect("ESP32Client")) {
            Serial.println("✅ connected.");
            client.subscribe(mqtt_topic_ota_start);
            client.subscribe(mqtt_topic_ota_chunk);
            client.subscribe(mqtt_topic_ota_done);
        } else {
            Serial.print("❌ failed, rc=");
            Serial.println(client.state());
            vTaskDelay(pdMS_TO_TICKS(5000));
        }
    }
}

void mqttLoop(void* pvParameters) {
    while (true) {
        client.loop();
        if (!client.connected() && !ota_in_progress) reconnectMQTT();
        if (ota_reboot_pending) {
            delay(1000);
            ESP.restart();
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

// ===== Sensor Publishing Task (no fusion) =====
void sensorPublishTask(void* pvParameters) {
    SensorData combinedData = {};
    static float wind_speed  = 4.0f, wind_dir = 180.0f;

    while (true) {
        SensorMessage msg;
        while (xQueueReceive(sensorQueue, &msg, 0)) {
            switch (msg.type) {
                case SENSOR_DEVICE_INFO:
                    combinedData.battery_percent = msg.data.battery_percent;
                    combinedData.battery_status  = msg.data.battery_status;
                    combinedData.wifi_strength   = msg.data.wifi_strength;
                    break;
                case SENSOR_SCD40:
                    combinedData.co2         = msg.data.co2;
                    combinedData.temperature = msg.data.temperature;
                    combinedData.humidity    = msg.data.humidity;
                    break;
                case SENSOR_BME680:
                    combinedData.pressure    = msg.data.pressure;
                    combinedData.iaq         = msg.data.iaq;
                    break;
            }
        }

        // — Dummy/random values —
        float base = WIND_BASE + sin(millis() / 60000.0f) * WIND_VARIATION;
        float gust = (random(0,10) > 8) ? random(8,14) : 0;
        wind_speed = constrain(base + random(-10,11)*0.1f + gust, 0.0f, 15.0f);
        wind_dir  += random(-5, 6);
        if (wind_dir < 0) wind_dir += 360;
        if (wind_dir >= 360) wind_dir -= 360;

        combinedData.pm25           = PM25_DUMMY_VALUE;
        combinedData.uv             = UV_DUMMY_VALUE;
        combinedData.co             = random(1, 11);
        combinedData.wind_speed     = wind_speed;
        combinedData.wind_direction = wind_dir;
        combinedData.raindrop       = analogRead(RAINDROP_ANALOG_PIN);

        // — Build & publish JSON —
        StaticJsonDocument<512> doc;
        doc["node_id"]         = node_id;
        doc["battery_percent"] = combinedData.battery_percent;
        doc["battery_status"]  = combinedData.battery_status;
        doc["wifi"]            = combinedData.wifi_strength;
        doc["co2"]             = combinedData.co2;
        doc["temperature"]     = combinedData.temperature;
        doc["humidity"]        = combinedData.humidity;
        doc["pressure"]        = combinedData.pressure;
        doc["iaq"]             = combinedData.iaq;
        doc["pm25"]            = combinedData.pm25;
        doc["uv"]              = combinedData.uv;
        doc["co"]              = combinedData.co;
        doc["wind_speed"]      = combinedData.wind_speed;
        doc["wind_direction"]  = combinedData.wind_direction;
        doc["raindrop"]        = combinedData.raindrop;

        String json;
        serializeJson(doc, json);

        if (client.connected()) {
            client.publish(mqtt_topic_data, json.c_str());
            Serial.println("✅ Packet Sent!");
        }

        vTaskDelay(pdMS_TO_TICKS(STATUS_INTERVAL));
    }
}

// ===== Wi-Fi HaLow Init =====
void initHaLow() {
    Serial.println("🚀 Initializing HaLow...");
    HaLow.init("AU");
    HaLow.begin(ssid, password);
    while (HaLow.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\n✅ Connected to HaLow!");
    Serial.println("🌍 IP: " + HaLow.localIP().toString());
}

// ===== Arduino Setup & Loop =====
void setup() {
    Serial.begin(115200);
    randomSeed(micros());

    Wire.begin(15, 16);
    scd4x.begin(Wire, 0x62);
    delay(30);
    scd4x.wakeUp();
    scd4x.stopPeriodicMeasurement();
    scd4x.reinit();
    scd4x.startPeriodicMeasurement();

    if (!bme.begin()) {
        Serial.println("❌ Could not find BME680 sensor!");
        while (1);
    }
    bme.setTemperatureOversampling(BME680_OS_8X);
    bme.setHumidityOversampling(BME680_OS_2X);
    bme.setPressureOversampling(BME680_OS_4X);
    bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
    bme.setGasHeater(320, 150);

    initHaLow();
    initRTC();

    client.setServer(mqtt_server, mqtt_port);
    client.setCallback(mqttCallback);

    sensorQueue = xQueueCreate(10, sizeof(SensorMessage));

    xTaskCreatePinnedToCore(mqttLoop, "MQTTTask", 8192, NULL, 1, &mqttTaskHandle, 1);
    xTaskCreatePinnedToCore(deviceInfoTask, "DeviceInfoTask", 4096, NULL, 1, &deviceInfoTaskHandle, 0);
    xTaskCreatePinnedToCore(scd40Task, "SCD40Task", 4096, NULL, 1, &scd40TaskHandle, 0);
    xTaskCreatePinnedToCore(bme680Task, "BME680Task", 4096, NULL, 1, &bme680TaskHandle, 0);
    xTaskCreatePinnedToCore(sensorPublishTask, "SensorPublishTask", 4096, NULL, 1, &sensorPublishTaskHandle, 0);
}

void loop() {}
