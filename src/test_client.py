import socket
import struct
import time

# 1. Prepare the 29-byte payload
# < = Little Endian, 16s = 16 chars, d = double, i = int, c = char
data = struct.pack('<16sdic', b'order_001'.ljust(16, b'\0'), 145.0, 10, b'B')

# 2. Connect to your Mac's Unix Socket
sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
try:
    print(f"Connecting to /tmp/engine.sock... Data size: {len(data)} bytes")
    sock.connect('/tmp/engine.sock')
    
    print("Sending binary data...")
    sock.sendall(data)
    
    # CRITICAL: Wait 2 seconds so the server doesn't see a '0' (disconnect) immediately
    print("Data sent. Waiting for server to process...")
    time.sleep(2) 
    
finally:
    print("Closing connection.")
    sock.close()