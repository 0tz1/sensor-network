const mqtt = require('mqtt');
const axios = require('axios');

const MQTT_BROKER = 'mqtt://192.168.1.137';
const MQTT_TOPIC = 'sensor/data';

const INFLUX_URL = 'http://localhost:8086/api/v2/write?org=my-org&bucket=sensors&precision=s';
const INFLUX_TOKEN = '8DkRu02wMx9c7OgL3tbbVOtujSyvOPncfcFKmYNyvCkK8m_i2hhzKLtnSfAF_0VjxDXYHUMsEWhvVXvq9o_cYw==';

const mqttClient = mqtt.connect(MQTT_BROKER);

mqttClient.on('connect', () => {
  console.log(`📶 Connected to MQTT broker at ${MQTT_BROKER}`);
  mqttClient.subscribe(MQTT_TOPIC, (err) => {
    if (!err) console.log(`🔔 Subscribed to topic: ${MQTT_TOPIC}`);
  });
});

mqttClient.on('message', async (topic, message) => {
  try {
    const data = JSON.parse(message.toString());

    // Convert JSON to InfluxDB Line Protocol
    const line = `environment,node_id=${data.node_id} ` +
        `temperature=${data.temperature},humidity=${data.humidity},` +
        `co2=${data.co2},pm25=${data.pm25},uv=${data.uv},wifi=${data.wifi},` +
        `wind_speed=${data.wind_speed},wind_direction=${data.wind_direction},` +
        `co=${data.co},raindrop=${data.raindrop},` +
        `battery_percent=${data.battery_percent},battery_status=${data.battery_status}`;

    // Send to InfluxDB
    await axios.post(INFLUX_URL, line, {
      headers: {
        'Authorization': `Token ${INFLUX_TOKEN}`,
        'Content-Type': 'text/plain',
      }
    });

    console.log('✅ Wrote to InfluxDB:', line);
  } catch (err) {
    console.error('❌ Error processing message:', err.message);
  }
});
