CREATE DATABASE IF NOT EXISTS smart_parking_iot;
USE smart_parking_iot;

CREATE TABLE IF NOT EXISTS parking_slots (
    id INT AUTO_INCREMENT PRIMARY KEY,
    slot_code VARCHAR(10) UNIQUE NOT NULL,
    floor INT NOT NULL,
    status INT NOT NULL DEFAULT 0, -- 0: Empty, 1: Anomaly, 2: Occupied
    distance FLOAT,
    pir_status INT,
    last_updated TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS parking_events (
    id INT AUTO_INCREMENT PRIMARY KEY,
    slot_code VARCHAR(10) NOT NULL,
    event_type VARCHAR(20) NOT NULL, -- 'MASUK', 'KELUAR', 'ANOMALI'
    distance FLOAT,
    detail VARCHAR(255),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS parking_sessions (
    id INT AUTO_INCREMENT PRIMARY KEY,
    slot_code VARCHAR(10) NOT NULL,
    time_in TIMESTAMP NOT NULL,
    time_out TIMESTAMP NULL,
    duration_seconds INT NULL
);

CREATE TABLE IF NOT EXISTS daily_summary (
    id INT AUTO_INCREMENT PRIMARY KEY,
    summary_date DATE UNIQUE NOT NULL,
    total_cars INT DEFAULT 0,
    total_anomalies INT DEFAULT 0,
    avg_duration_seconds INT DEFAULT 0
);
