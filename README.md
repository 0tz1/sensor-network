# 🌤️ Outdoor Climate Monitoring Station with Wi-Fi HaLow
**Embedded Systems Super Thesis Project — Jiahong He (47315389)**  
Semester 1, 2025 | Supervisor: Matthew D’Souza | Coordinator: Dr. Konstanty Bialkowski

## 📝 Project Overview
This project implements a **sustainable, long-range outdoor climate monitoring system** that utilizes **Wi-Fi HaLow (IEEE 802.11ah)** to enable real-time data acquisition and wireless transmission of environmental parameters. The system is centered around the **ESP32-S3-based HT-HC32 development board**, designed for remote, solar-powered deployment around UQ Lake.

Each sensor node collects and transmits data such as:
- Temperature
- Humidity
- UV Index
- CO₂ and PM2.5 Concentration
- Wind Speed and Rainfall

Data is transmitted using **MQTT over Wi-Fi HaLow** to a central access point and visualized through **InfluxDB + Grafana dashboards** hosted in a cloud containerized environment.

---

## ⚙️ Hardware Components
- **MCU**: ESP32-S3 (Heltec HT-HC32 Board)
- **Wi-Fi HaLow Module**: HT-HC01 (sub-GHz 802.11ah)
- **Sensors**: SEN55 (air quality), DHT22/SHT31 (temperature & humidity), UV sensor, raindrop sensor, anemometer, wind vane
- **Power**: Rechargeable LiPo Battery + 5V Solar Panel + HT-HC32 on-board charge controller
- **Storage**: Onboard MicroSD card for logging (optional)
- **Interfaces**: I2C, UART, ADC, SPI

---

## 📶 Network Architecture
- **Communication Protocol**: MQTT over Wi-Fi HaLow (sub-GHz 902–928 MHz)
- **Topology**: Star network; all sensor nodes communicate with one central access point
- **OTA Support**: Firmware updates over MQTT via chunked payloads
- **Cloud Backend**:
    - MQTT Broker: Mosquitto (Dockerized)
    - Time-Series DB: InfluxDB
    - Visualization: Grafana
    - Node.js bridge for MQTT ➜ InfluxDB

---

## 📦 Features
- 📡 **Wi-Fi HaLow Communication**: Reliable, long-range, and energy-efficient wireless data transmission.
- ☀️ **Solar Powered Nodes**: Fully autonomous, low-maintenance operation using sunlight.
- 📊 **Live Dashboard**: Real-time environmental data visualization with historical trend analysis.
- 🔁 **OTA Firmware Update**: Remotely update sensor node firmware using MQTT.
- 🌧️ **Weather-Resistant Design**: Compact and sealed enclosures suitable for outdoor deployment.

---

## 🏗️ Project Milestones
1. 📚 Literature Review & Component Selection
2. 🔌 Breadboard Testing & Circuit Validation
3. 🧾 PCB Design and Integration
4. 🔋 Battery Operation & Waterproof Enclosure
5. ☀️ Solar Integration & Field Optimization
6. 🧪 Field Deployment & Cloud Connectivity
7. 🌐 Full-Scale Deployment & Continuous Monitoring
8. 🔄 Maintenance, Scaling & Final Optimizations

---

## 📈 Key Performance Indicators
- **Data Accuracy**: Sensors calibrated and verified against known baselines.
- **Network Reliability**: >95% packet delivery rate over 300m.
- **Power Efficiency**: Up to 2 weeks autonomous runtime (night/cloudy).
- **Real-Time Access**: MQTT latency <1s for uplinked data.
- **Scalability**: Tested with up to 4 active nodes.
- **Environmental Robustness**: IP-rated enclosures and UV-resistant materials.

---

## 🛠️ Getting Started (Developers)
1. **Hardware Setup**: Flash firmware to HT-HC32 using Arduino IDE
2. **Install MQTT Stack**:
   ```bash
   docker-compose up -d
   pm2 start mqtt-bridge.js
