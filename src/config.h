#ifndef CONFIG_H
#define CONFIG_H

// WiFi credentials
//#define WIFI_SSID "Espressif"
//#define WIFI_PASSWORD "Centrino2121"

// Firebase configuration
#define FIREBASE_HOST "test-dbd7a-default-rtdb.firebaseio.com"
#define FIREBASE_SECRET "jBVd9wZ6n1dYTv4CSVmB96l66M8HspblZSmCN161"

//#define DEVICE_ID "Majd_leds"

// Time configuration - Madrid
#define NTP_SERVER "pool.ntp.org"

// Option 1: Madrid Summer Time (CEST - Central European Summer Time)
// Last Sunday in March to last Sunday in October
#define TIMEZONE_SUMMER "CEST-2"  // UTC+2
#define GMT_OFFSET_SEC_SUMMER 7200    // +2 hours in seconds
#define DAYLIGHT_OFFSET_SEC_SUMMER 0

// Option 2: Madrid Winter Time (CET - Central European Time)  
// Last Sunday in October to last Sunday in March
#define TIMEZONE_WINTER "CET-1"   // UTC+1
#define GMT_OFFSET_SEC_WINTER 3600    // +1 hour in seconds
#define DAYLIGHT_OFFSET_SEC_WINTER 0

// Default configuration (you can change this based on current season)
#define TIMEZONE TIMEZONE_SUMMER
#define GMT_OFFSET_SEC GMT_OFFSET_SEC_SUMMER
#define DAYLIGHT_OFFSET_SEC DAYLIGHT_OFFSET_SEC_SUMMER

// NeoPixel configuration
#define LED_PIN 26
//#define NUM_LEDS 180

#endif

// Configuration data structure
struct ConfigData {
    char wifiSSID[32];
    char wifiPassword[64];
    char deviceID[32];
    int numLeds;
};

// Declare global configData variable (extern means it's defined elsewhere)
extern ConfigData configData;
