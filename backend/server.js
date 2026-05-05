const express = require('express');
const cors = require('cors');
require('dotenv').config();

// Inisialisasi proses Subscriber MQTT saat server jalan
require('./services/mqttSubscriber');

const apiRoutes = require('./routes/api');

const app = express();
const port = process.env.PORT || 3000;

app.use(cors());
app.use(express.json());

// Load Routing API
app.use('/api', apiRoutes);

app.get('/', (req, res) => {
    res.json({ 
        message: 'Smart Parking Backend API is running',
        endpoints: [
            '/api/slots',
            '/api/events',
            '/api/sessions',
            '/api/dashboard/stats'
        ]
    });
});

app.listen(port, () => {
    console.log(`Backend server running on http://localhost:${port}`);
});
