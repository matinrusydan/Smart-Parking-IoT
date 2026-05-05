const mqtt = require('mqtt');
require('dotenv').config();

const client = mqtt.connect(process.env.MQTT_BROKER_URL, {
    clientId: 'SmartParkingBackend_' + Math.random().toString(16).substr(2, 8),
    clean: true,
    connectTimeout: 4000,
    reconnectPeriod: 1000,
});

module.exports = client;
