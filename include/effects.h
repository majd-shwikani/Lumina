#ifndef EFFECTS_H
#define EFFECTS_H

#include <Adafruit_NeoPixel.h>

// Declare external variables that effects need to access
extern Adafruit_NeoPixel strip;
extern volatile uint32_t effectColor;
extern volatile uint32_t effectSpeed;  // ADD THIS LINE
extern volatile double detectedFrequency;
extern volatile double frequencyMagnitude;
extern void updateFrequencyDetection();

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
void effectSolidColor();           // 21: Solid Color
void effectFrequencyResponse();
void effectPianoTiles();
void effectPianoTilesBars();
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

#endif