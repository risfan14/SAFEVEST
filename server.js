const express = require('express');
const http = require('http');
const path = require('path');
const { WebSocketServer, WebSocket } = require('ws');
const app = express();
const server = http.createServer(app);
const wss = new WebSocketServer({ server, path: '/ws' });
const PORT = Number(process.env.PORT || 3000);
let latest = null;
let lastSeen = 0;
app.use(express.json({ limit: '16kb' }));
app.use(express.static(__dirname));
function parsePayload(payload, rssi, snr) {
  const raw = String(payload || '').trim();
  if (raw === 'EMERGENCY') return { type: 'EMERGENCY', receivedAt: new Date().toISOString(), rssi: numberOrNull(rssi), snr: numberOrNull(snr) };
  const p = raw.split(',');
  if (p[0] !== 'DATA' || p.length < 7) return null;
  const gpsValid = p[1] !== 'NO_FIX' && p[2] !== 'NO_FIX';
  return {
    type: 'DATA',
    latitude: gpsValid ? numberOrNull(p[1]) : null,
    longitude: gpsValid ? numberOrNull(p[2]) : null,
    uv: numberOrNull(p[3]),
    temperature: numberOrNull(p[4]),
    bpm: numberOrNull(p[5]),
    beatAvg: numberOrNull(p[6]),
    rssi: numberOrNull(rssi),
    snr: numberOrNull(snr),
    raw,
    receivedAt: new Date().toISOString()
  };
}
function numberOrNull(value) { if (value === undefined || value === null || value === '' || value === 'NO_FIX') return null; const n = Number(value); return Number.isFinite(n) ? n : null; }
function broadcast(obj) { const data = JSON.stringify(obj); for (const client of wss.clients) if (client.readyState === WebSocket.OPEN) client.send(data); }
app.get('/api/health', (_req, res) => res.json({ ok: true, latest, receiverSeenRecently: Date.now() - lastSeen < 15000 }));
app.get('/api/latest', (_req, res) => res.json({ latest, receiverSeenRecently: Date.now() - lastSeen < 15000 }));
app.post('/api/data', (req, res) => {
  const packet = parsePayload(req.body && req.body.payload, req.body && req.body.rssi, req.body && req.body.snr);
  if (!packet) return res.status(400).json({ ok: false, error: 'Payload tidak dikenali. Format: DATA,lat,lon,uv,suhu,bpm,beatAvg atau EMERGENCY.' });
  lastSeen = Date.now();
  if (packet.type === 'DATA') latest = packet;
  broadcast(packet);
  res.json({ ok: true, type: packet.type });
});
wss.on('connection', socket => { socket.send(JSON.stringify({ type: 'status', connected: Date.now() - lastSeen < 15000, latest })); });
server.listen(PORT, '0.0.0.0', () => { console.log(`SAFEVEST dashboard berjalan: http://localhost:${PORT}`); console.log('ESP32 receiver harus POST ke http://IP-LAPTOP:3000/api/data'); });
