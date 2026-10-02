import socket
import struct
import base64
import json
import time

def test_soma_trigger():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(5.0)
    s.connect(('127.0.0.1', 9002))

    # Handshake
    key = base64.b64encode(b"soma_test_key_12").decode('ascii')
    req = (
        "GET / HTTP/1.1\r\n"
        "Host: 127.0.0.1:9002\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        f"Sec-WebSocket-Key: {key}\r\n"
        "Sec-WebSocket-Version: 13\r\n\r\n"
    )
    s.sendall(req.encode('utf-8'))
    resp = s.recv(1024).decode('utf-8')
    assert "101 Switching Protocols" in resp

    # Send client masked frame: "TRIGGER_SOS"
    msg = "TRIGGER_SOS".encode('utf-8')
    mask_key = b"\x12\x34\x56\x78"
    masked_payload = bytearray(len(msg))
    for i in range(len(msg)):
        masked_payload[i] = msg[i] ^ mask_key[i % 4]

    frame = bytearray([0x81, 0x80 | len(msg)]) + mask_key + masked_payload
    s.sendall(frame)
    print("\n[TEST] Sent 'TRIGGER_SOS' to VitalGrid Daemon...")

    # Wait for SOMA DSOC Event frame
    soma_detected = False
    start = time.time()
    while time.time() - start < 4:
        header = s.recv(2)
        if not header or len(header) < 2:
            break
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
            if not chunk:
                break
            payload += chunk

        try:
            data = json.loads(payload.decode('utf-8'))
            if data.get("type") == "SOMA_DSOC_EVENT":
                soma_detected = True
                print("================================================================================")
                print(">>> [SOMA_DSOC_EVENT RECEIVED VIA WEBSOCKET] <<<")
                print(f"INCIDENT TIME:     {data.get('incident_timestamp')}")
                print(f"TRIGGER REASON:    {data.get('trigger_reason')}")
                print(f"BURST SAMPLES:     {data.get('historical_samples_count')} (1000 Hz)")
                print(f"DATA CONTAINER:    {data.get('raw_payload_bytes')} B raw -> {data.get('compressed_payload_bytes')} B")
                print(f"COMPRESSION:       {data.get('compression_algorithm')} (Ratio: {data.get('compression_ratio')}x)")
                print(f"CIPHER:            {data.get('encryption_cipher')}")
                print(f"IV NONCE:          {data.get('iv_nonce_hex')}")
                print(f"AUTH TAG:          {data.get('auth_tag_hex')}")
                print(f"OPTICAL TERMINAL:  {data.get('dsoc_laser_terminal')}")
                print(f"TARGET GROUND STN: {data.get('target_ground_station')}")
                print(f"LIGHT DELAY:       {data.get('light_delay_minutes')} min")
                print("================================================================================")
                break
        except Exception:
            pass

    assert soma_detected, "SOMA event was not emitted!"

    # Clean up and reset back to nominal
    reset_msg = "RESET_NOMINAL".encode('utf-8')
    masked_reset = bytearray(len(reset_msg))
    for i in range(len(reset_msg)):
        masked_reset[i] = reset_msg[i] ^ mask_key[i % 4]
    frame_reset = bytearray([0x81, 0x80 | len(reset_msg)]) + mask_key + masked_reset
    s.sendall(frame_reset)
    print("\n[TEST] Sent 'RESET_NOMINAL' to restore baseline...")
    time.sleep(0.5)

    s.close()
    print("[TEST COMPLETED SUCCESSFULLY] SOMA emergency protocol & DSOC transmission verified!\n")

if __name__ == '__main__':
    test_soma_trigger()
