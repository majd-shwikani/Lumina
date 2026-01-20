import os
import subprocess
import re
import platform

# --- CONFIGURATION ---
# Change this to your actual build folder name (e.g., 'esp32dev', 'lolin32', etc.)
ENV_NAME = "esp32dev" 
# The path to your firmware.elf relative to the script
ELF_PATH = f".pio/build/{ENV_NAME}/firmware.elf"
# ---------------------

def get_tool_path():
    """Locates the xtensa-addr2line tool in the PlatformIO directory."""
    home = os.path.expanduser("~")
    
    if platform.system() == "Windows":
        tool_relative = ".platformio/packages/toolchain-xtensa-esp32/bin/xtensa-esp32-elf-addr2line.exe"
    else:
        tool_relative = ".platformio/packages/toolchain-xtensa-esp32/bin/xtensa-esp32-elf-addr2line"
    
    full_path = os.path.join(home, tool_relative)
    
    if not os.path.exists(full_path):
        # Try alternate path for newer ESP-IDF versions
        tool_relative = tool_relative.replace("xtensa-esp32", "xtensa-esp32s3")
        full_path = os.path.join(home, tool_relative)
        
    return full_path

def decode_backtrace(backtrace_str):
    tool = get_tool_path()
    
    if not os.path.exists(tool):
        print(f"Error: Tool not found at {tool}")
        return

    if not os.path.exists(ELF_PATH):
        print(f"Error: ELF file not found at {ELF_PATH}")
        print("Make sure you run this script from your project root and have compiled the project.")
        return

    # Extract all hex addresses (0x40xxxxxx)
    addresses = re.findall(r'0x40[0-9a-fA-F]+', backtrace_str)
    
    if not addresses:
        print("No valid addresses found in the input.")
        return

    print(f"\n--- Decoding {len(addresses)} addresses ---\n")
    
    # Construct command: addr2line -pfiaC -e [ELF] [ADDR1] [ADDR2] ...
    cmd = [tool, "-pfiaC", "-e", ELF_PATH] + addresses
    
    try:
        result = subprocess.check_output(cmd, stderr=subprocess.STDOUT).decode('utf-8')
        print(result)
    except subprocess.CalledProcessError as e:
        print(f"Error running decoder: {e.output.decode('utf-8')}")

if __name__ == "__main__":
    print("ESP32 Exception Decoder (Python Wrapper)")
    print("Paste your Backtrace line (e.g., 0x40084451:0x3ffbef5c ...) and press Enter:")
    user_input = input("> ")
    decode_backtrace(user_input)