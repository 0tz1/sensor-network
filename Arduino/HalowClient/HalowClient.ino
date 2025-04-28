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
    t.tm_mon = 3 - 1;
    t.tm_mday = 26;
    t.tm_hour = 14;
    t.tm_min = 30;
    t.tm_sec = 0;
    time_t now = mktime(&t);
    struct timeval now_val = { .tv_sec = now };
    settimeofday(&now_val, NULL);
}

// ===== Battery Functions =====
float readBatteryVoltageSmoothed() {
    const int samples = 10;
    int total = 0;
    for (int i = 0; i < samples; i++) {
        total += analogRead(BATTERY_ADC_PIN);
        delay(5);
    }
    float avg_adc = (float)total / samples;
    float voltage = (avg_adc / 4095.0) * 3.3;
    return voltage * 2.0;
}

int batteryPercentage(float voltage) {
    if (voltage >= 4.2) return 100;
    if (voltage <= 3.3) return 0;
    return (int)((voltage - 3.3) * 100.0 / (4.2 - 3.3));
}

int getBatteryStatus() {
    int stat = digitalRead(CHARGING_STATUS_PIN);
    return (stat == LOW) ? 1 : 0;
}

// ===== Device Info Task =====
void deviceInfoTask(void* pvParameters) {
    while (true) {
        SensorMessage msg;
        msg.type = SENSOR_DEVICE_INFO;

        float batt_voltage = readBatteryVoltageSmoothed();
        msg.data.battery_percent = batteryPercentage(batt_voltage);
        msg.data.battery_status = getBatteryStatus();
        msg.data.wifi_strength = HaLow.RSSI();

        xQueueSend(sensorQueue, &msg, portMAX_DELAY);

        vTaskDelay(pdMS_TO_TICKS(2000)); // Every 2 seconds
    }
}

