//test
#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <Adafruit_NeoPixel.h>
#include <ArduinoOTA.h>
#include <Wire.h>
#include <Adafruit_VEML7700.h>

// Provide the token generation process info.
#include <addons/TokenHelper.h>

// WiFi credentials
#define WIFI_SSID "Espressif"
#define WIFI_PASSWORD "Centrino2121"

// Firebase configuration
#define FIREBASE_HOST "test-dbd7a-default-rtdb.firebaseio.com"
#define FIREBASE_SECRET "jBVd9wZ6n1dYTv4CSVmB96l66M8HspblZSmCN161"

// NeoPixel configuration
#define LED_PIN 26
#define NUM_LEDS 180

// VEML7700 configuration
volatile float luxThreshold = 1.0;  // Default value, will be controlled via Firebase

// Global variables
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_VEML7700 veml = Adafruit_VEML7700();
FirebaseData fbdoStream; // used only for stream and callbacks
FirebaseData fbdoUpload; // used for setFloat, setBool and all write ops

FirebaseAuth auth;
FirebaseConfig config;

// Animation control variables
volatile int currentEffect = 0;
volatile uint32_t effectSpeed = 50;
volatile uint32_t effectColor = 0xFF0000;
volatile bool updateEffect = false;
volatile bool firebaseConnected = false;
volatile bool stripEnabled = true;  // Manual on/off control
volatile bool autoDarknessControl = true;  // Auto turn off in darkness
volatile bool turnedOffByDarkness = false;

// Sensor data
volatile float currentLux = 0;
volatile bool sensorAvailable = false;

// Function declarations
void connectToWiFi();
void setupFirebase();
void setupOTA();
void setupVEML7700();
void firebaseTask(void *parameter);
void ledTask(void *parameter);
void automationtask(void *parameter);
void sensorDataTask(void *parameter);
void handleFirebaseData();
void updateLEDs();
void streamCallback(FirebaseStream data);
void streamTimeoutCallback(bool timeout);
void createDefaultFirebaseData();
void updateSensorData();
bool shouldTurnOffDueToDarkness();

// Animation function declarations
void effectRainbow();              // 0: Rainbow cycle
void effectMeteorShower();         // 1: Meteor shower with trails
void effectDigitalRain();          // 2: Matrix-style digital rain
void effectPulsingSpheres();       // 3: Colorful pulsing spheres
void effectBinaryClock();          // 4: Binary number display
void effectVortex();               // 5: Swirling vortex pattern
void effectDNAHelix();             // 6: Double helix simulation
void effectAudioVisualizer();      // 7: Audio spectrum analyzer
void effectLavaLamp();             // 8: Lava lamp blobs
void effectRadarSweep();           // 9: Radar with blips
void effectQuantumParticles();     // 10: Quantum particle simulation
void effectNeuralNetwork();        // 11: Neural network firing
void effectGalaxySpin();           // 12: Rotating galaxy
void effectCrystalGrowth();        // 13: Growing crystals
void effectLightningStorm();       // 14: Lightning storm
void effectOceanDepth();           // 15: Deep ocean with creatures
void effectNorthernLights();       // 16: Aurora borealis
void effectTimeTunnel();           // 17: Time tunnel/wormhole
void effectCyberCity();            // 18: Cyberpunk cityscape
void effectSolarFlare();           // 19: Solar flare activity
void effectFireSimulation();       // 20: Realistic fire simulation
void effectSolidColor();

// Helper function declarations
uint32_t Wheel(byte WheelPos);
uint32_t HeatColor(uint8_t temperature);
uint8_t qsub8(uint8_t i, uint8_t j);
uint8_t qadd8(uint8_t i, uint8_t j);
uint8_t random8();
uint8_t random8(uint8_t lim);
uint8_t random8(uint8_t min, uint8_t max);
uint32_t colorBlend(uint32_t color1, uint32_t color2, uint8_t blend);
void setAllLeds(uint32_t color);

void setup() {
  Serial.begin(115200);
  
  // Initialize NeoPixel strip
  strip.begin();
  strip.show();
  strip.setBrightness(100);
  
  // Initialize I2C for VEML7700
  Wire.begin();
  
  // Connect to WiFi
  connectToWiFi();
  
  // Setup OTA
  setupOTA();
  
  // Setup VEML7700 sensor
  setupVEML7700();
  
  // Setup Firebase
  setupFirebase();
  
xTaskCreatePinnedToCore(
  firebaseTask,
  "FirebaseTask",
  15000,  // Increased from 10000
  NULL,
  1,
  NULL,
  0
);

xTaskCreatePinnedToCore(
  ledTask,
  "LEDTask",
  15000,  // Increased from 10000
  NULL,
  1,
  NULL,
  1
);

xTaskCreatePinnedToCore(
  sensorDataTask,
  "SensorDataTask",
  15000,   // Increased from 4000
  NULL,
  1,
  NULL,
  0
);

  xTaskCreatePinnedToCore(
  automationtask,           // Task function
  "AutomationTask",         // Task name
  4000,                     // Stack size
  NULL,                     // Parameters
  0,                        // Priority (low)
  NULL,    // Task handle (NEW - this was missing)
  0                         //
);

}



void loop() {
  // Empty - everything is handled by FreeRTOS tasks
  vTaskDelay(1000 / portTICK_PERIOD_MS);
}

void connectToWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
}

void setupOTA() {
  ArduinoOTA.setHostname("esp32-neopixel-controller");
  
  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH) {
        type = "sketch";
      } else {
        type = "filesystem";
      }
      Serial.println("Start updating " + type);
      strip.clear();
      strip.show();
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

  ArduinoOTA.begin();
  Serial.println("OTA Ready");
}

void setupVEML7700() {
  if (!veml.begin()) {
    Serial.println("VEML7700 sensor not found, continuing without light sensor");
    sensorAvailable = false;
    return;
  }
  
  sensorAvailable = true;
  veml.setGain(VEML7700_GAIN_1_8);
  veml.setIntegrationTime(VEML7700_IT_100MS);
  Serial.println("VEML7700 initialized");
}

void setupFirebase() {
  config.host = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_SECRET;
  
  // Optional, set AP reconnection
  config.timeout.serverResponse = 10 * 1000;
  
  fbdoStream.setResponseSize(2048);
  
  // Connect to Firebase
  Firebase.begin(&config, &auth);
  Firebase.reconnectNetwork(true);
  
  // Wait for Firebase connection
  Serial.println("Connecting to Firebase...");
  delay(1000);
  
  // Create default data structure
  //createDefaultFirebaseData();
  
  // Start stream listener for real-time updates
  if (!Firebase.RTDB.beginStream(&fbdoStream, "/")) {
    Serial.printf("Stream begin error: %s\n", fbdoStream.errorReason().c_str());
  }
  
  Firebase.RTDB.setStreamCallback(&fbdoStream, streamCallback, streamTimeoutCallback);
  Serial.println("Firebase initialized with stream and default data");
}

