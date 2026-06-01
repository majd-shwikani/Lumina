#ifndef EFFECTS_H
#define EFFECTS_H

#include <FastLED.h>

// ============================================================================
// EXTERNAL VARIABLE DECLARATIONS
// ============================================================================

extern CRGB *leds;
extern volatile uint32_t effectColor;
extern volatile uint32_t effectSpeed;

// ============================================================================
// ORIGINAL ANIMATION EFFECTS (0-21)
// ============================================================================

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

// ============================================================================
// ADVANCED SOUND-REACTIVE EFFECTS (22-26)
// ============================================================================

void effectSpectrumRipple();       // 22: Spectrum Ripple
void effectKineticPlasma();        // 23: Kinetic Plasma
void effectTransientPulse();       // 24: Transient Pulse
void effectSpectrumBars();         // 25: Spectrum Bars
void effectSpectralVerve();        // 26: Spectral Verve

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

// ============================================================================
// FIRE SIMULATION EFFECTS (53-62)
// ============================================================================

void effectBlueGasFlame();         // 53: Blue gas burner flame
void effectWildfire();             // 54: Wind-fanned wildfire with embers
void effectCandleFlame();          // 55: Intimate single candle flicker
void effectCampfire();             // 56: Roaring campfire / bonfire
void effectPlasmaFire();           // 57: Supernatural violet-magenta plasma fire
void effectInferno();              // 58: Maximum-intensity furnace blaze
void effectSmolderingEmbers();     // 59: Dying embers glowing with radiant heat
void effectLavaFlow();             // 60: Slow molten lava with hardening crust
void effectEmberStorm();           // 61: Wind-gusted fire with ember ejection
void effectHearthFire();           // 62: Cosy living-room hearth

// ============================================================================
// HELPER FUNCTION DECLARATIONS
// ============================================================================

uint32_t Wheel(byte WheelPos);
uint32_t EffectHeatColor(uint8_t temperature);
uint32_t colorBlend(uint32_t color1, uint32_t color2, uint8_t blend);
void setAllLeds(uint32_t color);

#endif