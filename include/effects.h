#ifndef EFFECTS_H
#define EFFECTS_H

#include <FastLED.h>

// ============================================================================
// EXTERNAL VARIABLE DECLARATIONS
// ============================================================================

extern CRGB *leds;
extern volatile uint32_t effectColor;
extern volatile uint32_t effectSpeed;

// Audio-related externals
extern volatile double detectedFrequency;
extern volatile double frequencyMagnitude;
extern volatile double globalAudioLevel;
extern volatile double bassLevel;
extern volatile double midLevel;
extern volatile double trebleLevel;
extern volatile bool beatDetected;
extern volatile float beatEnergy;
extern volatile int activeMicrophone;

// Auto-calibration externals
extern volatile bool calibrationComplete;
extern volatile double noiseFloor;
extern volatile double gainMultiplier;

// Number of frequency bands
#define NUM_FREQ_BANDS 8

extern double bandMagnitudes[NUM_FREQ_BANDS];
extern double bandMaxima[NUM_FREQ_BANDS];

// Function declarations
extern void updateFrequencyDetection();
extern void analyzeAudioBands();
extern void detectBeat();

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
// NEW SOUND-REACTIVE EFFECTS (22-35)
// ============================================================================

void effectFrequencySpectrum();    // 22: Frequency spectrum analyzer
void effectReactiveWaveform();     // 23: Audio waveform visualization
void effectBeatPulse();            // 24: Beat-reactive pulse
void effectFrequencyBloom();       // 25: Frequencies bloom outward
void effectAudioReactiveFire();    // 26: Music-reactive fire
void effectMusicalRainbow();       // 27: Colors shift with frequencies
void effectReactiveStrobe();       // 28: Beat-driven strobe
void effectGuitarVisualizer();     // 29: Guitar-optimized visualizer
void effectCascadingFrequency();   // 30: Frequency cascade waterfall
void effectEnergyOrbits();         // 31: Orbiting particles (audio-driven)
void effectAudioRipples();         // 32: Sound creates ripples

// ============================================================================
// HELPER FUNCTION DECLARATIONS
// ============================================================================

uint32_t Wheel(byte WheelPos);
uint32_t EffectHeatColor(uint8_t temperature);
uint32_t colorBlend(uint32_t color1, uint32_t color2, uint8_t blend);
void setAllLeds(uint32_t color);

#endif