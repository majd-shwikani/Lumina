#ifndef CONFIG_H
#define CONFIG_H

// WiFi credentials
#define WIFI_SSID "Espressif"
#define WIFI_PASSWORD "Centrino2121"

// Firebase configuration
#define FIREBASE_HOST "test-dbd7a-default-rtdb.firebaseio.com"
#define FIREBASE_SECRET "jBVd9wZ6n1dYTv4CSVmB96l66M8HspblZSmCN161"

// Time configuration
#define NTP_SERVER "pool.ntp.org"
#define TIMEZONE "EST5EDT"  // Change to your timezone
#define GMT_OFFSET_SEC -18000  // -5 hours in seconds
#define DAYLIGHT_OFFSET_SEC 3600

// NeoPixel configuration
#define LED_PIN 26
#define NUM_LEDS 180

#endif