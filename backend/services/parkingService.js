const db = require('../config/database');

class ParkingService {
    async updateSlotStatus(slot) {
        try {
            // Cek slot apakah sudah ada
            const [rows] = await db.query('SELECT * FROM parking_slots WHERE slot_code = ?', [slot.name]);
            
            let previousState = 0;
            
            if (rows.length === 0) {
                // Buat slot baru jika belum ada
                await db.query(
                    'INSERT INTO parking_slots (slot_code, floor, status, distance, pir_status) VALUES (?, ?, ?, ?, ?)',
                    [slot.name, slot.floor || 1, slot.state, slot.dist, slot.mot]
                );
            } else {
                previousState = rows[0].status;
                // Update status terkini
                await db.query(
                    'UPDATE parking_slots SET status = ?, distance = ?, pir_status = ? WHERE slot_code = ?',
                    [slot.state, slot.dist, slot.mot, slot.name]
                );
            }

            // Deteksi Logika Event (Masuk/Keluar/Anomali)
            if (previousState !== 2 && slot.state === 2) {
                await this.logEvent(slot.name, 'MASUK', slot.dist, `Kendaraan masuk, jarak: ${slot.dist}cm`);
                await this.startSession(slot.name);
            } else if (previousState === 2 && slot.state === 0) {
                await this.logEvent(slot.name, 'KELUAR', slot.dist, `Kendaraan keluar`);
                await this.endSession(slot.name);
            } else if (previousState !== 1 && slot.state === 1) {
                await this.logEvent(slot.name, 'ANOMALI', slot.dist, `Anomali terdeteksi (Jarak: ${slot.dist}cm, PIR: ${slot.mot})`);
            }
        } catch (error) {
            console.error('Error updating slot status:', error);
        }
    }

    async logEvent(slotCode, eventType, distance, detail) {
        await db.query(
            'INSERT INTO parking_events (slot_code, event_type, distance, detail) VALUES (?, ?, ?, ?)',
            [slotCode, eventType, distance, detail]
        );
    }

    async startSession(slotCode) {
        await db.query(
            'INSERT INTO parking_sessions (slot_code, time_in) VALUES (?, CURRENT_TIMESTAMP)',
            [slotCode]
        );
    }

    async endSession(slotCode) {
        // Cari sesi parkir yang belum ditutup
        const [rows] = await db.query(
            'SELECT id, time_in FROM parking_sessions WHERE slot_code = ? AND time_out IS NULL ORDER BY time_in DESC LIMIT 1',
            [slotCode]
        );
        
        if (rows.length > 0) {
            const sessionId = rows[0].id;
            await db.query(
                'UPDATE parking_sessions SET time_out = CURRENT_TIMESTAMP, duration_seconds = TIMESTAMPDIFF(SECOND, time_in, CURRENT_TIMESTAMP) WHERE id = ?',
                [sessionId]
            );
        }
    }
}

module.exports = new ParkingService();