void firebaseTask(void *parameter) {
  for(;;) {
    if (WiFi.status() == WL_CONNECTED) {
      ArduinoOTA.handle();
      
      // Keep Firebase connection alive
      if (!Firebase.ready()) {
        Serial.println("Firebase not ready, reconnecting...");
        firebaseConnected = false;
        setupFirebase();
      } else {
        firebaseConnected = true;
      }
    } else {
      firebaseConnected = false;
      Serial.println("WiFi disconnected, reconnecting...");
      connectToWiFi();
    }
    
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

void ledTask(void *parameter) {
  for(;;) {
    updateLEDs();
    vTaskDelay(effectSpeed / portTICK_PERIOD_MS);
  }
}

void automationtask(void *parameter) {
  const TickType_t xDelay = 100 / portTICK_PERIOD_MS;

  for(;;) {
    if (sensorAvailable) {
      updateSensorData();

      if (autoDarknessControl && shouldTurnOffDueToDarkness()) {
        if (stripEnabled) {
          stripEnabled = false;
          turnedOffByDarkness = true;
          strip.clear();
          strip.show();
          Serial.println("Darkness detected - turning LEDs off automatically");
        }
      } 
      else if (autoDarknessControl && turnedOffByDarkness && !shouldTurnOffDueToDarkness()) {
        stripEnabled = true;
        turnedOffByDarkness = false;
        Serial.println("Light detected - turning LEDs back on automatically");
      }
    }

    vTaskDelay(xDelay);
  }
}


void sensorDataTask(void *parameter) {
  const TickType_t xDelay = 2000 / portTICK_PERIOD_MS; // 2 second delay
  
  for(;;) {
    if (firebaseConnected) {
      // Update all sensor data first
      if (sensorAvailable) {
        updateSensorData();
        
        // Send lux data to Firebase
        if (Firebase.RTDB.setFloat(&fbdoUpload, "/lux", currentLux)) {
          Serial.printf("Lux data sent: %.2f\n", currentLux);
        } else {
          Serial.printf("Failed to send lux data: %s\n", fbdoUpload.errorReason().c_str());
        }
      }
      
    } else {
      Serial.println("Firebase not connected, skipping sensor data send");
    }
    
    vTaskDelay(xDelay);
  }
}

void updateSensorData() {
  currentLux = veml.readLux();
}

bool shouldTurnOffDueToDarkness() {
  return currentLux < luxThreshold;  // Use the variable instead of the hardcoded value
}

void streamCallback(FirebaseStream data) {
  Serial.printf("Stream data path: %s, event: %s, type: %s, value: %s\n",
                data.streamPath().c_str(),
                data.dataType().c_str(),
                data.eventType().c_str(),
                data.stringData().c_str());

  // Handle effect changes
  if (data.dataPath() == "/effect") {
    currentEffect = data.intData();
    Serial.printf("Effect changed to: %d\n", currentEffect);
  }
  // Handle speed changes
  else if (data.dataPath() == "/speed") {
    effectSpeed = data.intData();
    Serial.printf("Speed changed to: %d\n", effectSpeed);
  }
  // Handle color changes
  else if (data.dataPath() == "/color") {
    String colorStr = data.stringData();
    if (colorStr.length() == 6) {
      effectColor = strtoul(colorStr.c_str(), NULL, 16);
      Serial.printf("Color changed to: %s\n", colorStr.c_str());
    }
  }
  else if (data.dataPath() == "/lux_threshold") {
  luxThreshold = data.floatData();
  Serial.printf("Lux threshold changed to: %.2f\n", luxThreshold);
}
else if (data.dataPath() == "/enabled") {
  bool newState = data.boolData();
  
  // Only allow turning on if not in darkness (when auto control is enabled)
  if (newState && autoDarknessControl && shouldTurnOffDueToDarkness()) {
    Serial.println("Cannot turn on LEDs - darkness detected");
    // Revert to false in Firebase to reflect actual state
    Firebase.RTDB.setBool(&fbdoUpload, "/enabled", false);
  } else {
    stripEnabled = newState;
    turnedOffByDarkness = false;  // Reset when user manually toggles
    Serial.printf("Strip %s\n", stripEnabled ? "enabled" : "disabled");
    if (!stripEnabled) {
      strip.clear();
      strip.show();
    }
  }
}
  // Handle auto darkness control
  else if (data.dataPath() == "/auto_darkness_control") {
    autoDarknessControl = data.boolData();
    Serial.printf("Auto darkness control %s\n", autoDarknessControl ? "enabled" : "disabled");
  }
}

void streamTimeoutCallback(bool timeout) {
  if (timeout) {
    Serial.println("Stream timeout, resuming...");
  }
  if (!fbdoStream.httpConnected()) {
    Serial.printf("Stream error: %s\n", fbdoStream.errorReason().c_str());
  }
}

void updateLEDs() {
  // Check if we should turn off due to darkness (auto control)
  if (autoDarknessControl && sensorAvailable && shouldTurnOffDueToDarkness()) {
    strip.clear();
    strip.show();
    return;
  }
  
  // Manual control check
  if (!stripEnabled) {
    strip.clear();
    strip.show();
    return;
  }
  
  // Run the selected animation effect
  switch(currentEffect) {
    case 0: effectRainbow(); break;
    case 1: effectMeteorShower(); break;
    case 2: effectDigitalRain(); break;
    case 3: effectPulsingSpheres(); break;
    case 4: effectBinaryClock(); break;
    case 5: effectVortex(); break;
    case 6: effectDNAHelix(); break;
    case 7: effectAudioVisualizer(); break;
    case 8: effectLavaLamp(); break;
    case 9: effectRadarSweep(); break;
    case 10: effectQuantumParticles(); break;
    case 11: effectNeuralNetwork(); break;
    case 12: effectGalaxySpin(); break;
    case 13: effectCrystalGrowth(); break;
    case 14: effectLightningStorm(); break;
    case 15: effectOceanDepth(); break;
    case 16: effectNorthernLights(); break;
    case 17: effectTimeTunnel(); break;
    case 18: effectCyberCity(); break;
    case 19: effectSolarFlare(); break;
    case 20: effectFireSimulation(); break;
    case 21: effectSolidColor(); break;  // NEW: Solid Color effect
    default: effectRainbow(); break;
  }
  strip.show();
}
// Effect 0: Rainbow cycle
void effectRainbow() {
  static uint16_t j = 0;
  for(int i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, Wheel((i + j) & 255));
  }
  j++;
  if(j >= 256) j = 0;
}

// Effect 1: Meteor shower
void effectMeteorShower() {
  static int meteors[3] = {-50, -100, -150};
  static uint32_t meteorColors[3] = {0xFF5500, 0x00AAFF, 0xAA00FF};
  static int meteorSpeeds[3] = {3, 2, 4};
  
  // Fade all pixels
  for(int i = 0; i < strip.numPixels(); i++) {
    uint32_t c = strip.getPixelColor(i);
    if(c > 0) {
      uint8_t r = (c >> 16) & 0xFF;
      uint8_t g = (c >> 8) & 0xFF;
      uint8_t b = c & 0xFF;
      
      r = r * 8 / 10;
      g = g * 8 / 10;
      b = b * 8 / 10;
      
      strip.setPixelColor(i, r, g, b);
    }
  }
  
  // Update meteors
  for(int m = 0; m < 3; m++) {
    meteors[m] += meteorSpeeds[m];
    
    // Draw meteor with tail
    for(int i = 0; i < 15; i++) {
      int pos = meteors[m] - i;
      if(pos >= 0 && pos < strip.numPixels()) {
        float intensity = 1.0 - (i * 0.07);
        uint32_t color = meteorColors[m];
        uint8_t r = ((color >> 16) & 0xFF) * intensity;
        uint8_t g = ((color >> 8) & 0xFF) * intensity;
        uint8_t b = (color & 0xFF) * intensity;
        strip.setPixelColor(pos, r, g, b);
      }
    }
    
    // Reset meteor if it goes off screen
    if(meteors[m] > strip.numPixels() + 20) {
      meteors[m] = -random(20, 100);
      meteorColors[m] = Wheel(random(256));
    }
  }
}

// Effect 2: Digital rain
void effectDigitalRain() {
  static uint8_t columns[16];
  static uint8_t columnHeights[16];
  static uint32_t lastDrop = 0;
  
  // Fade all pixels slightly
  for(int i = 0; i < strip.numPixels(); i++) {
    uint32_t c = strip.getPixelColor(i);
    if(c > 0) {
      uint8_t r = (c >> 16) & 0xFF;
      uint8_t g = (c >> 8) & 0xFF;
      uint8_t b = c & 0xFF;
      
      r = r * 95 / 100;
      g = g * 95 / 100;
      b = b * 95 / 100;
      
      strip.setPixelColor(i, r, g, b);
    } else {
      // Add some random sparkles in the background
      if(random(1000) < 2) {
        strip.setPixelColor(i, 0, 10, 0);
      }
    }
  }
  
  // Create new drops
  if(millis() - lastDrop > 150) {
    lastDrop = millis();
    int col = random(16);
    columns[col] = 1;
    columnHeights[col] = random(5, 12);
  }
  
  // Update and draw columns
  for(int col = 0; col < 16; col++) {
    if(columns[col] > 0) {
      for(int row = 0; row < columnHeights[col]; row++) {
        int pos = col + (row * 16);
        if(pos < strip.numPixels()) {
          float intensity = 1.0 - (row * 0.15);
          uint8_t brightness = 255 * intensity;
          // Head of the drop is white, tail is green
          if(row == 0) {
            strip.setPixelColor(pos, brightness, brightness, brightness);
          } else {
            strip.setPixelColor(pos, 0, brightness, 0);
          }
        }
      }
      columns[col]++;
      if(columns[col] > strip.numPixels() / 16 + columnHeights[col]) {
        columns[col] = 0;
      }
    }
  }
}

// Effect 3: Pulsing spheres
void effectPulsingSpheres() {
  static float spheres[3][3] = {{0.3, 0.2, 0.0}, {0.7, 0.5, 0.0}, {0.5, 0.8, 0.0}}; // x, phase, size
  static uint8_t sphereHues[3] = {0, 85, 170};
  
  // Clear with dark background
  strip.fill(strip.Color(5, 5, 10));
  
  // Update spheres
  for(int s = 0; s < 3; s++) {
    spheres[s][1] += 0.02 + (s * 0.01); // Phase
    spheres[s][2] = 0.1 + 0.15 * (sin(spheres[s][1]) + 1.0) / 2.0; // Pulsing size
    sphereHues[s] += 1; // Slowly change color
  }
  
  // Draw spheres
  for(int i = 0; i < strip.numPixels(); i++) {
    float pos = (float)i / strip.numPixels();
    
    for(int s = 0; s < 3; s++) {
      float dx = pos - spheres[s][0];
      float distance = abs(dx);
      if(distance < spheres[s][2]) {
        float intensity = 1.0 - (distance / spheres[s][2]);
        intensity = intensity * intensity; // Quadratic falloff
        
        uint32_t sphereColor = Wheel(sphereHues[s]);
        uint8_t r = ((sphereColor >> 16) & 0xFF) * intensity;
        uint8_t g = ((sphereColor >> 8) & 0xFF) * intensity;
        uint8_t b = (sphereColor & 0xFF) * intensity;
        
        // Blend with existing color
        uint32_t currentColor = strip.getPixelColor(i);
        uint8_t cr = (currentColor >> 16) & 0xFF;
        uint8_t cg = (currentColor >> 8) & 0xFF;
        uint8_t cb = currentColor & 0xFF;
        
        strip.setPixelColor(i, 
          cr + r > 255 ? 255 : cr + r, 
          cg + g > 255 ? 255 : cg + g, 
          cb + b > 255 ? 255 : cb + b
        );
      }
    }
  }
}

// Effect 4: Binary clock
void effectBinaryClock() {
  static uint32_t lastChange = 0;
  static uint8_t binaryValue = 0;
  static uint8_t counter = 0;
  
  if(millis() - lastChange > 300) {
    lastChange = millis();
    binaryValue = counter++;
    
    // Clear strip
    for(int i = 0; i < strip.numPixels(); i++) {
      // Create grid background
      if((i / 16) % 2 == (i % 2)) {
        strip.setPixelColor(i, 2, 5, 2);
      } else {
        strip.setPixelColor(i, 1, 3, 1);
      }
    }
    
    // Display binary value as bars
    for(int bit = 0; bit < 8; bit++) {
      if(binaryValue & (1 << bit)) {
        int startPos = bit * (strip.numPixels() / 8);
        int barHeight = (bit + 1) * 2;
        
        for(int j = 0; j < barHeight && j < 11; j++) {
          int pos = startPos + j * 16;
          if(pos < strip.numPixels()) {
            uint8_t intensity = 255 - (j * 20);
            strip.setPixelColor(pos, 0, intensity, 0);
          }
        }
      }
    }
  }
}

// Effect 5: Vortex
void effectVortex() {
  static float angle = 0;
  static float twist = 0;
  
  for(int i = 0; i < strip.numPixels(); i++) {
    float pos = (float)i / strip.numPixels();
    
    // Create vortex pattern
    float vortexAngle = angle + pos * 15.0 + twist;
    float radius = pos * 8.0;
    
    float wave1 = sin(vortexAngle);
    float wave2 = cos(vortexAngle + radius);
    
    float pattern = (wave1 + wave2 + 2.0) / 4.0; // Normalize to 0-1
    
    // Color based on position and angle
    int hueValue = (int)(i * 3 + angle * 50) % 256;
    uint8_t hue = (uint8_t)hueValue;
    uint32_t color = Wheel(hue);
    
    uint8_t r = ((color >> 16) & 0xFF) * pattern;
    uint8_t g = ((color >> 8) & 0xFF) * pattern;
    uint8_t b = (color & 0xFF) * pattern;
    
    strip.setPixelColor(i, r, g, b);
  }
  
  angle += 0.05;
  twist += 0.02;
}

// Effect 6: DNA helix
void effectDNAHelix() {
  static float phase = 0;
  static float rotation = 0;
  
  for(int i = 0; i < strip.numPixels(); i++) {
    float pos = (float)i / strip.numPixels();
    
    // Create double helix
    float helix1 = sin(pos * 20.0 + phase);
    float helix2 = sin(pos * 20.0 + phase + 3.14159); // 180 degrees out of phase
    float backbone = sin(pos * 5.0 + rotation) * 0.3 + 0.7;
    
    // Determine which part of the helix to light
    if(helix1 > 0.7) {
      // First strand - red
      strip.setPixelColor(i, 255 * backbone, 50 * backbone, 50 * backbone);
    } else if(helix2 > 0.7) {
      // Second strand - blue
      strip.setPixelColor(i, 50 * backbone, 50 * backbone, 255 * backbone);
    } else if(abs(helix1) < 0.2) {
      // Connection - yellow
      strip.setPixelColor(i, 200 * backbone, 200 * backbone, 0);
    } else {
      // Background - very dim
      strip.setPixelColor(i, 1, 2, 3);
    }
  }
  
  phase += 0.1;
  rotation += 0.02;
}

// Effect 7: Audio visualizer
void effectAudioVisualizer() {
  static uint8_t bands[8] = {0};
  static uint8_t peaks[8] = {0};
  static uint32_t lastUpdate = 0;
  static uint32_t peakTimer[8] = {0};
  
  // Simulate audio bands with more natural movement
  if(millis() - lastUpdate > 80) {
    lastUpdate = millis();
    for(int i = 0; i < 8; i++) {
      // Random walk for more natural movement
      int change = random(-20, 25);
      bands[i] = constrain(bands[i] + change, 10, 255);
      
      // Occasionally create peaks
      if(random(100) < 5 && bands[i] > 150) {
        peaks[i] = 255;
        peakTimer[i] = millis();
      }
    }
  }
  
  // Clear strip with gradient background
  for(int i = 0; i < strip.numPixels(); i++) {
    uint8_t bgBrightness = 5 + (i % 16);
    strip.setPixelColor(i, bgBrightness / 4, bgBrightness / 8, bgBrightness / 2);
  }
  
  // Draw audio bands
  for(int band = 0; band < 8; band++) {
    int bandWidth = strip.numPixels() / 16;
    int startPos = band * (strip.numPixels() / 8);
    int height = bands[band] * 10 / 255;
    
    for(int level = 0; level < height; level++) {
      for(int w = 0; w < bandWidth; w++) {
        int pos = startPos + w + level * 16;
        if(pos < strip.numPixels()) {
          uint8_t intensity = 255 - (level * 20);
          uint8_t hue = (band * 32) & 255;
          uint32_t color = Wheel(hue);
          
          // Peak indicator
          if(peaks[band] > 0 && level == height - 1) {
            strip.setPixelColor(pos, 255, 255, 255);
          } else {
            strip.setPixelColor(pos, color);
          }
        }
      }
    }
    
    // Decay peaks
    if(peaks[band] > 0 && millis() - peakTimer[band] > 100) {
      peaks[band] = peaks[band] * 9 / 10;
      if(peaks[band] < 10) peaks[band] = 0;
    }
  }
}

// Effect 8: Lava lamp
void effectLavaLamp() {
  static float blobs[4][4] = {
    {0.2, 0.3, 0.1, 0.0}, // x, y, size, phase
    {0.7, 0.2, 0.15, 1.57},
    {0.3, 0.7, 0.12, 3.14},
    {0.8, 0.6, 0.18, 4.71}
  };
  static float velocities[4][2] = {{0.008, 0.012}, {-0.01, 0.008}, {0.007, -0.009}, {-0.006, -0.011}};
  static uint8_t blobHues[4] = {0, 30, 60, 90};
  
  // Clear with dark purple background
  strip.fill(strip.Color(15, 0, 25));
  
  // Update blob positions and properties
  for(int b = 0; b < 4; b++) {
    blobs[b][0] += velocities[b][0];
    blobs[b][1] += velocities[b][1];
    blobs[b][2] = 0.1 + 0.1 * (sin(blobs[b][3]) + 1.0) / 2.0;
    blobs[b][3] += 0.03;
    blobHues[b] += 1;
    
    // Bounce off edges with slight randomness
    if(blobs[b][0] < 0.1 || blobs[b][0] > 0.9) {
      velocities[b][0] *= -1.0;
      blobs[b][0] = (blobs[b][0] < 0.1) ? 0.1 : 0.9;
    }
    if(blobs[b][1] < 0.1 || blobs[b][1] > 0.9) {
      velocities[b][1] *= -1.0;
      blobs[b][1] = (blobs[b][1] < 0.1) ? 0.1 : 0.9;
    }
  }
  
  // Draw blobs
  for(int i = 0; i < strip.numPixels(); i++) {
    float x = (float)(i % 16) / 16.0;
    float y = (float)(i / 16) / 11.0;
    
    float totalBrightness = 0;
    uint32_t blendedColor = 0;
    
    for(int b = 0; b < 4; b++) {
      float dx = x - blobs[b][0];
      float dy = y - blobs[b][1];
      float distance = sqrt(dx * dx + dy * dy);
      
      if(distance < blobs[b][2]) {
        float brightness = 1.0 - (distance / blobs[b][2]);
        brightness = brightness * brightness * brightness; // Cubic falloff
        
        uint32_t blobColor = Wheel(blobHues[b]);
        totalBrightness += brightness;
        
        // Blend colors
        if(blendedColor == 0) {
          blendedColor = blobColor;
        } else {
          blendedColor = colorBlend(blendedColor, blobColor, (uint8_t)(brightness * 128));
        }
      }
    }
    
    if(totalBrightness > 0) {
      if(totalBrightness > 1.0) totalBrightness = 1.0;
      uint8_t r = ((blendedColor >> 16) & 0xFF) * totalBrightness;
      uint8_t g = ((blendedColor >> 8) & 0xFF) * totalBrightness;
      uint8_t b = (blendedColor & 0xFF) * totalBrightness;
      strip.setPixelColor(i, r, g, b);
    }
  }
}

// Effect 9: Radar sweep
void effectRadarSweep() {
  static float angle = 0;
  static uint8_t sweepHistory[180] = {0};
  static uint32_t lastBlip = 0;
  static uint8_t blips[5] = {0};
  static uint8_t blipSizes[5] = {0};
  static uint8_t blipAges[5] = {0};
  
  // Fade sweep history
  for(int i = 0; i < strip.numPixels(); i++) {
    if(sweepHistory[i] > 0) {
      sweepHistory[i] = sweepHistory[i] * 9 / 10;
      if(sweepHistory[i] < 5) sweepHistory[i] = 0;
      
      uint8_t green = sweepHistory[i];
      strip.setPixelColor(i, 0, green, 0);
    } else {
      // Grid background
      if((i / 16) % 2 == 0) {
        strip.setPixelColor(i, 0, 1, 0);
      } else {
        strip.setPixelColor(i, 0, 0, 0);
      }
    }
  }
  
  // Draw sweep line
  int sweepPos = (angle / (2 * 3.14159)) * strip.numPixels();
  if(sweepPos < strip.numPixels()) {
    sweepHistory[sweepPos] = 255;
    strip.setPixelColor(sweepPos, 0, 255, 0);
    
    // Draw sweep line with falloff
    for(int i = 1; i < 5; i++) {
      int pos = sweepPos - i;
      if(pos >= 0 && pos < strip.numPixels()) {
        uint8_t intensity = 200 - (i * 40);
        if(intensity > sweepHistory[pos]) {
          sweepHistory[pos] = intensity;
          strip.setPixelColor(pos, 0, intensity, 0);
        }
      }
    }
  }
  
  // Create new blips
  if(millis() - lastBlip > 500 && random(100) < 20) {
    lastBlip = millis();
    for(int i = 0; i < 5; i++) {
      if(blips[i] == 0) {
        blips[i] = random(strip.numPixels());
        blipSizes[i] = random(2, 6);
        blipAges[i] = 255;
        break;
      }
    }
  }
  
  // Update and draw blips
  for(int i = 0; i < 5; i++) {
    if(blips[i] > 0) {
      for(int j = 0; j < blipSizes[i]; j++) {
        int pos = blips[i] + j;
        if(pos < strip.numPixels()) {
          uint8_t intensity = blipAges[i];
          strip.setPixelColor(pos, intensity, intensity, 0);
        }
      }
      blipAges[i] = blipAges[i] * 19 / 20;
      if(blipAges[i] < 10) {
        blips[i] = 0;
      }
    }
  }
  
  angle += 0.08;
  if(angle >= 2 * 3.14159) angle = 0;
}

// Effect 10: Quantum particles
void effectQuantumParticles() {
  static float particles[10][3] = {0}; // x, velocity, phase
  static uint8_t particleColors[10] = {0};
  static uint32_t lastSpawn = 0;
  
  // Clear with space background
  for(int i = 0; i < strip.numPixels(); i++) {
    if(random(1000) < 2) {
      strip.setPixelColor(i, 50, 50, 100); // Stars
    } else {
      strip.setPixelColor(i, 5, 5, 15); // Space
    }
  }
  
  // Spawn new particles
  if(millis() - lastSpawn > 200 && random(100) < 30) {
    lastSpawn = millis();
    for(int i = 0; i < 10; i++) {
      if(particles[i][0] == 0) {
        particles[i][0] = 0; // Start from left
        particles[i][1] = random(10, 30) / 100.0; // Velocity
        particles[i][2] = random(100) / 100.0 * 2 * 3.14159; // Phase
        particleColors[i] = random(256);
        break;
      }
    }
  }
  
  // Update and draw particles
  for(int i = 0; i < 10; i++) {
    if(particles[i][0] > 0) {
      particles[i][0] += particles[i][1];
      particles[i][2] += 0.2;
      
      int pos = particles[i][0] * strip.numPixels();
      if(pos < strip.numPixels()) {
        // Quantum uncertainty - particle appears at multiple positions
        for(int j = -2; j <= 2; j++) {
          int quantumPos = pos + j;
          if(quantumPos >= 0 && quantumPos < strip.numPixels()) {
            float probability = 1.0 / (1.0 + abs(j)); // Higher probability near center
            uint8_t intensity = (uint8_t)(255 * probability * (sin(particles[i][2]) + 1.0) / 2.0);
            
            uint32_t color = Wheel(particleColors[i]);
            uint8_t r = ((color >> 16) & 0xFF) * intensity / 255;
            uint8_t g = ((color >> 8) & 0xFF) * intensity / 255;
            uint8_t b = (color & 0xFF) * intensity / 255;
            
            strip.setPixelColor(quantumPos, r, g, b);
          }
        }
      }
      
      // Remove particles that go off screen
      if(particles[i][0] > 1.2) {
        particles[i][0] = 0;
      }
    }
  }
}

// Effect 11: Neural network
void effectNeuralNetwork() {
  static uint8_t neurons[20] = {0};
  static uint8_t connections[20][20] = {0};
  static uint32_t lastFire = 0;
  static uint8_t activation = 0;
  
  // Clear with dark background
  strip.fill(strip.Color(1, 1, 3));
  
  // Randomly activate neurons
  if(millis() - lastFire > 50) {
    lastFire = millis();
    
    // Fire random neuron
    if(random(100) < 40) {
      int neuron = random(20);
      neurons[neuron] = 255;
      
      // Create connections to other neurons
      for(int i = 0; i < 5; i++) {
        int target = random(20);
        if(target != neuron) {
          connections[neuron][target] = 200;
        }
      }
    }
    
    activation = (activation + 1) % 256;
  }
  
  // Draw neurons and connections
  for(int i = 0; i < 20; i++) {
    int neuronPos = i * (strip.numPixels() / 20);
    
    // Draw neuron
    if(neurons[i] > 0) {
      uint8_t intensity = neurons[i];
      strip.setPixelColor(neuronPos, intensity, intensity / 2, intensity);
      neurons[i] = neurons[i] * 8 / 10; // Decay
      if(neurons[i] < 5) neurons[i] = 0;
    }
    
    // Draw connections
    for(int j = 0; j < 20; j++) {
      if(connections[i][j] > 0) {
        int startPos = i * (strip.numPixels() / 20);
        int endPos = j * (strip.numPixels() / 20);
        
        // Draw connection line
        int steps = abs(endPos - startPos);
        for(int k = 0; k <= steps; k++) {
          int pos = startPos + (endPos - startPos) * k / steps;
          if(pos < strip.numPixels()) {
            uint8_t current = strip.getPixelColor(pos) & 0xFF;
            uint8_t newBlue = (current > connections[i][j] / 3) ? current : connections[i][j] / 3;
            strip.setPixelColor(pos, 0, 0, newBlue);
          }
        }
        
        connections[i][j] = connections[i][j] * 9 / 10; // Decay connection
        if(connections[i][j] < 5) connections[i][j] = 0;
      }
    }
  }
}

// Effect 12: Galaxy spin
void effectGalaxySpin() {
  static float angle = 0;
  static float spiral = 0;
  
  for(int i = 0; i < strip.numPixels(); i++) {
    float pos = (float)i / strip.numPixels();
    
    // Create spiral galaxy arms
    float arm1 = sin(pos * 15.0 + angle) * 0.5 + 0.5;
    float arm2 = sin(pos * 15.0 + angle + 2.094) * 0.5 + 0.5; // 120 degrees
    float arm3 = sin(pos * 15.0 + angle + 4.189) * 0.5 + 0.5; // 240 degrees
    
    float brightness = (arm1 > arm2) ? arm1 : arm2;
    brightness = (brightness > arm3) ? brightness : arm3;
    brightness = brightness * brightness; // Enhance contrast
    
    // Core of the galaxy
    float core = 1.0 - abs(pos - 0.5) * 2.0;
    if(core > 0) {
      brightness = (brightness > core * core) ? brightness : core * core;
    }
    
    // Color based on position (blue to purple gradient)
    uint8_t hue = 170 + (pos * 50); // Blue to purple
    uint32_t color = Wheel(hue);
    
    uint8_t r = ((color >> 16) & 0xFF) * brightness;
    uint8_t g = ((color >> 8) & 0xFF) * brightness;
    uint8_t b = (color & 0xFF) * brightness;
    
    // Add stars
    if(random(1000) < 3 && brightness < 0.3) {
      strip.setPixelColor(i, 255, 255, 255);
    } else {
      strip.setPixelColor(i, r, g, b);
    }
  }
  
  angle += 0.03;
  spiral += 0.01;
}

// Effect 13: Crystal growth
void effectCrystalGrowth() {
  static uint8_t crystals[180] = {0};
  static uint8_t crystalColors[180] = {0};
  static uint32_t lastGrowth = 0;
  static uint8_t activeSeeds = 0;
  
  // Initialize seeds
  if(activeSeeds < 5 && random(100) < 10) {
    int seedPos = random(strip.numPixels());
    if(crystals[seedPos] == 0) {
      crystals[seedPos] = 1;
      crystalColors[seedPos] = random(256);
      activeSeeds++;
    }
  }
  
  // Grow crystals
  if(millis() - lastGrowth > 100) {
    lastGrowth = millis();
    
    for(int i = 0; i < strip.numPixels(); i++) {
      if(crystals[i] > 0 && crystals[i] < 255) {
        // Grow to adjacent pixels
        for(int dir = -1; dir <= 1; dir += 2) {
          int neighbor = i + dir;
          if(neighbor >= 0 && neighbor < strip.numPixels() && random(100) < 30) {
            if(crystals[neighbor] == 0) {
              crystals[neighbor] = 1;
              crystalColors[neighbor] = crystalColors[i]; // Same color as parent
              activeSeeds++;
            }
          }
        }
        crystals[i] = (crystals[i] + 5 > 255) ? 255 : crystals[i] + 5; // Brighten existing crystal
      }
    }
  }
  
  // Draw crystals
  for(int i = 0; i < strip.numPixels(); i++) {
    if(crystals[i] > 0) {
      uint32_t color = Wheel(crystalColors[i]);
      uint8_t r = ((color >> 16) & 0xFF) * crystals[i] / 255;
      uint8_t g = ((color >> 8) & 0xFF) * crystals[i] / 255;
      uint8_t b = (color & 0xFF) * crystals[i] / 255;
      strip.setPixelColor(i, r, g, b);
    } else {
      strip.setPixelColor(i, 0, 0, 0);
    }
  }
  
  // Occasionally reset when fully grown
  if(activeSeeds >= strip.numPixels() * 0.8 && random(100) < 5) {
    memset(crystals, 0, sizeof(crystals));
    activeSeeds = 0;
  }
}

// Effect 14: Lightning storm
void effectLightningStorm() {
  static uint8_t lightning[180] = {0};
  static uint32_t lastStrike = 0;
  static uint8_t strikeActive = 0;
  static int strikePos = 0;
  static uint8_t flash = 0;
  
  // Background - storm clouds
  for(int i = 0; i < strip.numPixels(); i++) {
    uint8_t cloud = 10 + random(10);
    strip.setPixelColor(i, cloud, cloud, cloud + 10);
  }
  
  // Lightning strike
  if(!strikeActive && millis() - lastStrike > 1000 && random(100) < 10) {
    strikeActive = 1;
    strikePos = random(strip.numPixels());
    flash = 255;
    lastStrike = millis();
    
    // Create lightning path
    memset(lightning, 0, sizeof(lightning));
    int currentPos = strikePos;
    for(int i = 0; i < 20; i++) {
      if(currentPos >= 0 && currentPos < strip.numPixels()) {
        lightning[currentPos] = 255;
        // Lightning can branch
        currentPos += random(-2, 3);
        if(random(100) < 20) { // Branch
          int branchPos = currentPos + random(-5, 6);
          if(branchPos >= 0 && branchPos < strip.numPixels()) {
            lightning[branchPos] = 200;
          }
        }
      }
    }
  }
  
  // Draw lightning
  if(strikeActive) {
    for(int i = 0; i < strip.numPixels(); i++) {
      if(lightning[i] > 0) {
        strip.setPixelColor(i, flash, flash, 255);
        lightning[i] = lightning[i] * 7 / 8; // Fade lightning
      }
    }
    flash = flash * 8 / 10;
    if(flash < 10) {
      strikeActive = 0;
    }
  }
  
  // Rain drops
  if(random(100) < 30) {
    int dropPos = random(strip.numPixels());
    strip.setPixelColor(dropPos, 100, 100, 255);
  }
}

// Effect 15: Ocean depth
void effectOceanDepth() {
  static float wavePhase = 0;
  static float creaturePhase = 0;
  static uint8_t creatures[5] = {0};
  static uint8_t creaturePos[5] = {0};
  static uint8_t creatureTypes[5] = {0};
  
  for(int i = 0; i < strip.numPixels(); i++) {
    float depth = (float)i / strip.numPixels();
    
    // Ocean color gradient (deep blue to green)
    uint8_t r = 0;
    uint8_t g = (uint8_t)(50 * (1.0 - depth) + 100 * depth);
    uint8_t b = (uint8_t)(100 + 155 * depth);
    
    // Wave patterns
    float wave1 = sin(i * 0.1 + wavePhase) * 0.3 + 0.7;
    float wave2 = sin(i * 0.05 + wavePhase * 0.7) * 0.2 + 0.8;
    
    r = (uint8_t)(r * wave1);
    g = (uint8_t)(g * wave2);
    b = (uint8_t)(b * (wave1 + wave2) / 2.0);
    
    // Sunlight rays from top
    if(i < 30) {
      float ray = (30 - i) / 30.0;
      r = (uint8_t)(r + 50 * ray);
      g = (uint8_t)(g + 50 * ray);
    }
    
    strip.setPixelColor(i, r, g, b);
  }
  
  // Marine creatures
  creaturePhase += 0.02;
  for(int c = 0; c < 5; c++) {
    if(creatures[c] == 0 && random(100) < 5) {
      creatures[c] = 1;
      creaturePos[c] = random(20, strip.numPixels());
      creatureTypes[c] = random(3);
    }
    
    if(creatures[c] > 0) {
      // Move creature
      creaturePos[c] -= 1;
      
      if(creaturePos[c] < 0) {
        creatures[c] = 0;
      } else {
        // Draw creature based on type
        uint32_t color;
        switch(creatureTypes[c]) {
          case 0: color = strip.Color(255, 50, 50); break; // Red jellyfish
          case 1: color = strip.Color(255, 255, 100); break; // Yellow fish
          case 2: color = strip.Color(100, 255, 255); break; // Cyan creature
        }
        
        // Pulsing effect
        uint8_t pulse = (uint8_t)((sin(creaturePhase * 3 + c) + 1.0) * 127.5);
        uint32_t pulsedColor = colorBlend(0, color, pulse);
        
        strip.setPixelColor(creaturePos[c], pulsedColor);
        
        // Trail
        for(int t = 1; t <= 3; t++) {
          int trailPos = creaturePos[c] + t;
          if(trailPos < strip.numPixels()) {
            uint8_t trailIntensity = 100 - (t * 30);
            strip.setPixelColor(trailPos, 
              ((pulsedColor >> 16) & 0xFF) * trailIntensity / 255,
              ((pulsedColor >> 8) & 0xFF) * trailIntensity / 255,
              (pulsedColor & 0xFF) * trailIntensity / 255
            );
          }
        }
      }
    }
  }
  
  wavePhase += 0.05;
}

// Effect 16: Northern lights
void effectNorthernLights() {
  static float phase1 = 0;
  static float phase2 = 0;
  static float phase3 = 0;
  
  for(int i = 0; i < strip.numPixels(); i++) {
    float pos = (float)i / strip.numPixels();
    
    // Multiple layers of aurora
    float aurora1 = sin(pos * 8.0 + phase1) * 0.5 + 0.5;
    float aurora2 = sin(pos * 12.0 + phase2) * 0.3 + 0.7;
    float aurora3 = sin(pos * 6.0 + phase3) * 0.4 + 0.6;
    
    // Combine layers
    float brightness = (aurora1 + aurora2 + aurora3) / 3.0;
    brightness = brightness * brightness; // Enhance contrast
    
    // Green-purple aurora colors
    uint8_t r = (uint8_t)(50 * brightness + 100 * aurora3);
    uint8_t g = (uint8_t)(200 * brightness + 50 * aurora1);
    uint8_t b = (uint8_t)(150 * brightness + 100 * aurora2);
    
    // Stars in the background
    if(random(1000) < 2 && brightness < 0.3) {
      r = 255; g = 255; b = 255;
    }
    
    strip.setPixelColor(i, r, g, b);
  }
  
  phase1 += 0.02;
  phase2 += 0.015;
  phase3 += 0.025;
}

// Effect 17: Time tunnel
void effectTimeTunnel() {
  static float tunnelDepth = 0;
  static float rotation = 0;
  
  for(int i = 0; i < strip.numPixels(); i++) {
    float pos = (float)i / strip.numPixels();
    
    // Create tunnel effect with rotating pattern
    float angle = rotation + pos * 20.0;
    float radius = 0.5 + 0.3 * sin(pos * 10.0 + tunnelDepth);
    
    float tunnel = sin(angle) * cos(angle * 2.0) * radius;
    tunnel = (tunnel + 1.0) / 2.0; // Normalize to 0-1
    
    // Color shift through time
    uint8_t hue = (uint8_t)(tunnelDepth * 50 + pos * 100) % 256;
    uint32_t color = Wheel(hue);
    
    uint8_t r = ((color >> 16) & 0xFF) * tunnel;
    uint8_t g = ((color >> 8) & 0xFF) * tunnel;
    uint8_t b = (color & 0xFF) * tunnel;
    
    // Time particles flowing through tunnel
    if(random(1000) < 10) {
      float particle = sin(pos * 50.0 + tunnelDepth * 10.0);
      if(particle > 0.8) {
        r = 255; g = 255; b = 255;
      }
    }
    
    strip.setPixelColor(i, r, g, b);
  }
  
  tunnelDepth += 0.05;
  rotation += 0.03;
}

// Effect 18: Cyber city
void effectCyberCity() {
  static uint8_t buildings[16] = {0};
  static uint8_t buildingLights[16][10] = {0};
  static uint32_t lastUpdate = 0;
  static uint8_t scanLine = 0;
  
  // Initialize random building heights
  if(lastUpdate == 0) {
    for(int i = 0; i < 16; i++) {
      buildings[i] = random(5, 12);
      for(int j = 0; j < 10; j++) {
        buildingLights[i][j] = random(100) < 30 ? random(50, 255) : 0;
      }
    }
  }
  
  // Update building lights occasionally
  if(millis() - lastUpdate > 200) {
    lastUpdate = millis();
    
    for(int i = 0; i < 16; i++) {
      for(int j = 0; j < 10; j++) {
        if(random(100) < 10) { // 10% chance to change a light
          buildingLights[i][j] = random(100) < 40 ? random(100, 255) : 0;
        }
      }
    }
    
    scanLine = (scanLine + 1) % strip.numPixels();
  }
  
  // Draw cyber city
  strip.clear();
  
  // Draw buildings
  for(int col = 0; col < 16; col++) {
    int buildingHeight = buildings[col];
    
    for(int row = 0; row < buildingHeight; row++) {
      int pos = col + row * 16;
      if(pos < strip.numPixels()) {
        uint8_t intensity = buildingLights[col][row % 10];
        if(intensity > 0) {
          // Neon colors: pink, cyan, blue, purple
          uint32_t colors[4] = {0xFF00FF, 0x00FFFF, 0x0088FF, 0x8800FF};
          uint32_t color = colors[col % 4];
          strip.setPixelColor(pos, 
            ((color >> 16) & 0xFF) * intensity / 255,
            ((color >> 8) & 0xFF) * intensity / 255,
            (color & 0xFF) * intensity / 255
          );
        } else {
          // Building structure
          strip.setPixelColor(pos, 20, 20, 30);
        }
      }
    }
  }
  
  // Scanning laser line
  for(int i = 0; i < 3; i++) {
    int pos = scanLine + i;
    if(pos < strip.numPixels()) {
      strip.setPixelColor(pos, 0, 255, 0);
    }
  }
}

// Effect 19: Solar flare
void effectSolarFlare() {
  static float flareIntensity = 0;
  static float flarePhase = 0;
  static uint32_t lastFlare = 0;
  static uint8_t activeFlares = 0;
  static float flarePositions[3] = {0.2, 0.5, 0.8};
  static float flareSizes[3] = {0, 0, 0};
  
  // Background - sun surface
  for(int i = 0; i < strip.numPixels(); i++) {
    float pos = (float)i / strip.numPixels();
    
    // Sun surface with turbulence
    float turbulence = sin(pos * 20.0 + flarePhase) * 0.3 + 0.7;
    turbulence += sin(pos * 35.0 + flarePhase * 1.3) * 0.2;
    
    uint8_t r = (uint8_t)(255 * turbulence);
    uint8_t g = (uint8_t)(100 * turbulence);
    uint8_t b = (uint8_t)(50 * turbulence);
    
    strip.setPixelColor(i, r, g, b);
  }
  
  // Solar flares
  if(millis() - lastFlare > 1000 && random(100) < 20 && activeFlares < 3) {
    lastFlare = millis();
    for(int i = 0; i < 3; i++) {
      if(flareSizes[i] == 0) {
        flarePositions[i] = random(100) / 100.0;
        flareSizes[i] = 0.01;
        activeFlares++;
        break;
      }
    }
  }
  
  // Update and draw flares
  activeFlares = 0;
  for(int f = 0; f < 3; f++) {
    if(flareSizes[f] > 0) {
      flareSizes[f] += 0.02;
      
      // Draw flare
      for(int i = 0; i < strip.numPixels(); i++) {
        float pos = (float)i / strip.numPixels();
        float distance = abs(pos - flarePositions[f]);
        
        if(distance < flareSizes[f]) {
          float intensity = 1.0 - (distance / flareSizes[f]);
          intensity = intensity * intensity; // Quadratic falloff
          
          uint32_t currentColor = strip.getPixelColor(i);
          uint8_t r = (currentColor >> 16) & 0xFF;
          uint8_t g = (currentColor >> 8) & 0xFF;
          uint8_t b = currentColor & 0xFF;
          
          // Add flare color (white-hot)
          r = (r + 255 * intensity > 255) ? 255 : (uint8_t)(r + 255 * intensity);
          g = (g + 255 * intensity > 255) ? 255 : (uint8_t)(g + 255 * intensity);
          b = (b + 200 * intensity > 255) ? 255 : (uint8_t)(b + 200 * intensity);
          
          strip.setPixelColor(i, r, g, b);
        }
      }
      
      activeFlares++;
      
      // Remove expired flares
      if(flareSizes[f] > 0.3) {
        flareSizes[f] = 0;
        activeFlares--;
      }
    }
  }
  
  flarePhase += 0.04;
}

// Effect 20: Realistic fire simulation
void effectFireSimulation() {
  static uint32_t lastFire = 0;
  
  if (millis() - lastFire < effectSpeed / 2) return;
  lastFire = millis();
  
  for (int i = 0; i < strip.numPixels(); i++) {
    int flicker = random(0, 150);
    int r = 255 - flicker;
    int g = 100 - flicker / 2;
    int b = 0;
    
    // Ensure values don't go below 0
    r = (r > 0) ? r : 0;
    g = (g > 0) ? g : 0;
    
    strip.setPixelColor(i, strip.Color(r, g, b));
  }
}

// Effect 21: Solid Color
void effectSolidColor() {
  for(int i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, effectColor);
  }
}

