import socket
import struct
import base64
import json
import time
import subprocess
import os
import signal

def run_integration_test():
    # 1. Start vitalgrid_daemon.exe
    proc = subprocess.Popen(["./vitalgrid_daemon.exe", "9005"], cwd="f:/VitalGrid", stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    time.sleep(1.2) # Allow daemon to bind port 9005

    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(5.0)
        s.connect(('127.0.0.1', 9005))
        print("[TEST] Connected to WebSocket at 127.0.0.1:9005")

        # Handshake
        key = base64.b64encode(b"0123456789abcdef").decode('ascii')
        req = (
            "GET / HTTP/1.1\r\n"
            "Host: 127.0.0.1:9005\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            f"Sec-WebSocket-Key: {key}\r\n"
            "Sec-WebSocket-Version: 13\r\n\r\n"
        )
        s.sendall(req.encode('utf-8'))
        resp = s.recv(1024).decode('utf-8')
        assert "101 Switching Protocols" in resp
        print("[TEST] Handshake OK")

        # Helper to send masked text frame
        def send_text(text):
            payload = text.encode('utf-8')
            mask_key = b"\xaa\xbb\xcc\xdd"
            masked = bytearray(len(payload))
            for i in range(len(payload)):
                masked[i] = payload[i] ^ mask_key[i % 4]
            frame = bytearray([0x81, 0x80 | len(payload)]) + mask_key + masked
            s.sendall(frame)

        # Helper to read frames
        def read_frame():
            header = s.recv(2)
            if not header or len(header) < 2: return None
            length = header[1] & 0x7F
            if length == 126:
                ext = s.recv(2)
                length = struct.unpack("!H", ext)[0]
            elif length == 127:
                ext = s.recv(8)
                length = struct.unpack("!Q", ext)[0]
            payload = b""
            while len(payload) < length:
                chunk = s.recv(length - len(payload))
                if not chunk: break
                payload += chunk
            return json.loads(payload.decode('utf-8'))

        # Drain initial frames
        for _ in range(2):
            f = read_frame()
            if f and "telemetry_ridge" in f:
                iso = f["telemetry_ridge"]["I_isolation"]
                print(f"[TEST] Nominal state: blink_rate={iso.get('blink_rate_per_min')}, microsleep={iso.get('microsleep_events')}")

        # Send peripheral JSON: severe blink drop
        print("[TEST] Sending peripheral telemetry: {'metric': 'blink_rate', 'value': 5}...")
        send_text(json.dumps({"metric": "blink_rate", "value": 5}))

        # Read until alert or ridge received
        blink_alert_detected = False
        start = time.time()
        while time.time() - start < 4:
            f = read_frame()
            if not f: continue
            if f.get("type") == "CLINICAL_ALERT" and f.get("trigger_metric") == "blink_rate_per_min":
                print(f"[TEST PASS] Received Blink Rate WARNING alert: {f.get('diagnosis')}")
                blink_alert_detected = True
                break

        assert blink_alert_detected, "Blink rate warning alert not emitted!"

        # Send peripheral JSON: microsleep event
        print("[TEST] Sending peripheral telemetry: {'metric': 'microsleep', 'value': 1}...")
        send_text(json.dumps({"metric": "microsleep", "value": 1}))

        microsleep_alert_detected = False
        start = time.time()
        while time.time() - start < 4:
            f = read_frame()
            if not f: continue
            if f.get("type") == "CLINICAL_ALERT" and f.get("trigger_metric") == "microsleep_events":
                print(f"[TEST PASS] Received Microsleep WARNING alert: {f.get('diagnosis')}")
                microsleep_alert_detected = True
                break

        assert microsleep_alert_detected, "Microsleep warning alert not emitted!"

        s.close()
        print("\n>>> ALL PERIPHERAL INTEGRATION TESTS PASSED PERFECTLY! <<<")

    finally:
        proc.terminate()
        try:
            proc.wait(timeout=2)
        except Exception:
            proc.kill()

if __name__ == "__main__":
    run_integration_test()
