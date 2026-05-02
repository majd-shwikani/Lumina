import pyaudio

p = pyaudio.PyAudio()
print(f"Host APIs: {p.get_host_api_count()}")
for i in range(p.get_host_api_count()):
    print(p.get_host_api_info_by_index(i))

print("\nDevices:")
for i in range(p.get_device_count()):
    dev = p.get_device_info_by_index(i)
    print(f"Index {i}: {dev['name']} (Inputs: {dev['maxInputChannels']}, API: {dev['hostApi']})")

try:
    print(f"\nDefault Output: {p.get_default_output_device_info()}")
except:
    print("\nNo default output found.")

p.terminate()
