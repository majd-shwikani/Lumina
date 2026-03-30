import os
import subprocess
import re
import platform

# --- CONFIGURATION ---
# Updated to match your platformio.ini environment name
ENV_NAME = "esp32-s3-devkitc-1" 
# The path to your firmware.elf relative to the script
ELF_PATH = f".pio/build/{ENV_NAME}/firmware.elf"
# ---------------------

def get_tool_path():
    """Locates the xtensa-addr2line tool in the PlatformIO directory."""
    home = os.path.expanduser("~")
    
    # S3 uses the xtensa-esp32s3 toolchain
    tool_name = "xtensa-esp32s3-elf-addr2line"
    if platform.system() == "Windows":
        tool_name += ".exe"
    
    # Possible paths for ESP32-S3 toolchains in PlatformIO
    possible_paths = [
        os.path.join(home, ".platformio", "packages", "toolchain-xtensa-esp32s3", "bin", tool_name),
        os.path.join(home, ".platformio", "packages", "toolchain-xtensa-esp-s3", "bin", tool_name), # Alternate naming
    ]
    
    for full_path in possible_paths:
        if os.path.exists(full_path):
            return full_path
            
    return None

def decode_backtrace(backtrace_str):
    tool = get_tool_path()
    
    if not tool:
        print("Error: xtensa-esp32s3-elf-addr2line tool not found.")
        print("Ensure you have built the project in PlatformIO so the toolchain is downloaded.")
        return

    if not os.path.exists(ELF_PATH):
        print(f"Error: ELF file not found at {ELF_PATH}")
        print(f"Check that ENV_NAME in this script matches the [env:...] in your platformio.ini.")
        return

    # S3 addresses often use 0x42xxxxxx or 0x40xxxxxx depending on the memory region
    addresses = re.findall(r'0x[0-9a-fA-F]+', backtrace_str)
    
    # Filter for likely code addresses (typically start with 0x4)
    code_addresses = [addr for addr in addresses if addr.startswith("0x4")]
    
    if not code_addresses:
        print("No valid code addresses (starting with 0x4) found in the input.")
        return

    print(f"\n--- Decoding {len(code_addresses)} addresses for ESP32-S3 ---\n")
    
    cmd = [tool, "-pfiaC", "-e", ELF_PATH] + code_addresses
    
    try:
        result = subprocess.check_output(cmd, stderr=subprocess.STDOUT).decode('utf-8')
        print(result)
    except subprocess.CalledProcessError as e:
        print(f"Error running decoder: {e.output.decode('utf-8')}")

if __name__ == "__main__":
    print(f"ESP32-S3 Exception Decoder ({ENV_NAME})")
    print("Paste your Backtrace line and press Enter:")
    user_input = input("> ")
    decode_backtrace(user_input)