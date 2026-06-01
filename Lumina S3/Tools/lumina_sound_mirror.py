import subprocess
import sys
import time
import numpy as np

def install_dependencies():
    dependencies = ['pyserial', 'numpy', 'pyaudio']
    missing = []
    
    for dep in dependencies:
        try:
            __import__(dep)
        except ImportError:
            missing.append(dep)
    
    if missing:
        print(f"📦 Missing dependencies found: {', '.join(missing)}")
        print("Installing now... Please wait.")
        try:
            subprocess.check_call([sys.executable, "-m", "pip", "install", *missing])
            print("✅ Dependencies installed successfully!\n")
            time.sleep(1)
        except Exception as e:
            print(f"❌ Failed to install dependencies automatically: {e}")
            print(f"Please run manually: pip install {' '.join(missing)}")
            sys.exit(1)

# Ensure dependencies are present
install_dependencies()

import serial
import serial.tools.list_ports
import pyaudio

# =============================================================================
# CONFIGURATION
# =============================================================================
BAUD_RATE = 2000000
MAGIC_HEADER = b'LUMI'
CMD_AUDIO = b'\xAA'
CMD_HANDSHAKE = b'\xCC'

CHUNK_SIZE = 1024
FORMAT = pyaudio.paInt16
CHANNELS = 2
RATE = 44100

# Frequency band mapping (approximate ranges for 8 bands)
# [0-100, 100-250, 250-500, 500-1k, 1k-2k, 2k-4k, 4k-8k, 8k-16k]
BANDS = [
    (20, 150),
    (150, 400),
    (400, 800),
    (800, 1500),
    (1500, 3000),
    (3000, 5000),
    (5000, 10000),
    (10000, 20000)
]
# =============================================================================

def find_esp32_s3():
    ports = list(serial.tools.list_ports.comports())
    for p in ports:
        if p.vid == 0x303A and p.pid == 0x1001:
            print(f"🔍 Found ESP32-S3 by VID:PID on {p.device}")
            return p.device
    for p in ports:
        desc = p.description.upper()
        if "ESP32-S3" in desc or "USB-SERIAL" in desc:
            print(f"🔍 Found ESP32-S3 by description on {p.device} ({p.description})")
            return p.device
    return None

def get_led_count(ser):
    print("🤝 Performing handshake to get LED count...")
    # Clear buffers thoroughly
    time.sleep(2) # Wait for ESP32 to finish booting/printing
    ser.reset_input_buffer()
    ser.reset_output_buffer()
    
    ser.write(MAGIC_HEADER + CMD_HANDSHAKE)
    ser.flush()
    
    start_time = time.time()
    while ser.in_waiting < 2:
        if time.time() - start_time > 3.0:
            print("❌ Handshake timeout!")
            return None
        time.sleep(0.1)
    
    response = ser.read(2)
    led_count = (response[0] << 8) | response[1]
    # Sanity check
    if led_count <= 0 or led_count > 10000:
        print(f"⚠️ Received suspicious LED count: {led_count}. Retrying...")
        return get_led_count(ser)
    return led_count

def find_all_input_devices(p):
    """Return all devices with input channels."""
    devices = []
    for i in range(p.get_device_count()):
        dev = p.get_device_info_by_index(i)
        if dev['maxInputChannels'] > 0:
            devices.append(dev)
    return devices

def find_loopback_device(p):
    """Exhaustive search for a loopback or stereo mix device."""
    candidates = []
    for i in range(p.get_device_count()):
        dev = p.get_device_info_by_index(i)
        name = dev['name'].lower()
        if dev['maxInputChannels'] > 0:
            # Score candidates
            score = 0
            if "stereo mix" in name: score += 10
            if "loopback" in name: score += 10
            if "what u hear" in name: score += 10
            if "wave out" in name: score += 5
            
            if score > 0:
                candidates.append((score, dev))
    
    # Sort by score descending
    candidates.sort(key=lambda x: x[0], reverse=True)
    return [c[1] for c in candidates]

def open_audio_stream(p, device_info):
    """Attempt to open an audio stream with various configurations."""
    rates = [int(device_info['defaultSampleRate']), 44100, 48000]
    channels = [min(2, device_info['maxInputChannels']), 1]
    
    for rate in rates:
        for channel in channels:
            try:
                print(f"  Attempting: {device_info['name']} (Rate: {rate}, Ch: {channel}, API: {device_info['hostApi']})")
                stream = p.open(format=FORMAT,
                                channels=channel,
                                rate=rate,
                                input=True,
                                frames_per_buffer=CHUNK_SIZE,
                                input_device_index=device_info['index'])
                return stream, rate, channel
            except Exception as e:
                # print(f"  Failed: {e}")
                continue
    return None, None, None

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

    num_leds = get_led_count(ser)
    if not num_leds:
        print("⚠️ Could not determine LED count. Continuing anyway.")
    else:
        print(f"✨ Successfully detected {num_leds} LEDs.")

    p = pyaudio.PyAudio()
    
    # Priority 1: Likely loopback devices
    devices = find_loopback_device(p)
    
    # Priority 2: All other input devices
    all_inputs = find_all_input_devices(p)
    for d in all_inputs:
        if d not in devices:
            devices.append(d)

    if not devices:
        print("❌ Could not find any input device.")
        ser.close()
        return

    stream = None
    actual_rate = 0
    actual_channels = 0
    
    print(f"🔍 Found {len(devices)} potential input devices. Trying them...")
    for dev in devices:
        stream, actual_rate, actual_channels = open_audio_stream(p, dev)
        if stream:
            print(f"✅ Successfully opened: {dev['name']}")
            break
    
    if not stream:
        print("❌ Failed to open any audio device.")
        ser.close()
        p.terminate()
        return

    print(f"🚀 Starting Sound Mirroring...")
    print("Press Ctrl+C to stop")

    data_header = MAGIC_HEADER + CMD_AUDIO
    
    try:
        while True:
            # Read audio data
            try:
                raw_data = stream.read(CHUNK_SIZE, exception_on_overflow=False)
                audio_data = np.frombuffer(raw_data, dtype=np.int16)
                if actual_channels == 2:
                    audio_data = audio_data.reshape(-1, 2).mean(axis=1)
            except Exception as e:
                continue

            # Perform FFT
            fft_data = np.abs(np.fft.rfft(audio_data))
            fft_freqs = np.fft.rfftfreq(len(audio_data), 1.0/actual_rate)

            # Calculate bands
            band_values = []
            for low, high in BANDS:
                # Find indices for this frequency range
                idx = np.where((fft_freqs >= low) & (fft_freqs < high))[0]
                if len(idx) > 0:
                    val = np.mean(fft_data[idx])
                else:
                    val = 0
                band_values.append(val)

            # Normalize and scale to 0-255
            # Simple auto-gain normalization
            max_val = max(band_values) if max(band_values) > 0 else 1
            scaled_bands = [min(255, int(v * 255 / 1000000)) for v in band_values] # Adjust 1M based on volume
            
            # Send Packet
            ser.write(data_header + bytes(scaled_bands))
            
    except KeyboardInterrupt:
        print("\nStopping...")
    finally:
        stream.stop_stream()
        stream.close()
        p.terminate()
        ser.close()

if __name__ == "__main__":
    main()
