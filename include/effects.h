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

// Core Effects
void effectRainbow();
void effectMeteorShower();
void effectDigitalRain();
void effectPulsingSpheres();
void effectBinaryClock();
void effectVortex();
void effectDNAHelix();
void effectLavaLamp();
void effectQuantumParticles();
void effectNeuralNetwork();
void effectGalaxySpin();
void effectCrystalGrowth();
void effectLightningStorm();
void effectOceanDepth();
void effectNorthernLights();
void effectTimeTunnel();
void effectCyberCity();
void effectSolarFlare();
void effectFireSimulation();
void effectSolidColor();
void effectEnergyOrbits();

#endif
