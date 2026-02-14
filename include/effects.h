#ifndef EFFECTS_H
#define EFFECTS_H

#include <Arduino.h>
#include <FastLED.h>

// Helper functions
uint32_t stripColor(uint8_t r, uint8_t g, uint8_t b);
uint32_t getPixelColor(int i);
void setPixelColor(int i, uint8_t r, uint8_t g, uint8_t b);
void setPixelColor(int i, uint32_t c);
uint32_t Wheel(byte WheelPos);
uint32_t colorBlend(uint32_t color1, uint32_t color2, uint8_t blend);
void setAllLeds(uint32_t color);

// Core Effects (0-21, skipping audio effect 7)
void effectRainbow();              // 0
void effectMeteorShower();         // 1
void effectDigitalRain();          // 2
void effectPulsingSpheres();       // 3
void effectBinaryClock();          // 4
void effectVortex();               // 5
void effectDNAHelix();             // 6
// effectAudioVisualizer (7) is skipped
void effectLavaLamp();             // 8
void effectRadarSweep();           // 9
void effectQuantumParticles();     // 10
void effectNeuralNetwork();        // 11
void effectGalaxySpin();           // 12
void effectCrystalGrowth();        // 13
void effectLightningStorm();       // 14
void effectOceanDepth();           // 15
void effectNorthernLights();       // 16
void effectTimeTunnel();           // 17
void effectCyberCity();            // 18
void effectSolarFlare();           // 19
void effectFireSimulation();       // 20
void effectSolidColor();           // 21

#endif
