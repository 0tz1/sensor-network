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
    console.log("📥 Incoming JSON:", data);

    const tags = `environment,node_id=${data.node_id}`;
    let fields = [];

    // Include all known fields if present
    const possibleFields = [
      'temperature', 'humidity', 'co2',
      'pm1p0', 'pm2p5', 'pm4p0', 'pm10p0', 'voc_index', 'nox_index',
      'uv', 'wifi', 'wind_speed', 'wind_direction', 'co', 'raindrop',
      'battery_percent', 'battery_status', 'pressure', 'iaq'
    ];

    possibleFields.forEach(field => {
      if (typeof data[field] !== 'undefined') {
        fields.push(`${field}=${data[field]}`);
      }
    });

    if (fields.length === 0) {
      console.warn("⚠️ No valid fields to send to InfluxDB. Skipping...");
      return;
    }

    const line = `${tags} ${fields.join(',')}`;

    await axios.post(INFLUX_URL, line, {
      headers: {
        'Authorization': `Token ${INFLUX_TOKEN}`,
        'Content-Type': 'text/plain',
      }
    });

    console.log('✅ Wrote to InfluxDB:', line);
  } catch (err) {
    console.error('❌ Error processing message:', err.message);
    console.error('⚠️  Raw MQTT message:', message.toString());
  }
});
