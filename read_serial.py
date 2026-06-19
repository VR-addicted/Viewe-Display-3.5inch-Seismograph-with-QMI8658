import serial
import time
import sys

try:
    ser = serial.Serial('COM5', 115200, timeout=1)
    # Reset chip
    ser.dtr = False
    ser.rts = True
    time.sleep(0.1)
    ser.rts = False
    ser.dtr = True
    time.sleep(0.5)

    print("--- Capturing ESP32-S3 Serial Logs for 10 seconds ---")
    start_time = time.time()
    while time.time() - start_time < 10.0:
        if ser.in_waiting > 0:
            line = ser.readline()
            sys.stdout.write(line.decode('utf-8', errors='ignore'))
            sys.stdout.flush()
        else:
            time.sleep(0.01)
            
    ser.close()
    print("\n--- Done ---")
except Exception as e:
    print(f"Error: {e}")