// ===== SCD40 Sensor Task =====
void scd40Task(void* pvParameters) {
    uint16_t co2;
    float temperature, humidity;
    while (true) {
        int16_t error = scd4x.readMeasurement(co2, temperature, humidity);
        if (error == 0) {
            SensorMessage msg;
            msg.type = SENSOR_SCD40;
            msg.data.co2 = (float)co2;
            msg.data.temperature = temperature;
            msg.data.humidity = humidity;
            xQueueSend(sensorQueue, &msg, portMAX_DELAY);
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void bme680Task(void* pvParameters) {
    while (true) {
        if (!bme.performReading()) {
            Serial.println("❌ BME680 reading failed");
        } else {
            SensorMessage msg;
            msg.type = SENSOR_BME680;
            msg.data.pressure = bme.pressure / 100.0; // hPa
            // Simple IAQ approximation
            float gas = bme.gas_resistance / 1000.0; // kOhm
            if (gas > 50) gas = 50;
            float iaq = (1.0 - (gas / 50.0)) * 500.0; // Map from 0-50 kOhm to 0-500 IAQ
            msg.data.iaq = iaq;
            xQueueSend(sensorQueue, &msg, portMAX_DELAY);
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

// ===== MQTT Functions =====
void mqttCallback(char* topic, byte* payload, unsigned int length) {
    if (strcmp(topic, mqtt_topic_ota_start) == 0) {
        ota_total_size = atoi((char*)payload);
        ota_received_size = 0;
        ota_in_progress = true;
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

        if ((ota_received_size % 20480) < length) {
            Serial.printf("*");
        }

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

// ===== Sensor Publishing Task =====
void fuseHumidityOnly(float scd40_humi, float bme680_humi, float& fused_humi) {
    float humi_diff = fabs(scd40_humi - bme680_humi);

    if (humi_diff <= 5.0f) {
        fused_humi = (scd40_humi * HUMI_WEIGHT_SCD40) + (bme680_humi * HUMI_WEIGHT_BME680);
    } else {
        fused_humi = scd40_humi; // Trust SCD40 more
    }
}

void sensorPublishTask(void* pvParameters) {
    SensorData combinedData;
    static float wind_speed = 4.0, wind_dir = 180.0;

    float scd40_temp = 0, scd40_humi = 0;
    float bme680_temp = 0, bme680_humi = 0;
    bool scd40_ready = false, bme680_ready = false;

    while (true) {
        SensorMessage msg;
        while (xQueueReceive(sensorQueue, &msg, 0)) {
            if (msg.type == SENSOR_DEVICE_INFO) {
                combinedData.battery_percent = msg.data.battery_percent;
                combinedData.battery_status = msg.data.battery_status;
                combinedData.wifi_strength = msg.data.wifi_strength;
            } else if (msg.type == SENSOR_SCD40) {
                // Only accept valid SCD40 readings
                if (msg.data.temperature > -50 && msg.data.temperature < 80 &&
                    msg.data.humidity >= 0 && msg.data.humidity <= 100) {
                    combinedData.co2 = msg.data.co2;
                    scd40_temp = msg.data.temperature;
                    scd40_humi = msg.data.humidity;
                    scd40_ready = true;
                }
            } else if (msg.type == SENSOR_BME680) {
                // Only accept valid BME680 readings
                if (msg.data.pressure > 800 && msg.data.pressure < 1200) {
                    combinedData.pressure = msg.data.pressure;
                    combinedData.iaq = msg.data.iaq;
                    bme680_temp = msg.data.temperature;
                    bme680_humi = msg.data.humidity;
                    bme680_ready = true;
                }
            }
        }

        // Use SCD40 temp directly, fuse humidity if both ready
        if (scd40_ready && bme680_ready) {
            combinedData.temperature = scd40_temp;
            fuseHumidityOnly(scd40_humi, bme680_humi, combinedData.humidity);
        } else if (scd40_ready) {
            combinedData.temperature = scd40_temp;
            combinedData.humidity = scd40_humi;
        } else if (bme680_ready) {
            combinedData.temperature = bme680_temp;
            combinedData.humidity = bme680_humi;
        }

        // Generate dummy/random values
        float base = WIND_BASE + sin(millis() / 60000.0) * WIND_VARIATION;
        float gust = random(0, 10) > 8 ? random(8, 14) : 0;
        wind_speed = constrain(base + random(-10, 11) * 0.1 + gust, 0, 15);
        wind_dir += random(-5, 6);
        while (wind_dir < 0) wind_dir += 360;
        while (wind_dir >= 360) wind_dir -= 360;

        combinedData.pm25 = PM25_DUMMY_VALUE;
        combinedData.uv = UV_DUMMY_VALUE;
        combinedData.co = random(1, 11);
        combinedData.wind_speed = wind_speed;
        combinedData.wind_direction = wind_dir;
        combinedData.raindrop = analogRead(RAINDROP_ANALOG_PIN);

        // Prepare JSON and send
        StaticJsonDocument<512> doc;
        doc["node_id"] = node_id;
        doc["temperature"] = combinedData.temperature;
        doc["humidity"] = combinedData.humidity;
        doc["co2"] = combinedData.co2;
        doc["pm25"] = combinedData.pm25;
        doc["uv"] = combinedData.uv;
        doc["co"] = combinedData.co;
        doc["wind_speed"] = combinedData.wind_speed;
        doc["wind_direction"] = combinedData.wind_direction;
        doc["raindrop"] = combinedData.raindrop;
        doc["pressure"] = combinedData.pressure;
        doc["iaq"] = combinedData.iaq;
        doc["wifi"] = combinedData.wifi_strength;
        doc["battery_percent"] = combinedData.battery_percent;
        doc["battery_status"] = combinedData.battery_status;

        String json;
        serializeJson(doc, json);

        if (client.connected()) {
            client.publish(mqtt_topic_data, json.c_str());
            Serial.println("✅ Packet Sent!");
        }

        scd40_ready = false;
        bme680_ready = false;

        vTaskDelay(pdMS_TO_TICKS(STATUS_INTERVAL));
    }
}

// ===== WiFi Init =====
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

// ===== Arduino Setup =====
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
