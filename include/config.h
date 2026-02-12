#ifndef CONFIG_H
#define CONFIG_H

// ============================================================================
// PIN DEFINITIONS
// ============================================================================

// NeoPixel LED strip
#define LED_PIN 7

// Button for configuration/reset
#define BUTTON_PIN 19

// Radar sensor (LD2410)
#define RADAR_RX_PIN 16
#define RADAR_TX_PIN 17
#define RADAR_OUTPUT 23

// Audio/Microphone pins
// I2S Configuration (for ICS43434 microphone)
#define I2S_WS_PIN   25    // Word select/LRCLK
#define I2S_SD_PIN   32    // Serial data/DOUT  
#define I2S_SCK_PIN  33    // Serial clock/BCLK
#define I2S_PORT     I2S_NUM_0

// Analog Configuration (for MAX9814 microphone)
#define ANALOG_MIC_PIN 27

// Firebase configuration
#define FIREBASE_HOST "test-dbd7a-default-rtdb.firebaseio.com"
#define FIREBASE_SECRET "jBVd9wZ6n1dYTv4CSVmB96l66M8HspblZSmCN161"

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
#define TIMEZONE TIMEZONE_WINTER
#define GMT_OFFSET_SEC GMT_OFFSET_SEC_WINTER
#define DAYLIGHT_OFFSET_SEC DAYLIGHT_OFFSET_SEC_WINTER

// Configuration data structure
struct ConfigData {
    char wifiSSID[32];
    char wifiPassword[64];
    char deviceID[32];
    int numLeds;
};

// Declare global configData variable (extern means it's defined elsewhere)
extern ConfigData configData;

#endif