import subprocess
import os
import sys
import re
import csv

# --- Universal Detection Logic ---

def get_project_root():
    """Finds the PlatformIO project root starting from the script's location."""
    current = os.path.dirname(os.path.abspath(__file__))
    while current != os.path.dirname(current):
        if os.path.exists(os.path.join(current, "platformio.ini")):
            return current
        current = os.path.dirname(current)
    return os.path.dirname(os.path.abspath(__file__)) # Fallback

PROJECT_ROOT = get_project_root()
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))

def get_coredump_info():
    """Parses partitions.csv to find the coredump partition address and size."""
    part_file = os.path.join(PROJECT_ROOT, "partitions.csv")
    if os.path.exists(part_file):
        with open(part_file, 'r') as f:
            for line in f:
                if 'coredump' in line.lower() and not line.startswith('#'):
                    parts = [p.strip() for p in line.split(',')]
                    # Format: Name, Type, SubType, Offset, Size
                    if len(parts) >= 5:
                        return parts[3], parts[4]
    return "0xFF0000", "0x10000" # Default fallback

def find_firmware_elf():
    """Searches for the compiled firmware.elf file in the .pio directory."""
    pio_dir = os.path.join(PROJECT_ROOT, ".pio", "build")
    if os.path.exists(pio_dir):
        for root, dirs, files in os.walk(pio_dir):
            if "firmware.elf" in files:
                return os.path.join(root, "firmware.elf")
    return None

def find_gdb_path(elf_path):
    """Finds the GDB executable in the .platformio/packages directory matching the chip."""
    if not elf_path:
        return "gdb"
    
    # Detect chip from elf_path (e.g., .../.pio/build/esp32-s3-devkitc-1/firmware.elf)
    chip = "esp32" # default
    if "esp32s3" in elf_path.lower() or "esp32-s3" in elf_path.lower():
        chip = "esp32s3"
    elif "esp32s2" in elf_path.lower() or "esp32-s2" in elf_path.lower():
        chip = "esp32s2"
    elif "esp32c3" in elf_path.lower() or "esp32-c3" in elf_path.lower():
        chip = "esp32c3"
    elif "esp8266" in elf_path.lower():
        chip = "lx106"

    pkg_dir = os.path.expanduser("~/.platformio/packages")
    if os.path.exists(pkg_dir):
        # Prefer specific match
        for root, dirs, files in os.walk(pkg_dir):
            for f in files:
                if f.endswith("-gdb.exe") or f == "gdb.exe":
                    if chip in f.replace("-", ""):
                        return os.path.join(root, f)
        
        # Fallback to any xtensa-*-elf-gdb if it's an esp32 variant
        if "esp32" in chip:
            for root, dirs, files in os.walk(pkg_dir):
                for f in files:
                    if "xtensa-esp32" in f and f.endswith("-gdb.exe"):
                        return os.path.join(root, f)

    return "gdb" # Fallback to PATH

def get_serial_port():
    """Attempts to find the best serial port."""
    import serial.tools.list_ports
    ports = list(serial.tools.list_ports.comports())
    if not ports:
        return "COM6" # Default fallback
    
    # Heuristic: Prefer ports with 'USB' or 'UART' in the name
    for p in ports:
        if 'USB' in p.description or 'UART' in p.description:
            return p.device
    return ports[0].device

# --- Initialization ---

OFFSET, SIZE = get_coredump_info()
ELF_FILE = find_firmware_elf()
GDB_PATH = find_gdb_path(ELF_FILE)
OUTPUT_FILE = os.path.join(SCRIPT_DIR, "coredump.txt")
TEMP_BIN = os.path.join(SCRIPT_DIR, "coredump_temp.bin")

def run_command(cmd, description):
    print(f"--- {description} ---")
    env = os.environ.copy()
    env["PYTHONIOENCODING"] = "utf-8"
    try:
        result = subprocess.run(cmd, shell=True, check=True, capture_output=True, text=True, encoding='utf-8', env=env)
        return result.stdout
    except subprocess.CalledProcessError as e:
        print(f"Error during {description}: {e.stderr}")
        return None

def main():
    if not ELF_FILE:
        print(f"Error: firmware.elf not found in {PROJECT_ROOT}")
        print("Please run 'pio run' first from the project root.")
        return

    port = get_serial_port()
    print(f"Project Root: {PROJECT_ROOT}")
    print(f"Detected Port: {port}")
    print(f"Coredump Partition: {OFFSET} (Size: {SIZE})")
    print(f"GDB Tool: {GDB_PATH}")

    if "--clear-only" in sys.argv:
        clear_cmd = f"python -m esptool --port {port} erase_region {OFFSET} {SIZE}"
        run_command(clear_cmd, "Clearing Flash")
        return

    # 1. Read flash using esptool
    print(f"Reading coredump from {port}...")
    read_cmd = f"python -m esptool --port {port} read_flash {OFFSET} {SIZE} \"{TEMP_BIN}\""
    run_command(read_cmd, "Reading Flash")

    if not os.path.exists(TEMP_BIN):
        print("Failed to extract coredump binary from flash.")
        return

    # Check if empty
    is_empty = False
    with open(TEMP_BIN, "rb") as f:
        data = f.read()
        if all(b == 0xFF for b in data):
            is_empty = True

    if is_empty:
        print("\n[INFO] No crash data found (the partition is empty).")
        if os.path.exists(TEMP_BIN): os.remove(TEMP_BIN)
        return

    # 2. Analyze
    print("Analyzing coredump...")
    analyze_cmd = (
        f"python -m esp_coredump info_corefile -t raw -c \"{TEMP_BIN}\" "
        f"--gdb \"{GDB_PATH}\" \"{ELF_FILE}\""
    )
    
    analysis_output = run_command(analyze_cmd, "Analyzing Coredump")

    if analysis_output:
        with open(OUTPUT_FILE, "w", encoding="utf-8") as f:
            f.write(analysis_output)
        print(f"Success! Full report saved to {OUTPUT_FILE}")
        
        clear = input("\nWould you like to CLEAR the coredump from the ESP32 flash now? (y/n): ")
        if clear.lower() == 'y':
            clear_cmd = f"python -m esptool --port {port} erase_region {OFFSET} {SIZE}"
            run_command(clear_cmd, "Clearing Flash")
            print("Done. The next crash will be fresh.")
    
    if os.path.exists(TEMP_BIN):
        os.remove(TEMP_BIN)

if __name__ == "__main__":
    main()