// Helper function for scaling values
uint8_t scale8(uint8_t i, uint8_t scale) {
  return (uint16_t(i) * (uint16_t(scale) + 1)) >> 8;
}

// Helper function for rainbow effect
uint32_t Wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if(WheelPos < 85) {
    return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if(WheelPos < 170) {
    WheelPos -= 85;
    return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}

// Helper function for fire effect
uint32_t HeatColor(uint8_t temperature) {
  uint8_t t192 = round((temperature / 255.0) * 191);
  uint8_t heatramp = t192 & 0x3F;
  heatramp <<= 2;
  
  if(t192 > 0x80) {
    return strip.Color(255, 255, heatramp);
  } else if(t192 > 0x40) {
    return strip.Color(255, heatramp, 0);
  } else {
    return strip.Color(heatramp, 0, 0);
  }
}

// FastLED-compatible math functions
uint8_t qsub8(uint8_t i, uint8_t j) {
  int t = i - j;
  return t < 0 ? 0 : t;
}

uint8_t qadd8(uint8_t i, uint8_t j) {
  unsigned int t = i + j;
  return t > 255 ? 255 : t;
}

uint8_t random8() {
  return random(256);
}

uint8_t random8(uint8_t lim) {
  return random(lim);
}

uint8_t random8(uint8_t min, uint8_t max) {
  return random(min, max);
}

// Color blending helper
uint32_t colorBlend(uint32_t color1, uint32_t color2, uint8_t blend) {
  uint8_t r1 = (color1 >> 16) & 0xFF;
  uint8_t g1 = (color1 >> 8) & 0xFF;
  uint8_t b1 = color1 & 0xFF;
  
  uint8_t r2 = (color2 >> 16) & 0xFF;
  uint8_t g2 = (color2 >> 8) & 0xFF;
  uint8_t b2 = color2 & 0xFF;
  
  uint8_t r = (r1 * (255 - blend) + r2 * blend) / 255;
  uint8_t g = (g1 * (255 - blend) + g2 * blend) / 255;
  uint8_t b = (b1 * (255 - blend) + b2 * blend) / 255;
  
  return strip.Color(r, g, b);
}

// Set all LEDs to a color
void setAllLeds(uint32_t color) {
  for(int i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, color);
  }
}

