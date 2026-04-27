import subprocess
import sys
import time

def install_dependencies():
    dependencies = ['mss', 'pyserial', 'numpy']
    missing = []
    
    # Check for missing dependencies
    for dep in dependencies:
        try:
            if dep == 'pyserial':
                import serial
            else:
                __import__(dep)
        except ImportError:
            missing.append(dep)
    
    if missing:
        print(f"📦 Missing dependencies found: {', '.join(missing)}")
        print("Installing now... Please wait.")
        try:
            subprocess.check_call([sys.executable, "-m", "pip", "install", *missing])
            print("✅ Dependencies installed successfully!\n")
            # Give the system a moment to register the new packages
            time.sleep(1)
        except Exception as e:
            print(f"❌ Failed to install dependencies automatically: {e}")
            print(f"Please run manually: pip install {' '.join(missing)}")
            sys.exit(1)

# Ensure dependencies are present before importing them
install_dependencies()

import mss
import serial
import serial.tools.list_ports
import numpy as np

# =============================================================================
# CONFIGURATION
# =============================================================================
BAUD_RATE = 2000000  # High speed for USB OTG (matched to S3 capabilities)
MAGIC_HEADER = b'LUMI'
CMD_DATA = b'\xBB'
CMD_HANDSHAKE = b'\xCC'
FPS_TARGET = 60
# =============================================================================

def find_esp32_s3():
    ports = list(serial.tools.list_ports.comports())
    
    # 1. First pass: Specific ESP32-S3 VID:PID (303A:1001)
    for p in ports:
        if p.vid == 0x303A and p.pid == 0x1001:
            print(f"🔍 Found ESP32-S3 by VID:PID on {p.device}")
            return p.device
            
    # 2. Second pass: String matching in description
    for p in ports:
        desc = p.description.upper()
        if "ESP32-S3" in desc or "USB-SERIAL" in desc:
            print(f"🔍 Found ESP32-S3 by description on {p.device} ({p.description})")
            return p.device
            
    return None

def get_led_count(ser):
    print("🤝 Performing handshake to get LED count...")
    # Clear buffers
    ser.reset_input_buffer()
    ser.reset_output_buffer()
    
    # Send handshake request: MAGIC + CMD
    ser.write(MAGIC_HEADER + CMD_HANDSHAKE)
    ser.flush()
    
    # Wait for response (2 bytes)
    start_time = time.time()
    while ser.in_waiting < 2:
        if time.time() - start_time > 2.0:
            print("❌ Handshake timeout! Make sure Screen Mirror Mode is ENABLED in Firebase/MQTT.")
            return None
        time.sleep(0.1)
    
    response = ser.read(2)
    led_count = (response[0] << 8) | response[1]
    return led_count

def main():
    port = find_esp32_s3()
    if not port:
        print("❌ Could not find ESP32-S3. Please specify COM port manually.")
        port = input("Enter COM port (e.g. COM3): ")
    
    try:
        ser = serial.Serial(port, BAUD_RATE, timeout=0.1)
        print(f"✅ Connected to {port} at {BAUD_RATE} baud")
    except Exception as e:
        print(f"❌ Failed to connect: {e}")
        return

    # Dynamic LED count detection
    num_leds = get_led_count(ser)
    if not num_leds or num_leds == 0:
        print("❌ Could not determine LED count. Falling back to 100.")
        num_leds = 100
    else:
        print(f"✨ Successfully detected {num_leds} LEDs from Lumina system.")

    print(f"🚀 Starting Screen Mirroring...")
    print("Press Ctrl+C to stop")

    with mss.mss() as sct:
        # Use the primary monitor
        monitor = sct.monitors[1]
        width = monitor["width"]
        height = monitor["height"]
        
        # Sampling Strategy:
        # Sample across the middle, but also include some average of top and bottom
        # for more representative colors.
        sample_y_mid = height // 2
        sample_y_top = height // 4
        sample_y_bot = (3 * height) // 4
        
        sample_x = np.linspace(0, width - 1, num_leds, dtype=int)

        frame_time = 1.0 / FPS_TARGET
        data_header = MAGIC_HEADER + CMD_DATA
        
        try:
            while True:
                start_time = time.time()
                
                # Capture screen
                img = sct.grab(monitor)
                img_np = np.array(img) # BGRA
                
                # Sample 3 lines and average them for better stability
                line_mid = img_np[sample_y_mid, sample_x]
                line_top = img_np[sample_y_top, sample_x]
                line_bot = img_np[sample_y_bot, sample_x]
                
                # Weighted average (Middle has more weight)
                pixels = (line_mid.astype(np.uint16) * 2 + line_top + line_bot) // 4
                
                # Convert to RGB bytes
                rgb_data = pixels[:, [2, 1, 0]].flatten().astype(np.uint8).tobytes()
                
                # Send Packet
                ser.write(data_header + rgb_data)
                
                # Maintain FPS
                elapsed = time.time() - start_time
                sleep_time = frame_time - elapsed
                if sleep_time > 0:
                    time.sleep(sleep_time)
                else:
                    # If we're falling behind, clear output buffer to reduce latency
                    ser.reset_output_buffer()
                    
        except KeyboardInterrupt:
            print("\nStopping...")
        finally:
            ser.close()

if __name__ == "__main__":
    main()
