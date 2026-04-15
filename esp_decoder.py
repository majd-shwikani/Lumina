import os
import subprocess
import re
import glob

def find_elf_file():
    # Searches for any .elf file in the PlatformIO build directory
    paths = glob.glob(".pio/build/*/firmware.elf")
    if paths:
        return paths[0]
    return None

def get_addr2line_path(elf_path):
    # Determine toolchain based on the chip target in the path name
    if "c3" in elf_path.lower() or "c6" in elf_path.lower():
        return "riscv32-esp-elf-addr2line"
    elif "s3" in elf_path.lower():
        return "xtensa-esp32s3-elf-addr2line"
    else:
        return "xtensa-esp32-elf-addr2line"

def decode_backtrace(backtrace_str):
    elf_path = find_elf_file()
    if not elf_path:
        print("Error: Could not find firmware.elf. Ensure you are in the project root and have built the project.")
        return

    addr2line = get_addr2line_path(elf_path)
    
    # Extract addresses (0x40...) from the input string
    addresses = re.findall(r"0x[0-9a-fA-F]+", backtrace_str)
    
    # Filter: Backtrace usually pairs PC:SP. We only want the PC (even-indexed or first of pair)
    # Most decoders just try to resolve every hex address found.
    print(f"\nDecoding for: {elf_path}\n" + "-"*50)
    
    for addr in addresses:
        try:
            # Run addr2line command
            result = subprocess.check_output(
                [addr2line, "-pfiaC", "-e", elf_path, addr],
                stderr=subprocess.STDOUT,
                text=True
            )
            print(f"{addr}: {result.strip()}")
        except FileNotFoundError:
            print(f"Error: {addr2line} not found in PATH. Ensure ESP-IDF/PlatformIO tools are installed.")
            break
        except Exception as e:
            print(f"Failed to decode {addr}: {e}")

if __name__ == "__main__":
    print("ESP32 Universal Exception Decoder")
    user_input = input("Paste Backtrace line: ")
    decode_backtrace(user_input)