void createDefaultFirebaseData() {
  Serial.println("Creating default Firebase data structure...");
  
  // Set default effect
  if (Firebase.RTDB.setInt(&fbdoUpload, "/effect", 0)) {
    Serial.println("Effect set to default: 0");
  } else {
    Serial.printf("Failed to set effect: %s\n", fbdoUpload.errorReason().c_str());
  }
  
  // Set default speed
  if (Firebase.RTDB.setInt(&fbdoUpload, "/speed", 50)) {
    Serial.println("Speed set to default: 50");
  } else {
    Serial.printf("Failed to set speed: %s\n", fbdoUpload.errorReason().c_str());
  }
  
  // Set default color
  if (Firebase.RTDB.setString(&fbdoUpload, "/color", "FF0000")) {
    Serial.println("Color set to default: FF0000");
  } else {
    Serial.printf("Failed to set color: %s\n", fbdoUpload.errorReason().c_str());
  }
  
  // Set default enabled state
  if (Firebase.RTDB.setBool(&fbdoUpload, "/enabled", true)) {
    Serial.println("Enabled set to default: true");
  } else {
    Serial.printf("Failed to set enabled: %s\n", fbdoUpload.errorReason().c_str());
  }
  
  // Set default auto darkness control
  if (Firebase.RTDB.setBool(&fbdoUpload, "/auto_darkness_control", true)) {
    Serial.println("Auto darkness control set to default: true");
  } else {
    Serial.printf("Failed to set auto darkness control: %s\n", fbdoUpload.errorReason().c_str());
  }
  if (Firebase.RTDB.setFloat(&fbdoUpload, "/lux_threshold", 1.0)) {
  Serial.println("Lux threshold set to default: 1.0");
} else {
  Serial.printf("Failed to set lux threshold: %s\n", fbdoUpload.errorReason().c_str());
}
}
