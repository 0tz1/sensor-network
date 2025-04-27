const fs = require('fs');
const mqtt = require('mqtt');
const path = require('path');

// === CONFIGURATION ===
const BROKER       = 'mqtt://192.168.1.137';
const TOPIC_START  = 'sensor/ota/start';
const TOPIC_CHUNK  = 'sensor/ota/chunk';
const TOPIC_DONE   = 'sensor/ota/done';
const FILE_PATH    = '/Users/ceylan/Library/Caches/arduino/sketches/9216BF5B202591106F2E159A1E328B7E/HalowClient.ino.bin';
const CHUNK_SIZE   = 128;
const CHUNK_DELAY  = 10; // ms

// === MQTT SETUP ===
const client = mqtt.connect(BROKER);

client.on('connect', async () => {
    console.log(`📡 Connected to MQTT broker: ${BROKER}`);
    try {
        await sendFirmware(FILE_PATH);
        console.log('🎉 OTA complete!');
    } catch (err) {
        console.error('❌ OTA failed:', err.message);
    } finally {
        client.end();
    }
});

client.on('error', err => {
    console.error('❌ MQTT error:', err.message);
});

function delay(ms) {
    return new Promise(resolve => setTimeout(resolve, ms));
}

async function sendFirmware(filePath) {
    if (!fs.existsSync(filePath)) throw new Error('Firmware file not found');

    const firmware = fs.readFileSync(filePath);
    const totalSize = firmware.length;
    console.log(`📦 Firmware size: ${totalSize} bytes`);

    // Step 1: Notify OTA start
    await new Promise((resolve, reject) => {
        client.publish(TOPIC_START, String(totalSize), { qos: 0 }, (err) => {
            if (err) reject(err);
            else resolve();
        });
    });
    console.log(`🚀 Sent OTA start`);

    // Step 2: Send firmware in chunks
    let offset = 0;
    while (offset < totalSize) {
        const chunk = firmware.slice(offset, offset + CHUNK_SIZE);

        await new Promise((resolve, reject) => {
            client.publish(TOPIC_CHUNK, chunk, { qos: 0 }, (err) => {
                if (err) reject(err);
                else resolve();
            });
        });

        offset += chunk.length;

        const percent = ((offset / totalSize) * 100).toFixed(1);
        process.stdout.write(`➡️ Chunk: ${chunk.length} bytes (${offset}/${totalSize}) - ${percent}%\r`);

        await delay(CHUNK_DELAY);
    }

    // Step 3: Notify OTA done
    await new Promise((resolve, reject) => {
        client.publish(TOPIC_DONE, '', { qos: 0 }, (err) => {
            if (err) reject(err);
            else resolve();
        });
    });
    console.log('\n✅ Sent OTA done');
}
