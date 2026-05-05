const mqttClient = require('../config/mqtt');
const parkingService = require('./parkingService');
require('dotenv').config();

const topic = process.env.MQTT_TOPIC;

mqttClient.on('connect', () => {
    console.log('Connected to MQTT Broker');
    mqttClient.subscribe(topic, (err) => {
        if (!err) {
            console.log(`Subscribed to topic: ${topic}`);
        } else {
            console.error('Failed to subscribe:', err);
        }
    });
});

mqttClient.on('message', async (topic, message) => {
    try {
        const payload = JSON.parse(message.toString());
        if (payload.slots && Array.isArray(payload.slots)) {
            for (const slot of payload.slots) {
                // Update database untuk setiap slot yang ada di array
                await parkingService.updateSlotStatus(slot);
            }
        }
    } catch (error) {
        console.error('Error parsing MQTT message:', error);
    }
});

module.exports = mqttClient;
