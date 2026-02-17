#ifndef EFFECTS_H
#define EFFECTS_H

#include <Arduino.h>
#include <FastLED.h>

// ============================================================================
// EXTERNAL VARIABLE DECLARATIONS
// ============================================================================

extern CRGB *leds;
extern volatile uint32_t effectColor;
extern volatile uint32_t effectSpeed;



// ============================================================================
// NEW REVOLUTIONARY EFFECTS (33-42)
// ============================================================================

void effectPlasmaWaves();          // 33
void effectConfettiPalettes();     // 34
void effectSinelonDual();          // 35
void effectBPM();                  // 36
void effectJuggle();               // 37
void effectGlitterRainbow();       // 38
void effectPacific();              // 39
void effectTwinkleFox();           // 40
void effectColorWaves();           // 41
void effectPerlinMove();           // 42

// ============================================================================
// HELPER FUNCTION DECLARATIONS
// ============================================================================

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
// ============================================================================
// ORGANIC LAMP EFFECTS (43-52)
// ============================================================================

void effectPlasmaLamp();           // 43
void effectBiolume();              // 44
void effectDeepSeaVolcano();       // 45
void effectMagicalAurora();        // 46
void effectSolarWinds();           // 47
void effectEtherealMist();         // 48
void effectBioPulse();             // 49
void effectRadioactiveGlow();      // 50
void effectSupernova();            // 51
void effectEnchantedStream();      // 52
#endif
