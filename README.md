# SAFEVEST — Dashboard Real-time tanpa Database

Project ini menghubungkan paket LoRa dari ESP32 transmitter ke dashboard web melalui ESP32 receiver + Wi-Fi + backend Node.js. Tidak ada database: backend hanya menyimpan paket terakhir di RAM selama server berjalan.

## Isi folder
- `index.html`, `style.css`, `script.js`: dashboard web.
- `server.js`, `package.json`: backend HTTP + WebSocket.
- `ESP32_LoRa_Receiver_WiFi.ino`: firmware receiver LoRa yang meneruskan payload ke backend.

## Jalankan backend di laptop
1. Install Node.js versi LTS.
2. Buka Terminal → New Terminal, jalankan:
   ```bash
   npm install
   npm start
   ```
3. Buka `http://localhost:3000`. **Jangan pakai Live Server** untuk dashboard terintegrasi, karena backend melayani website sekaligus WebSocket.
4. Izinkan Node.js melewati Windows Firewall pada jaringan Private jika diminta.

## Cari IP laptop
- Windows: jalankan `ipconfig` di Command Prompt, cari `IPv4 Address` pada adaptor Wi-Fi yang sedang dipakai, misalnya `192.168.1.10`.
- Laptop dan ESP32 receiver harus terhubung ke jaringan Wi-Fi yang sama. Jika menggunakan hotspot HP, keduanya harus terhubung ke hotspot yang sama.

## Setel firmware receiver
Buka `ESP32_LoRa_Receiver_WiFi.ino` di Arduino IDE dan ubah:
- `WIFI_SSID` menjadi nama Wi-Fi.
- `WIFI_PASSWORD` menjadi kata sandi Wi-Fi.
- `SERVER_URL` menjadi `http://IP_LAPTOP:3000/api/data`, dengan IP laptop sebenarnya.

Contoh: `http://192.168.1.10:3000/api/data`.
Upload ke ESP32 receiver. Pastikan pin LoRa sesuai perangkat: SCK 18, MISO 19, MOSI 23, SS 5, RST 14, DIO0 26, frekuensi 433 MHz. OLED I2C 0x3C dan buzzer GPIO33 dipertahankan.

## Format paket LoRa
Firmware ini mendukung format yang sudah digunakan transmitter:
- `DATA,<latitude>,<longitude>,<uvIndex>,<suhu>,<BPM>,<beatAvg>`
- `DATA,NO_FIX,NO_FIX,<uvIndex>,<suhu>,<BPM>,<beatAvg>`
- `EMERGENCY`

Urutan field harus sama persis. Untuk `NO_FIX`, peta tidak akan dipindahkan ke koordinat palsu.

## Uji koneksi
- Buka `http://localhost:3000/api/health`; hasil `ok: true` menandakan backend hidup.
- Serial Monitor receiver pada 115200 baud menampilkan status Wi-Fi, paket LoRa, RSSI/SNR, dan hasil HTTP POST.
- Dashboard akan berubah dari OFFLINE menjadi ONLINE setelah data diterima. Tombol Perbarui data mengambil paket terakhir dari RAM.

## Catatan
- Tidak ada database; data terakhir hilang ketika backend dimatikan. Grafik hanya merekam data selama dashboard terbuka.
- Perangkat receiver dan laptop harus berada pada jaringan yang saling dapat mengakses. IP laptop bisa berubah, sehingga `SERVER_URL` mungkin perlu diperbarui.
- Pustaka peta, grafik, dan font dimuat dari CDN, jadi fitur tersebut memerlukan internet. Komunikasi receiver-ke-backend tetap melalui Wi-Fi lokal.

