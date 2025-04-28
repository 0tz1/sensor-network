#include "SensorTasks.h"

const int node_id = 1;

// --- Battery ADC Pin ---
#define BATTERY_ADC_PIN 1
// --- Charging Status Pin (connect CHG pin here) ---
#define CHARGING_STATUS_PIN 2

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

String getRTC() {
    time_t now = time(nullptr);
    struct tm* timeinfo = localtime(&now);
    char buffer[32];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
    return String(buffer);
}

// ======= Battery =======
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

// ======= Device Info Task =======
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

// ======= MQTT Callback =======
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

// ======= MQTT Reconnect =======
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

// ======= MQTT Task =======
void mqttLoop(void* pvParameters) {
    while (true) {
        client.loop();

        if (!client.connected() && !ota_in_progress) {
            reconnectMQTT();
        }

        if (ota_reboot_pending) {
            Serial.println("🔁 Rebooting in 1s...");
            delay(1000);
            ESP.restart();
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

// ======= Sensor Publishing Task =======
void sensorPublishTask(void* pvParameters) {
    SensorData combinedData;
    static float temp = 25.0, hum = 50.0;
    static float wind_speed = 4.0, wind_dir = 180.0;

    while (true) {
        SensorMessage msg;
        while (xQueueReceive(sensorQueue, &msg, 0)) {
            if (msg.type == SENSOR_DEVICE_INFO) {
                combinedData.battery_percent = msg.data.battery_percent;
                combinedData.battery_status = msg.data.battery_status;
                combinedData.wifi_strength = msg.data.wifi_strength;
            }
        }

        // Update dummy values
        temp += random(-10, 11) * 0.1;
        hum += random(-5, 6) * 0.1;
        temp = constrain(temp, 15.0, 40.0);
        hum = constrain(hum, 20.0, 90.0);

        float base = 3.0 + sin(millis() / 60000.0) * 1.5;
        float gust = random(0, 10) > 8 ? random(8, 14) : 0;
        wind_speed = constrain(base + random(-10, 11) * 0.1 + gust, 0, 15);
        wind_dir += random(-5, 6);
        while (wind_dir < 0) wind_dir += 360;
        while (wind_dir >= 360) wind_dir -= 360;

        combinedData.temperature = temp;
        combinedData.humidity = hum;
        combinedData.co2 = 543;
        combinedData.pm25 = 45;
        combinedData.uv = 5.2;
        combinedData.co = random(1, 11);
        combinedData.wind_speed = wind_speed;
        combinedData.wind_direction = wind_dir;
        combinedData.raindrop = analogRead(34);

        // Build JSON and publish
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
        doc["wifi"] = combinedData.wifi_strength;
        doc["battery_percent"] = combinedData.battery_percent;
        doc["battery_status"] = combinedData.battery_status;

        String json;
        serializeJson(doc, json);
        if (client.connected()) {
            client.publish(mqtt_topic_data, json.c_str());
            Serial.println("✅ Packet Sent!");
        }

        vTaskDelay(pdMS_TO_TICKS(STATUS_INTERVAL));
    }
}

// ======= WiFi Init =======
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

// ======= Arduino Setup =======
void setup() {
    Serial.begin(115200);
    randomSeed(micros());
    initHaLow();
    initRTC();

    client.setServer(mqtt_server, mqtt_port);
    client.setCallback(mqttCallback);

    sensorQueue = xQueueCreate(10, sizeof(SensorMessage)); // Create queue

    xTaskCreatePinnedToCore(mqttLoop, "MQTTTask", 8192, NULL, 1, &mqttTaskHandle, 1);
    xTaskCreatePinnedToCore(deviceInfoTask, "DeviceInfoTask", 4096, NULL, 1, &deviceInfoTaskHandle, 0);
    xTaskCreatePinnedToCore(sensorPublishTask, "SensorPublishTask", 4096, NULL, 1, &sensorPublishTaskHandle, 0);
}

void loop() {}
