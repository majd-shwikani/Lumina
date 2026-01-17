import serial
import time
from datetime import datetime

# --- CONFIGURATION ---
# ⚠️ CHANGE 'COM3' to the port your Arduino/device is connected to
PORT_NAME = 'COM5'
BAUD_RATE = 115200
LOG_FILE = 'log.txt'

# --- SCRIPT START ---
try:
    # Open the serial port connection
    # timeout=0.1 means the script won't hang forever waiting for data
    ser = serial.Serial(PORT_NAME, BAUD_RATE, timeout=0.1)
    
    # Open log file in append mode
    log_file = open(LOG_FILE, 'a', encoding='utf-8')
    
    # Wait a moment for the connection to fully establish
    time.sleep(2)
    
    # Write header to log file
    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    log_file.write(f"\n{'='*60}\n")
    log_file.write(f"Serial Log Started: {timestamp}\n")
    log_file.write(f"Port: {PORT_NAME}, Baud Rate: {BAUD_RATE}\n")
    log_file.write(f"{'='*60}\n\n")
    log_file.flush()  # Ensure header is written immediately
    
    print(f"--- Listening on {PORT_NAME} at {BAUD_RATE} baud. Press Ctrl+C to stop. ---")
    print(f"--- Logging to '{LOG_FILE}' ---\n")
    
    while True:
        # Check for data in the serial buffer
        if ser.in_waiting > 0:
            # Read all available bytes at once (fastest way for throughput)
            data = ser.read(ser.in_waiting)
            
            try:
                # Decode the bytes to a readable string
                decoded_data = data.decode('utf-8', errors='ignore')
                
                # Print to console without adding newlines
                print(decoded_data, end='')
                
                # Write to log file
                log_file.write(decoded_data)
                log_file.flush()  # Ensure data is written immediately
                
            except Exception as decode_error:
                print(f"\n⚠️ Decoding error: {decode_error}")
                log_file.write(f"\n[ERROR decoding data at {datetime.now().strftime('%H:%M:%S')}]\n")
                log_file.flush()
                
        # Add a tiny delay to yield CPU resources
        time.sleep(0.001)

except serial.SerialException as e:
    error_msg = f"\n⚠️ Error opening port {PORT_NAME}: {e}"
    print(error_msg)
    print("Please check if the port name is correct and if the device is plugged in.")
    
    if 'log_file' in locals():
        log_file.write(f"\n[ERROR: {error_msg}]\n")
        log_file.flush()
        
except KeyboardInterrupt:
    print("\n--- Script terminated by user. ---")
    
    if 'log_file' in locals():
        timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        log_file.write(f"\n{'='*60}\n")
        log_file.write(f"Serial Log Ended: {timestamp}\n")
        log_file.write(f"{'='*60}\n\n")
        log_file.flush()
        
finally:
    # Safely close the port if it was opened
    if 'ser' in locals() and ser.is_open:
        ser.close()
    
    # Safely close the log file
    if 'log_file' in locals():
        log_file.close()
        print(f"--- Log saved to '{LOG_FILE}' ---")