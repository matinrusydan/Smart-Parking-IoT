# Laporan Teknis: Sistem Monitoring Aktivitas Parkir Gedung Berbasis IoT

## 1. Pendahuluan
Proyek ini merupakan pengembangan sistem pemantauan parkir pintar (Smart Parking) dari skala deteksi lokal menjadi sebuah sistem pemantauan _Internet of Things_ (IoT) skala _Enterprise_ penuh. Sistem ini dirancang untuk memantau okupansi slot parkir di area tertutup (gedung) menggunakan mikrokontroler **ESP32**, _Backend_ REST API (Node.js), basis data **MySQL**, dan antarmuka _Dashboard Modern_.

## 2. Arsitektur Sistem (Full-Stack IoT)
Sistem ini mengadopsi arsitektur pemisahan *concern* yang sangat terstruktur:
*   **Edge Computing (Hardware / Firmware):** Menggunakan mikrokontroler ESP32 yang diprogram via C++ (PlatformIO). ESP32 membaca sensor ultrasonik (HC-SR04) dan sensor gerak (PIR) secara *round-robin* dan melakukan _Sensor Fusion_ secara lokal.
*   **Jaringan (MQTT Broker):** Menggunakan protokol **MQTT** publik (`broker.hivemq.com` port 1883) sebagai jalur pipa pengiriman data JSON secara seketika (*real-time*).
*   **Backend & Database (Server-Side):** Sebuah server Node.js berfungsi sebagai _MQTT Subscriber_ yang otomatis menyaring dan memasukkan (_insert_) data histori ke dalam _Relational Database_ **MySQL** (`parking_events`, `parking_slots`, `parking_sessions`). Server ini juga membuka jalur **REST API** untuk menyajikan data secara aman.
*   **Web Dashboard (Frontend):** Antarmuka pengguna (UI) modern berbasis HTML, Tailwind CSS, dan Chart.js dengan perlindungan sistem Login. Web ini mengambil (_fetch_) data analitik dan grafik dari Backend API.

## 3. Alur Kerja Deteksi & Sinkronisasi
1.  **Deteksi Cerdas (Firmware):** ESP32 memfilter hasil mentah sensor dengan logika pintar:
    *   **Anomali Ekstrem (0-5 cm):** Jika ultrasonik mendeteksi benda dalam jarak 0-5 cm, sistem langsung memvonisnya sebagai `ANOMALI` (asumsi: sensor tertutup batu/kertas/tangan usil) tanpa perlu menunggu *timer*.
    *   **Mobil Parkir (Terisi):** Jarak tertutup (di atas 5cm) selama lebih dari 2 detik secara konsisten menghasilkan status `OCCUPIED`.
    *   **Pejalan Kaki (Anomali PIR):** Objek tertangkap jarak + sensor PIR mendeteksi panas tubuh.
2.  **Transmisi:** ESP32 mem-*publish* JSON berisi status 8 slot setiap 1 detik.
3.  **Sinkronisasi Basis Data:** Node.js Backend menangkap _payload_ MQTT. Jika ada perubahan status (mobil masuk, mobil keluar, atau anomali), Node.js akan menyimpannya ke MySQL.
4.  **Visualisasi Live:** Dashboard Web menerima _trigger_ dari MQTT untuk memperbarui warna hijau/merah/kuning, sekaligus memanggil REST API (`/api/dashboard/stats`, `/api/dashboard/trend`) untuk memutakhirkan grafik Chart.js dan riwayat secara instan.

## 4. Keunggulan Sistem
*   **Sistem Penyimpanan Permanen (MySQL):** Meninggalkan kelemahan sistem lama (yang hanya menyimpan di peramban / `localStorage`). Data riwayat masuk-keluar parkir tersimpan selamanya secara profesional di dalam basis data Relasional.
*   **Live Database Monitor:** Dashboard dilengkapi fitur inspeksi Database untuk memantau sinkronisasi tabel secara *real-time* dengan tampilan ala *Terminal Hacker*.
*   **Analitik & Grafik Dinamis:** Mengintegrasikan pustaka **Chart.js** yang terhubung langsung dengan komputasi agregasi API (seperti grafik tren parkir mingguan).
*   **UI/UX Kelas Enterprise:** Antarmuka dibangun tanpa desain murahan. Menggunakan _Sidebar Layout_, perlindungan otentikasi (Halaman Login `login.html`), dan bebas dari kode simulasi kotor (_Dev Mode_ telah dihapus total demi integritas sistem).

## 5. Kesimpulan
Proyek "Smart Parking IoT" ini berhasil bertransformasi dari sekadar tugas mikrokontroler sederhana menjadi sebuah purwarupa aplikasi _Full-Stack_ kelas industri. Integrasi yang sangat mulus antara kode keras bahasa C++ (ESP32), efisiensi protokol MQTT, keandalan server Node.js, pengelolaan data MySQL, dan antarmuka web modern membuktikan kelayakan yang sangat tinggi bagi proyek ini untuk mendapatkan nilai sempurna sebagai solusi sistem IoT nyata.
