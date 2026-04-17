import serial
import serial.tools.list_ports
import time
from datetime import datetime

# --- CONFIGURATION ---
BAUD_RATE = 115200
LOG_FILE = 'log.txt'

def find_serial_port():
    """Automatically finds the first available serial port."""
    ports = list(serial.tools.list_ports.comports())
    if not ports:
        return None
    
    # Returns the first port found (e.g., /dev/ttyUSB0 or COM5)
    return ports[0].device

# --- SCRIPT START ---
PORT_NAME = find_serial_port()

if not PORT_NAME:
    print("⚠️ No serial devices found. Please check your connection.")
    exit()

try:
    ser = serial.Serial(PORT_NAME, BAUD_RATE, timeout=0.1)
    log_file = open(LOG_FILE, 'a', encoding='utf-8')
    
    time.sleep(2)
    
    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    log_file.write(f"\n{'='*60}\n")
    log_file.write(f"Serial Log Started: {timestamp}\n")
    log_file.write(f"Auto-detected Port: {PORT_NAME}, Baud Rate: {BAUD_RATE}\n")
    log_file.write(f"{'='*60}\n\n")
    log_file.flush()
    
    print(f"--- Found device on {PORT_NAME} ---")
    print(f"--- Listening at {BAUD_RATE} baud. Press Ctrl+C to stop. ---")
    print(f"--- Logging to '{LOG_FILE}' ---\n")
    
    while True:
        if ser.in_waiting > 0:
            data = ser.read(ser.in_waiting)
            try:
                decoded_data = data.decode('utf-8', errors='ignore')
                print(decoded_data, end='')
                log_file.write(decoded_data)
                log_file.flush()
            except Exception as decode_error:
                print(f"\n⚠️ Decoding error: {decode_error}")
                
        time.sleep(0.001)

except serial.SerialException as e:
    print(f"\n⚠️ Error opening port {PORT_NAME}: {e}")
except KeyboardInterrupt:
    print("\n--- Script terminated by user. ---")
finally:
    if 'ser' in locals() and ser.is_open:
        ser.close()
    if 'log_file' in locals():
        log_file.close()
        print(f"--- Log saved to '{LOG_FILE}' ---")