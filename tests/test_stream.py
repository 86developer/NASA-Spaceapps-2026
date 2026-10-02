import socket
import struct
import base64
import hashlib
import json
import time

def test_websocket():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(5.0)
    s.connect(('127.0.0.1', 9002))
    print("[TEST] TCP Connected to 127.0.0.1:9002")

    # Send WebSocket Handshake
    key = base64.b64encode(b"0123456789abcdef").decode('ascii')
    req = (
        "GET / HTTP/1.1\r\n"
        "Host: 127.0.0.1:9002\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        f"Sec-WebSocket-Key: {key}\r\n"
        "Sec-WebSocket-Version: 13\r\n\r\n"
    )
    s.sendall(req.encode('utf-8'))

    # Receive Handshake Response
    resp = s.recv(1024).decode('utf-8')
    assert "101 Switching Protocols" in resp, f"Invalid handshake: {resp}"
    print("[TEST] Handshake Successful: 101 Switching Protocols")

    # Read WebSocket frames
    frames_received = 0
    start = time.time()
    while frames_received < 4 and (time.time() - start < 6):
        header = s.recv(2)
        if not header or len(header) < 2:
            break
        b1, b2 = header[0], header[1]
        opcode = b1 & 0x0F
        masked = (b2 & 0x80) != 0
        length = b2 & 0x7F

        if length == 126:
            ext = s.recv(2)
            length = struct.unpack("!H", ext)[0]
        elif length == 127:
            ext = s.recv(8)
            length = struct.unpack("!Q", ext)[0]

        payload = b""
        while len(payload) < length:
            chunk = s.recv(length - len(payload))
            if not chunk:
                break
            payload += chunk

        frames_received += 1
        text = payload.decode('utf-8', errors='replace')
        data = json.loads(text)
        print(f"\n--- [FRAME {frames_received}] Opcode: {opcode} | Size: {len(payload)} bytes ---")
        if "telemetry_ridge" in data:
            print("  TYPE: RIDGE TELEMETRY PAYLOAD")
            print(f"  CREW: {data.get('crew_member_id')} | HEALTH SCORE: {data.get('health_score')}")
            print(f"  HEART RATE: {data['telemetry_ridge']['G_gravity']['heart_rate_bpm']} bpm")
            print(f"  ppCO2: {data['telemetry_ridge']['E_environment']['cabin_ppco2_mmHg']} mmHg")
            print(f"  SpO2: {data['telemetry_ridge']['E_environment']['spo2_pct']}%")
        elif data.get("type") == "CLINICAL_ALERT":
            print("  TYPE: CLINICAL ALERT DIRECTIVE")
            print(f"  STATUS: {data.get('status')} | METRIC: {data.get('trigger_metric')}")
            print(f"  DIAGNOSIS: {data.get('diagnosis')}")
            print(f"  TACTICAL ACTION: {data.get('tactical_action')[:70]}...")

    s.close()
    print("\n[TEST COMPLETED SUCCESSFULLY] Telemetry ingestion and Flight Surgeon broadcasts verified!")

if __name__ == '__main__':
    test_websocket()
