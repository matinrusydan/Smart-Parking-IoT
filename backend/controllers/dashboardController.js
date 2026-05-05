const db = require('../config/database');

class DashboardController {
    async getSlots(req, res) {
        try {
            const [rows] = await db.query('SELECT * FROM parking_slots ORDER BY slot_code ASC');
            res.json(rows);
        } catch (error) {
            res.status(500).json({ error: error.message });
        }
    }

    async getEvents(req, res) {
        try {
            const limit = req.query.limit ? parseInt(req.query.limit) : 50;
            const [rows] = await db.query('SELECT * FROM parking_events ORDER BY created_at DESC LIMIT ?', [limit]);
            res.json(rows);
        } catch (error) {
            res.status(500).json({ error: error.message });
        }
    }

    async getSessions(req, res) {
        try {
            const [rows] = await db.query('SELECT * FROM parking_sessions ORDER BY time_in DESC LIMIT 50');
            res.json(rows);
        } catch (error) {
            res.status(500).json({ error: error.message });
        }
    }

    async getDashboardStats(req, res) {
        try {
            // Dapatkan total mobil masuk hari ini
            const [masukRows] = await db.query(`
                SELECT COUNT(*) as total_cars 
                FROM parking_events 
                WHERE event_type = 'MASUK' AND DATE(created_at) = CURDATE()
            `);
            
            // Dapatkan total anomali hari ini
            const [anomalyRows] = await db.query(`
                SELECT COUNT(*) as total_anomalies 
                FROM parking_events 
                WHERE event_type = 'ANOMALI' AND DATE(created_at) = CURDATE()
            `);
            
            // Rata-rata durasi parkir hari ini
            const [durationRows] = await db.query(`
                SELECT AVG(duration_seconds) as avg_duration 
                FROM parking_sessions 
                WHERE time_out IS NOT NULL AND DATE(time_in) = CURDATE()
            `);
            
            // Hitung persentase Okupansi
            const [slotRows] = await db.query(`
                SELECT 
                    COUNT(*) as total_slots,
                    SUM(CASE WHEN status = 2 THEN 1 ELSE 0 END) as occupied_slots
                FROM parking_slots
            `);

            res.json({
                total_cars_today: masukRows[0].total_cars || 0,
                total_anomalies_today: anomalyRows[0].total_anomalies || 0,
                avg_duration_seconds: durationRows[0].avg_duration ? Math.round(durationRows[0].avg_duration) : 0,
                occupancy: {
                    total: slotRows[0].total_slots || 0,
                    occupied: slotRows[0].occupied_slots || 0,
                    percentage: slotRows[0].total_slots > 0 ? Math.round((slotRows[0].occupied_slots / slotRows[0].total_slots) * 100) : 0
                }
            });
        } catch (error) {
            res.status(500).json({ error: error.message });
        }
    }

    async getDatabaseStatus(req, res) {
        try {
            const [slots] = await db.query('SELECT COUNT(*) as count FROM parking_slots');
            const [events] = await db.query('SELECT COUNT(*) as count FROM parking_events');
            const [sessions] = await db.query('SELECT COUNT(*) as count FROM parking_sessions');
            const [summary] = await db.query('SELECT COUNT(*) as count FROM daily_summary');

            res.json({
                status: 'Connected',
                tables: {
                    parking_slots: slots[0].count,
                    parking_events: events[0].count,
                    parking_sessions: sessions[0].count,
                    daily_summary: summary[0].count
                }
            });
        } catch (error) {
            res.status(500).json({ error: error.message, status: 'Disconnected' });
        }
    }

    async getWeeklyTrend(req, res) {
        try {
            const [rows] = await db.query(`
                SELECT 
                    DATE_FORMAT(created_at, '%Y-%m-%d') as date_str,
                    COUNT(*) as total_cars
                FROM parking_events
                WHERE event_type = 'MASUK' 
                  AND created_at >= CURDATE() - INTERVAL 6 DAY
                GROUP BY date_str
                ORDER BY date_str ASC
            `);

            const trendData = [];
            for(let i=6; i>=0; i--) {
                const d = new Date();
                d.setDate(d.getDate() - i);
                
                // Format tanggal YYYY-MM-DD sesuai Timezone Lokal Server
                const localDateStr = new Date(d.getTime() - (d.getTimezoneOffset() * 60000)).toISOString().split('T')[0];
                const dayName = d.toLocaleDateString('id-ID', { weekday: 'short' });

                const found = rows.find(r => r.date_str === localDateStr);

                trendData.push({
                    day: dayName,
                    total: found ? found.total_cars : 0
                });
            }
            res.json(trendData);
        } catch (error) {
            res.status(500).json({ error: error.message });
        }
    }
}

module.exports = new DashboardController();
