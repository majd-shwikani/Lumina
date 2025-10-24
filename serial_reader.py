import serial
import time

# --- CONFIGURATION ---
# ⚠️ CHANGE 'COM3' to the port your Arduino/device is connected to
PORT_NAME = 'COM5'
BAUD_RATE = 115200

# --- SCRIPT START ---
try:
    # Open the serial port connection
    # timeout=0.1 means the script won't hang forever waiting for data
    ser = serial.Serial(PORT_NAME, BAUD_RATE, timeout=0.1)
    
    # Wait a moment for the connection to fully establish
    time.sleep(2) 
    
    print(f"--- Listening on {PORT_NAME} at {BAUD_RATE} baud. Press Ctrl+C to stop. ---\n")

    while True:
        # Check for data in the serial buffer
        if ser.in_waiting > 0:
            # Read all available bytes at once (fastest way for throughput)
            data = ser.read(ser.in_waiting)
            
            # Decode the bytes to a readable string and print without adding newlines
            # 'ignore' handles any unprintable or malformed characters
            print(data.decode('utf-8', errors='ignore'), end='')
            
        # Add a tiny delay to yield CPU resources
        time.sleep(0.001) 

except serial.SerialException as e:
    print(f"\n⚠️ Error opening port {PORT_NAME}: {e}")
    print("Please check if the port name is correct and if the device is plugged in.")
except KeyboardInterrupt:
    print("\n--- Script terminated by user. ---")
finally:
    # Safely close the port if it was opened
    if 'ser' in locals() and ser.is_open:
        ser.close()