#include "SensorTasks.h"

const int node_id = 1;

TaskHandle_t mqttTaskHandle;
TaskHandle_t sensorPublishTaskHandle;

size_t ota_total_size = 0;
size_t ota_received_size = 0;
bool ota_in_progress = false;
bool ota_reboot_pending = false;

struct SensorData {
    float temperature, humidity, co2, pm25, uv, co, wind_speed, wind_direction;
    int raindrop, wifi_strength;
    String rtc;
};

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

SensorData getSensorData() {
    static float temp = 25.0, hum = 50.0, wind_speed = 4.0, wind_dir = 180.0;
    temp += random(-10, 11) * 0.1;
    hum += random(-5, 6) * 0.1;
    temp = constrain(temp, 15.0, 40.0);
    hum = constrain(hum, 20.0, 90.0);
    float base = 3.0 + sin(millis() / 60000.0) * 1.5;
    float gust = random(0, 10) > 8 ? random(8, 14) : 0;
    wind_speed = constrain(base + random(-10, 11) * 0.1 + gust, 0, 15);
    wind_dir += random(-5, 6);
    if (random(0, 100) < 5) wind_dir += random(-30, 31);
    while (wind_dir < 0) wind_dir += 360;
    while (wind_dir >= 360) wind_dir -= 360;

    SensorData d;
    d.temperature = temp;
    d.humidity = hum;
    d.co2 = 543;
    d.pm25 = 45;
    d.uv = 5.2;
    d.co = random(1, 11);
    d.wind_speed = wind_speed;
    d.wind_direction = wind_dir;
    d.raindrop = analogRead(34);
    d.wifi_strength = HaLow.RSSI();
    d.rtc = getRTC();
    return d;
}

String generateJSON(SensorData d) {
    StaticJsonDocument<384> doc;
    doc["node_id"] = node_id;
    doc["temperature"] = d.temperature;
    doc["humidity"] = d.humidity;
    doc["co2"] = d.co2;
    doc["pm25"] = d.pm25;
    doc["uv"] = d.uv;
    doc["co"] = d.co;
    doc["wind_speed"] = d.wind_speed;
    doc["wind_direction"] = d.wind_direction;
    doc["raindrop"] = d.raindrop;
    doc["wifi"] = d.wifi_strength;
    doc["rtc"] = d.rtc;
    String json;
    serializeJson(doc, json);
    return json;
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
    while (true) {
        if (!ota_in_progress && client.connected()) {
            SensorData data = getSensorData();
            String json = generateJSON(data);
            client.publish(mqtt_topic_data, json.c_str());
            Serial.println("📡 Published to MQTT: " + json);
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

// ======= Setup =======
void setup() {
    Serial.begin(115200);
    randomSeed(micros());
    initHaLow();
    initRTC();
    client.setServer(mqtt_server, mqtt_port);
    client.setCallback(mqttCallback);

    xTaskCreatePinnedToCore(mqttLoop, "MQTTTask", 8192, NULL, 1, &mqttTaskHandle, 1);
    xTaskCreatePinnedToCore(sensorPublishTask, "SensorPublishTask", 4096, NULL, 1, &sensorPublishTaskHandle, 0);
}

void loop() {}
