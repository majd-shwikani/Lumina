#include "globals.h"
#include "effects.h"
#include <Arduino.h>

// ============================================================================
// SHARED PALETTES & UTILS
// ============================================================================

// Global palette for effects that need one (Fire, Water, Cyber, etc.)
CRGBPalette16 currentPalette;
CRGBPalette16 targetPalette;

// Define FairyLight_p for TwinkleFox
const TProgmemRGBPalette16 FairyLight_p FL_PROGMEM =
{
    CRGB::White,      CRGB::AliceBlue,  CRGB::Azure,      CRGB::WhiteSmoke,
    CRGB::Ivory,      CRGB::FloralWhite,CRGB::OldLace,    CRGB::Linen,
    CRGB::AntiqueWhite,CRGB::AntiqueWhite,CRGB::AntiqueWhite,CRGB::AntiqueWhite,
    CRGB::AntiqueWhite,CRGB::AntiqueWhite,CRGB::AntiqueWhite,CRGB::AntiqueWhite
};

// Helper to fade all LEDs (creates trails)
void fadeAll(uint8_t scale) {
  fadeToBlackBy(leds, ledCount, scale);
}

// Effect 0: Rainbow cycle
// Replaced manual Wheel() with fill_rainbow
void effectRainbow() {
  static uint8_t hue = 0;
  fill_rainbow(leds, ledCount, hue, 7); // 7 is delta hue per pixel
  EVERY_N_MILLISECONDS(20) {
    hue++;
  }
}

// Effect 1: Meteor shower
// Uses fadeToBlackBy for trails instead of manual calculation
void effectMeteorShower() {
  // Fade everything to create tails
  fadeToBlackBy(leds, ledCount, 64); // Adjust 64 for tail length

  // Spawn new meteors
  EVERY_N_MILLISECONDS(100) {
    if (random8() < 40) { // 15% chance
       int pos = random16(ledCount);
       leds[pos] = CHSV(random8(), 200, 255);
    }
  }
  
  static int position = 0;
  
  // Move a meteor across
  EVERY_N_MILLISECONDS_I(timer, 0) {
    timer.setPeriod(map(effectSpeed, 0, 255, 10, 100));
    position++;
    if (position >= ledCount) {
      position = 0;
    }
  }
  
  // Draw the head
  if (position < ledCount) {
    leds[position] = CHSV(0, 0, 255); // White head
  }
}

// Effect 2: Digital rain
// Matrix style with CHSV and fading
void effectDigitalRain() {
  fadeToBlackBy(leds, ledCount, 20);

  EVERY_N_MILLISECONDS(80) {
    int pos = random16(ledCount);
    leds[pos] = CHSV(96, 255, 255); // Green
    // 10% chance of white tip
    if (random8() < 25) {
      leds[pos] = CRGB::White;
    }
  }
}

// Effect 3: Pulsing spheres
// Using beatsin8 for smooth sine wave modulation
void effectPulsingSpheres() {
  fadeToBlackBy(leds, ledCount, 20);
  
  // Three sine waves at different frequencies
  uint8_t pos1 = beatsin8(20, 0, ledCount - 1);
  uint8_t pos2 = beatsin8(27, 0, ledCount - 1);
  uint8_t pos3 = beatsin8(31, 0, ledCount - 1);
  
  // Add color with saturating math
  leds[pos1] += CHSV(0, 255, 255);   // Red
  leds[pos2] += CHSV(96, 255, 255);  // Green
  leds[pos3] += CHSV(160, 255, 255); // Blue
  
  // Blur to make them "spheres"
  blur1d(leds, ledCount, 64);
}

// Effect 4: Binary clock
// Keeping logic but using CRGB
void effectBinaryClock() {
  static uint8_t counter = 0;
  
  EVERY_N_MILLISECONDS(500) {
    counter++;
    FastLED.clear();
    
    // Background grid
    for(int i = 0; i < ledCount; i++) {
      if((i / 16) % 2 == (i % 2)) {
        leds[i] = CRGB(2, 5, 2);
      } else {
        leds[i] = CRGB(1, 3, 1);
      }
    }
    
    // Bits
    for(int bit = 0; bit < 8; bit++) {
      if(counter & (1 << bit)) {
        int startPos = bit * (ledCount / 8);
        int barHeight = (bit + 1) * 2;
        for(int j = 0; j < barHeight && j < 11; j++) {
           int pos = startPos + j * 16;
           if(pos < ledCount) leds[pos] = CRGB(0, 255 - (j*20), 0);
        }
      }
    }
  }
}

// Effect 5: Vortex
// Using beat8 for rotation and CHSV
void effectVortex() {
  uint8_t rotation = beat8(10); // 10 BPM
  
  for(int i = 0; i < ledCount; i++) {
    // Hue depends on position + rotation
    uint8_t hue = (i * 5) + rotation;
    // Value depends on a second wave
    uint8_t val = beatsin8(30, 100, 255, 0, i * 8); 
    leds[i] = CHSV(hue, 255, val);
  }
}

// Effect 6: DNA Helix
// Using phase-shifted sine waves
void effectDNAHelix() {
  FastLED.clear();
  uint8_t beat = beat8(20);
  
  for(int i = 0; i < ledCount; i++) {
    // Strand 1
    uint8_t y1 = cubicwave8((i * 10) + beat);
    // Strand 2 (180 deg out of phase)
    uint8_t y2 = cubicwave8((i * 10) + beat + 128);
    
    // Visualize "crossing" strands by brightness
    // In a 1D strip, we can map "y" to brightness or color
    leds[i] += CHSV(0, 255, map(y1, 0, 255, 0, 100));   // Red strand
    leds[i] += CHSV(160, 255, map(y2, 0, 255, 0, 100)); // Blue strand
    
    // Connection points (where they cross)
    if (abs(y1 - y2) < 20) {
      leds[i] += CRGB(100, 100, 0); // Yellow bridge
    }
  }
}

// Effect 7: Audio visualizer
// Simulating it with noise if no audio, or just using palette
void effectAudioVisualizer() {
  // Uses palette for "heat" map style
  static uint8_t hueOffset = 0;
  
  // Shift hue slowly
  EVERY_N_MILLISECONDS(50) { hueOffset++; }
  
  // Draw simulated bands (or real if connected)
  for(int i = 0; i < ledCount; i++) {
    // Just a placeholder visualization using noise
    uint8_t noise = inoise8(i * 30, millis() / 5);
    leds[i] = ColorFromPalette(RainbowColors_p, noise + hueOffset, noise);
  }
}

// Effect 8: Lava lamp
// Large, slow moving blobs using noise
void effectLavaLamp() {
  uint32_t ms = millis();
  for(int i = 0; i < ledCount; i++) {
    // Perlin noise for smooth organic movement
    // Scale x by 20, time by 10 (slow)
    uint8_t val = inoise8(i * 20, ms / 10);
    // Sharpen the noise to make "blobs"
    val = qsub8(val, 60); 
    val = qmul8(val, 3);
    
    // Map to palette (LavaColors_p)
    leds[i] = ColorFromPalette(LavaColors_p, val, 255);
  }
}

// Effect 9: Radar sweep
// Rotating bright spot with fading tail
void effectRadarSweep() {
  fadeToBlackBy(leds, ledCount, 10); // Short tail
  
  uint16_t pos = beatsin16(15, 0, ledCount - 1);
  leds[pos] = CRGB::Green;
  
  // Random blips
  if(random8() < 5) {
    leds[random16(ledCount)] = CRGB(0, 100, 0);
  }
}

// Effect 10: Quantum particles
// Random flickering pixels
void effectQuantumParticles() {
  fadeToBlackBy(leds, ledCount, 20);
  
  if(random8() < 80) {
    int pos = random16(ledCount);
    leds[pos] += CHSV(random8(), 200, 255);
  }
}

// Effect 11: Neural network
// Sending pulses down the strip
void effectNeuralNetwork() {
  fadeToBlackBy(leds, ledCount, 30);
  
  static int pulsePos = 0;
  EVERY_N_MILLISECONDS(30) {
    pulsePos++;
    if(pulsePos >= ledCount) {
      pulsePos = 0;
    }
    
    leds[pulsePos] = CRGB::Blue;
    // Synapse firing (random branching or sparking)
    if(random8() < 20) {
       leds[pulsePos] += CRGB::White;
    }
  }
}

// Effect 12: Galaxy spin
// Purple/Blue palette with glitter
void effectGalaxySpin() {
  fill_palette(leds, ledCount, millis() / 10, 5, OceanColors_p, 255, LINEARBLEND);
  
  // Add stars
  if(random8() < 10) {
    leds[random16(ledCount)] += CRGB::White;
  }
}

// Effect 13: Crystal growth
// Slowly lighting up pixels and keeping them
void effectCrystalGrowth() {
  static int activeSeeds = 0; 
  static uint8_t crystals[256]; // Buffer for crystal states
  EVERY_N_MILLISECONDS(100) {
    int pos = random16(ledCount);
    // Find a spot next to a lit pixel or random seed
    if (leds[pos].getAverageLight() == 0) {
      // Check neighbors
      bool neighborLit = false;
      if (pos > 0 && leds[pos-1].getAverageLight() > 0) neighborLit = true;
      if (pos < ledCount-1 && leds[pos+1].getAverageLight() > 0) neighborLit = true;
      
      if (neighborLit || random8() < 2) { // 2/256 chance to seed new
        leds[pos] = CHSV(180 + random8(60), 100, 255); // Cyan/Blue crystals
      }
    }
  }
  
  // Reset if full
  // (Simplified check)
  if (random8() == 0 && leds[ledCount/2].getAverageLight() > 0) {
    fadeToBlackBy(leds, ledCount, 20);
  }
  if(activeSeeds >= ledCount * 0.8 && random(100) < 5) { memset(crystals, 0, 256); activeSeeds = 0; }
}

// Effect 14: Lightning storm
// Cloud background + flashes
void effectLightningStorm() {
  // Dark stormy clouds
  uint8_t noise = inoise8(millis()/10);
  fill_solid(leds, ledCount, CRGB(noise/4, noise/4, noise/3));
  
  // Lightning strike
  if (random8() < 5) {
    int section = random16(ledCount - 10);
    fill_solid(&leds[section], 5 + random8(5), CRGB::White);
  }
}

// Effect 15: Ocean depth
// Blue/Teal gradient with wave motion
void effectOceanDepth() {
  uint8_t wave1 = beat8(10);
  uint8_t wave2 = beat8(15);
  
  for(int i = 0; i < ledCount; i++) {
    uint8_t index = (i * 5) + wave1 + wave2;
    leds[i] = ColorFromPalette(OceanColors_p, index, 255, LINEARBLEND);
  }
}

// Effect 16: Northern lights
// Green/Purple smooth noise
void effectNorthernLights() {
  static uint16_t x = 0;
  x += 10;
  for(int i = 0; i < ledCount; i++) {
    uint8_t noise = inoise8(i * 30 + x);
    // Map noise to Green -> Purple hue range (approx 96 to 192)
    uint8_t hue = map(noise, 0, 255, 100, 200);
    leds[i] = CHSV(hue, 255, noise); // Brightness varies with noise
  }
}

// Effect 17: Time tunnel
// Concentric expanding rings (1D equivalent: moving out from center)
void effectTimeTunnel() {
  uint8_t beat = beat8(60);
  int center = ledCount / 2;
  
  for(int i = 0; i <= center; i++) {
    uint8_t colorIndex = (i * 10) - beat;
    CRGB color = ColorFromPalette(RainbowColors_p, colorIndex, 255, LINEARBLEND);
    leds[center + i] = color;
    leds[center - i] = color;
  }
}

// Effect 18: Cyber city
// Pink/Cyan neon pulses
void effectCyberCity() {
  fadeToBlackBy(leds, ledCount, 40);
  
  EVERY_N_MILLISECONDS(200) {
    int pos = random16(ledCount);
    if(random8() > 128) {
      leds[pos] = CRGB(255, 0, 255); // Magenta
    } else {
      leds[pos] = CRGB(0, 255, 255); // Cyan
    }
  }
  
  // Car trails
  static int carPos = 0;
  leds[carPos] = CRGB::Red;
  carPos = (carPos + 1) % ledCount;
}

// Effect 19: Solar flare
// Heat colors with intense bursts
void effectSolarFlare() {
  // Use Fire logic but across the whole strip as a surface
  static uint8_t heat[255]; // Assumes max 255 for simplicity, better to dynamic alloc if needed
  
  // Cool down
  for(int i = 0; i < ledCount; i++) {
    heat[i] = qsub8(heat[i], random8(0, 10));
  }
  
  // Ignite
  if(random8() < 30) {
    heat[random16(ledCount)] = qadd8(heat[random16(ledCount)], random8(50, 150));
  }
  
  // Map to colors
  for(int i = 0; i < ledCount; i++) {
    leds[i] = HeatColor(heat[i]);
  }
}

// Effect 20: VERY REALISTIC FIRE (Horizontal Strip)
// Based on Fire2012WithPalette by Mark Kriegsman
// HORIZONTAL FIX: For a horizontal strip there is no "up". Heat is ignited
// randomly across the ENTIRE strip and diffuses symmetrically left+right using
// Perlin noise so every pixel flickers independently — no directional gradient.
void effectFireSimulation() {
  static byte heat[500];

  // 1. Cool every cell uniformly — no position bias
  for (int i = 0; i < ledCount; i++)
    heat[i] = qsub8(heat[i], random8(0, ((55 * 10) / ledCount) + 2));

  // 2. Local diffusion: average with neighbours (no direction)
  for (int k = 1; k < ledCount - 1; k++)
    heat[k] = (heat[k - 1] + heat[k] + heat[k] + heat[k + 1]) / 4;

  // 3. Ignite random sparks anywhere on the strip
  for (int s = 0; s < 3; s++) {
    if (random8() < 80) {
      int y = random16(ledCount);
      heat[y] = qadd8(heat[y], random8(160, 255));
    }
  }

  // 4. Perlin turbulence overlay — gives each pixel an organic flicker
  uint32_t ms = millis();
  for (int i = 0; i < ledCount; i++) {
    uint8_t turb = inoise8(i * 25, ms / 4);
    heat[i] = scale8(heat[i], map(turb, 0, 255, 180, 255));
  }

  // Black → Deep Red → Red → Orange → Gold → Yellow → White
  CRGBPalette16 firePalette = CRGBPalette16(
    CRGB::Black,      CRGB(60,0,0),     CRGB::Maroon,     CRGB::DarkRed,
    CRGB::Red,        CRGB::OrangeRed,  CRGB::Orange,     CRGB::Gold,
    CRGB::Yellow,     CRGB(255,230,100),CRGB::White,       CRGB::White,
    CRGB::White,      CRGB::White,      CRGB::White,       CRGB::White
  );

  for (int j = 0; j < ledCount; j++)
    leds[j] = ColorFromPalette(firePalette, scale8(heat[j], 240));

  // Random sparks
  if (random8() < 12) leds[random16(ledCount)] += CRGB(255, 200, 80);
}

// ============================================================================
// FIRE SIMULATION EFFECTS (53-62) — Ten ultra-realistic fire variants
// All use the horizontal model: uniform ignition + noise-based flickering.
// No directional heat drift — fire looks the same from any viewing angle.
// ============================================================================

// ---- Effect 53: Blue Gas Flame -----------------------------------------------
// The cool, intensely hot flame of a gas burner — cobalt blue core fading to
// pale cyan. Each pixel flickers independently like a true gas ring.
void effectBlueGasFlame() {
  static byte heat[500];
  uint32_t ms = millis();

  for (int i = 0; i < ledCount; i++)
    heat[i] = qsub8(heat[i], random8(0, ((60 * 10) / ledCount) + 2));

  for (int k = 1; k < ledCount - 1; k++)
    heat[k] = (heat[k - 1] + heat[k] + heat[k] + heat[k + 1]) / 4;

  for (int s = 0; s < 3; s++) {
    if (random8() < 100) {
      int y = random16(ledCount);
      heat[y] = qadd8(heat[y], random8(170, 255));
    }
  }

  // Turbulence — makes the blue flame roll
  for (int i = 0; i < ledCount; i++) {
    uint8_t turb = inoise8(i * 30, ms / 5);
    heat[i] = scale8(heat[i], map(turb, 0, 255, 170, 255));
  }

  // Black → Deep Blue → Royal Blue → Dodger Blue → Cyan → Ice White
  CRGBPalette16 blueFire = CRGBPalette16(
    CRGB::Black,       CRGB(0,0,40),      CRGB(0,0,100),     CRGB(0,30,180),
    CRGB(0,80,255),    CRGB(0,160,255),   CRGB(40,200,255),  CRGB(120,230,255),
    CRGB(200,245,255), CRGB::White,       CRGB::White,        CRGB::White,
    CRGB::White,       CRGB::White,       CRGB::White,        CRGB::White
  );

  for (int j = 0; j < ledCount; j++)
    leds[j] = ColorFromPalette(blueFire, scale8(heat[j], 240));

  if (random8() < 10) leds[random16(ledCount)] += CRGB(180, 230, 255);
}

// ---- Effect 54: Wildfire / Forest Fire ----------------------------------------
// Rolling, unpredictable wildfire. Wind modulates the whole strip at once —
// the flame surges and subsides in waves across the full length.
void effectWildfire() {
  static byte heat[500];
  static uint8_t windOffset = 0;
  uint32_t ms = millis();

  EVERY_N_MILLISECONDS(80) { windOffset = inoise8(ms / 200); }
  uint8_t windCooling = map(windOffset, 0, 255, 2, 12);

  for (int i = 0; i < ledCount; i++) {
    uint8_t cool = random8(windCooling, windCooling + ((50 * 10) / ledCount) + 2);
    heat[i] = qsub8(heat[i], cool);
  }

  for (int k = 1; k < ledCount - 1; k++)
    heat[k] = (heat[k - 1] + heat[k] + heat[k] + heat[k + 1]) / 4;

  // Multiple fuel pockets igniting across the strip
  for (int z = 0; z < 4; z++) {
    if (random8() < 70) {
      int y = random16(ledCount);
      heat[y] = qadd8(heat[y], random8(140, 255));
    }
  }

  // Wind turbulence sweeps left to right, making the fire lean
  for (int i = 0; i < ledCount; i++) {
    uint8_t turb = inoise8(i * 20 - ms / 8, ms / 12);
    heat[i] = scale8(heat[i], map(turb, 0, 255, 160, 255));
  }

  CRGBPalette16 wildPal = CRGBPalette16(
    CRGB::Black,       CRGB(20,5,0),      CRGB(60,10,0),     CRGB::Maroon,
    CRGB(180,30,0),    CRGB::OrangeRed,   CRGB(255,100,0),   CRGB(255,160,10),
    CRGB(255,200,30),  CRGB(255,230,80),  CRGB::Yellow,       CRGB::White,
    CRGB::White,       CRGB::White,       CRGB::White,        CRGB::White
  );

  for (int j = 0; j < ledCount; j++)
    leds[j] = ColorFromPalette(wildPal, scale8(heat[j], 240));

  // Flying embers — sparks anywhere on the strip
  if (random8() < 25) leds[random16(ledCount)] += CRGB(255, 120, 0);
}

// ---- Effect 55: Candle Flame --------------------------------------------------
// Intimate, slow single-candle flicker across the whole strip.
// Very gentle turbulence — the flame breathes more than it flickers.
void effectCandleFlame() {
  static byte heat[500];
  static uint8_t draughtStrength = 0;
  uint32_t ms = millis();

  // Very light cooling — candle is calm
  for (int i = 0; i < ledCount; i++)
    heat[i] = qsub8(heat[i], random8(0, ((38 * 10) / ledCount) + 2));

  for (int k = 1; k < ledCount - 1; k++)
    heat[k] = (heat[k - 1] + heat[k] + heat[k] + heat[k + 1]) / 4;

  // Soft ignition scattered across the strip
  for (int s = 0; s < 2; s++) {
    if (random8() < 70) {
      int y = random16(ledCount);
      heat[y] = qadd8(heat[y], random8(100, 190));
    }
  }

  // Slow, smooth Perlin flicker — candle sways gently
  for (int i = 0; i < ledCount; i++) {
    uint8_t sway = inoise8(i * 15, ms / 20);
    heat[i] = scale8(heat[i], map(sway, 0, 255, 185, 255));
  }

  // Occasional draught dims everything briefly
  EVERY_N_MILLISECONDS(300) {
    if (random8() < 15) draughtStrength = random8(20, 70);
    draughtStrength = qsub8(draughtStrength, 5);
  }

  CRGBPalette16 candlePal = CRGBPalette16(
    CRGB::Black,       CRGB(30,5,0),      CRGB(80,15,0),     CRGB(150,30,0),
    CRGB(220,60,0),    CRGB(255,100,5),   CRGB(255,150,20),  CRGB(255,190,50),
    CRGB(255,215,80),  CRGB(255,235,120), CRGB(255,245,180), CRGB::White,
    CRGB::White,       CRGB::White,       CRGB::White,        CRGB::White
  );

  for (int j = 0; j < ledCount; j++) {
    byte val = scale8(qsub8(heat[j], draughtStrength), 240);
    leds[j] = ColorFromPalette(candlePal, val);
  }
}

// ---- Effect 56: Campfire / Bonfire -------------------------------------------
// Big roaring campfire — rich amber-gold with heavy ignition everywhere and
// embers that pop brightly at random positions.
void effectCampfire() {
  static byte heat[500];
  uint32_t ms = millis();

  for (int i = 0; i < ledCount; i++)
    heat[i] = qsub8(heat[i], random8(1, ((65 * 10) / ledCount) + 3));

  for (int k = 1; k < ledCount - 1; k++)
    heat[k] = (heat[k - 1] + heat[k] + heat[k] + heat[k + 1]) / 4;

  // Dense, spread-out ignition — campfire burns wide
  for (int ig = 0; ig < 6; ig++) {
    if (random8() < 90) {
      int y = random16(ledCount);
      heat[y] = qadd8(heat[y], random8(150, 255));
    }
  }

  // Rumbling turbulence — the fire roars
  for (int i = 0; i < ledCount; i++) {
    uint8_t turb = inoise8(i * 22, ms / 6);
    heat[i] = scale8(heat[i], map(turb, 0, 255, 155, 255));
  }

  CRGBPalette16 campPal = CRGBPalette16(
    CRGB::Black,       CRGB(30,8,0),      CRGB::Maroon,      CRGB(160,20,0),
    CRGB::Red,         CRGB(255,60,0),    CRGB::Orange,      CRGB(255,170,0),
    CRGB(255,210,20),  CRGB(255,235,60),  CRGB::Yellow,       CRGB(255,250,150),
    CRGB::White,       CRGB::White,       CRGB::White,        CRGB::White
  );

  for (int j = 0; j < ledCount; j++)
    leds[j] = ColorFromPalette(campPal, scale8(heat[j], 240));

  // Log snap — sudden bright flash anywhere
  if (random8() < 4) leds[random16(ledCount)] = CRGB(255, 255, 200);
  // Drifting embers
  if (random8() < 20) leds[random16(ledCount)] += CRGB(255, 80, 0);
}

// ---- Effect 57: Plasma Fire ---------------------------------------------------
// Supernatural violet-to-magenta plasma. Turbulence makes it crackle and pulse
// like electricity — otherworldly but unmistakably hot.
void effectPlasmaFire() {
  static byte heat[500];
  uint32_t ms = millis();

  for (int i = 0; i < ledCount; i++)
    heat[i] = qsub8(heat[i], random8(0, ((58 * 10) / ledCount) + 2));

  for (int k = 1; k < ledCount - 1; k++)
    heat[k] = (heat[k - 1] + heat[k] + heat[k] + heat[k + 1]) / 4;

  for (int s = 0; s < 3; s++) {
    if (random8() < 85) {
      int y = random16(ledCount);
      heat[y] = qadd8(heat[y], random8(160, 255));
    }
  }

  // Fast, chaotic turbulence — plasma crackles
  for (int i = 0; i < ledCount; i++) {
    uint8_t turb = inoise8(i * 35, ms / 3);
    heat[i] = scale8(heat[i], map(turb, 0, 255, 150, 255));
  }

  CRGBPalette16 plasmaPal = CRGBPalette16(
    CRGB::Black,       CRGB(10,0,25),     CRGB(30,0,80),     CRGB(80,0,160),
    CRGB(140,0,200),   CRGB(200,0,200),   CRGB(255,0,180),   CRGB(255,40,140),
    CRGB(255,100,180), CRGB(255,160,220), CRGB::White,        CRGB::White,
    CRGB::White,       CRGB::White,       CRGB::White,        CRGB::White
  );

  for (int j = 0; j < ledCount; j++)
    leds[j] = ColorFromPalette(plasmaPal, scale8(heat[j], 240));

  // Electric discharge sparks
  if (random8() < 18) leds[random16(ledCount)] += CRGB(200, 100, 255);
}

// ---- Effect 58: Inferno -------------------------------------------------------
// Maximum-intensity furnace. Very little cooling, constant ignition — the strip
// is almost never dark. Brilliant gold-white with deep red edges.
void effectInferno() {
  static byte heat[500];
  uint32_t ms = millis();

  // Minimal cooling — this fire BURNS
  for (int i = 0; i < ledCount; i++)
    heat[i] = qsub8(heat[i], random8(0, ((35 * 10) / ledCount) + 1));

  for (int k = 1; k < ledCount - 1; k++)
    heat[k] = (heat[k - 1] + heat[k] + heat[k] + heat[k + 1]) / 4;

  // Dense ignition everywhere — the furnace never goes out
  for (int ig = 0; ig < 5; ig++) {
    int y = random16(ledCount);
    heat[y] = qadd8(heat[y], random8(200, 255));
  }

  // Violent turbulence
  for (int i = 0; i < ledCount; i++) {
    uint8_t turb = inoise8(i * 28, ms / 4);
    heat[i] = scale8(heat[i], map(turb, 0, 255, 170, 255));
  }

  CRGBPalette16 infernoPal = CRGBPalette16(
    CRGB(40,0,0),      CRGB::Maroon,      CRGB::Red,         CRGB(220,30,0),
    CRGB(255,80,0),    CRGB(255,140,0),   CRGB(255,200,0),   CRGB(255,230,50),
    CRGB(255,245,120), CRGB(255,252,200), CRGB::White,        CRGB::White,
    CRGB::White,       CRGB::White,       CRGB::White,        CRGB::White
  );

  for (int j = 0; j < ledCount; j++)
    leds[j] = ColorFromPalette(infernoPal, scale8(heat[j], 240));

  if (random8() < 35) leds[random16(ledCount)] = CRGB(255, 255, 180);
}

// ---- Effect 59: Smoldering Embers --------------------------------------------
// Dying coals — purely noise-driven, no flame physics at all.
// Each position glows independently based on Perlin noise.
void effectSmolderingEmbers() {
  static uint32_t noiseOffset = 0;
  EVERY_N_MILLISECONDS(50) { noiseOffset += 3; }

  CRGBPalette16 emberPal = CRGBPalette16(
    CRGB::Black,       CRGB(15,2,0),      CRGB(40,5,0),      CRGB(80,10,0),
    CRGB(130,20,0),    CRGB(180,35,0),    CRGB(220,60,5),    CRGB(255,90,5),
    CRGB(255,130,10),  CRGB(255,165,25),  CRGB(255,200,50),  CRGB(255,215,80),
    CRGB(255,230,120), CRGB(255,240,160), CRGB(255,248,200), CRGB::White
  );

  for (int j = 0; j < ledCount; j++) {
    uint8_t coal = inoise8(j * 40 + noiseOffset, noiseOffset / 2);
    coal = scale8(coal, 200);
    if (random8() < 5) coal = qadd8(coal, random8(40, 100));
    leds[j] = ColorFromPalette(emberPal, scale8(coal, 240));
  }
}

// ---- Effect 60: Lava Flow ----------------------------------------------------
// Slow molten lava. Perlin noise creates the hardening crust texture across the
// full strip — no single end is hotter than the other.
void effectLavaFlow() {
  static byte heat[500];
  uint32_t ms = millis();

  for (int i = 0; i < ledCount; i++)
    heat[i] = qsub8(heat[i], random8(0, ((45 * 10) / ledCount) + 1));

  for (int k = 1; k < ledCount - 1; k++)
    heat[k] = (heat[k - 1] + heat[k] + heat[k] + heat[k + 1]) / 4;

  // Lava surges from pressure pockets — random ignition anywhere
  for (int s = 0; s < 2; s++) {
    if (random8() < 50) {
      int y = random16(ledCount);
      heat[y] = qadd8(heat[y], random8(180, 255));
    }
  }

  // Hardening crust: Perlin noise darkens some patches
  for (int i = 0; i < ledCount; i++) {
    uint8_t crust = inoise8(i * 30 + ms / 20);
    if (crust < 80) heat[i] = qsub8(heat[i], 40);
  }

  // Very slow flow turbulence
  for (int i = 0; i < ledCount; i++) {
    uint8_t turb = inoise8(i * 18, ms / 25);
    heat[i] = scale8(heat[i], map(turb, 0, 255, 175, 255));
  }

  CRGBPalette16 lavaPal = CRGBPalette16(
    CRGB(5,0,0),       CRGB(25,2,0),      CRGB(70,5,0),      CRGB(130,10,0),
    CRGB(200,20,0),    CRGB(240,50,0),    CRGB(255,100,0),   CRGB(255,150,10),
    CRGB(255,200,30),  CRGB(255,230,80),  CRGB(255,245,150), CRGB::White,
    CRGB::White,       CRGB::White,       CRGB::White,        CRGB::White
  );

  for (int j = 0; j < ledCount; j++)
    leds[j] = ColorFromPalette(lavaPal, scale8(heat[j], 240));
}

// ---- Effect 61: Ember Storm --------------------------------------------------
// Gusty fire with heavy spark ejection everywhere. Wind turbulence sweeps
// the heat in waves — the whole strip participates.
void effectEmberStorm() {
  static byte heat[500];
  static uint16_t windBurstPos = 0;
  static bool burstActive = false;
  uint32_t ms = millis();

  for (int i = 0; i < ledCount; i++)
    heat[i] = qsub8(heat[i], random8(2, ((55 * 10) / ledCount) + 3));

  for (int k = 1; k < ledCount - 1; k++)
    heat[k] = (heat[k - 1] + heat[k] + heat[k] + heat[k + 1]) / 4;

  // Normal ignition spread across strip
  for (int s = 0; s < 4; s++) {
    if (random8() < 75) {
      int y = random16(ledCount);
      heat[y] = qadd8(heat[y], random8(140, 255));
    }
  }

  // Wind gust: a wave of extra heat sweeps across the strip periodically
  EVERY_N_MILLISECONDS(600) {
    if (random8() < 55) { windBurstPos = 0; burstActive = true; }
  }
  if (burstActive) {
    windBurstPos += 3;
    if (windBurstPos < (uint16_t)ledCount) {
      heat[windBurstPos] = qadd8(heat[windBurstPos], random8(80, 160));
    } else { burstActive = false; }
  }

  // Gusting turbulence — fire leans with the wind
  for (int i = 0; i < ledCount; i++) {
    uint8_t turb = inoise8(i * 22 - ms / 7, ms / 10);
    heat[i] = scale8(heat[i], map(turb, 0, 255, 145, 255));
  }

  CRGBPalette16 stormPal = CRGBPalette16(
    CRGB::Black,       CRGB(25,4,0),      CRGB::DarkRed,     CRGB(180,25,0),
    CRGB::Red,         CRGB(255,70,0),    CRGB::Orange,       CRGB(255,180,5),
    CRGB(255,215,30),  CRGB::Yellow,      CRGB(255,250,140), CRGB::White,
    CRGB::White,       CRGB::White,       CRGB::White,        CRGB::White
  );

  for (int j = 0; j < ledCount; j++)
    leds[j] = ColorFromPalette(stormPal, scale8(heat[j], 240));

  // Heavy ember ejection — sparks fly everywhere
  for (int e = 0; e < 4; e++) {
    if (random8() < 45) leds[random16(ledCount)] += CRGB(255, random8(60, 160), 0);
  }
}

// ---- Effect 62: Hearth Fire --------------------------------------------------
// Gentle, cosy hearth — smooth double-diffusion and slow Perlin sway makes the
// whole strip glow warmly and evenly, like staring into a fireplace.
void effectHearthFire() {
  static byte heat[500];
  uint32_t ms = millis();

  for (int i = 0; i < ledCount; i++)
    heat[i] = qsub8(heat[i], random8(0, ((42 * 10) / ledCount) + 2));

  // Two diffusion passes — silky smooth flame movement
  for (int pass = 0; pass < 2; pass++) {
    for (int k = 1; k < ledCount - 1; k++)
      heat[k] = (heat[k - 1] + heat[k] + heat[k] + heat[k + 1]) / 4;
  }

  // Steady ignition scattered everywhere
  for (int s = 0; s < 3; s++) {
    if (random8() < 65) {
      int y = random16(ledCount);
      heat[y] = qadd8(heat[y], random8(120, 210));
    }
  }

  // Slow, calming Perlin sway — the hearth breathes
  for (int i = 0; i < ledCount; i++) {
    uint8_t sway = inoise8(i * 18, ms / 22);
    heat[i] = scale8(heat[i], map(sway, 0, 255, 190, 255));
  }

  CRGBPalette16 hearthPal = CRGBPalette16(
    CRGB::Black,       CRGB(25,4,0),      CRGB(70,12,0),     CRGB(130,22,0),
    CRGB(200,45,0),    CRGB(240,80,5),    CRGB(255,120,10),  CRGB(255,160,20),
    CRGB(255,195,40),  CRGB(255,220,70),  CRGB(255,238,120), CRGB(255,246,175),
    CRGB(255,251,210), CRGB::White,       CRGB::White,        CRGB::White
  );

  for (int j = 0; j < ledCount; j++)
    leds[j] = ColorFromPalette(hearthPal, scale8(heat[j], 240));

  if (random8() < 8) leds[random16(ledCount)] += CRGB(255, 210, 100);
}

// Effect 21: Solid Color
void effectSolidColor() {
  // Unpack effectColor (uint32_t 0xRRGGBB) to CRGB
  CRGB color = CRGB(effectColor);
  fill_solid(leds, ledCount, color);
}


// ============================================================================
// NEW EFFECTS (33-42) - REVOLUTIONARY & COOL
// ============================================================================

// Effect 33: Plasma Waves
// Complex oscillating noise patterns
void effectPlasmaWaves() {
  uint32_t ms = millis();
  for(int i = 0; i < ledCount; i++) {
    uint8_t v = sin8(i*10 + ms/2) + cos8(i*5 - ms/3) + sin8(i*20 + ms/4);
    leds[i] = ColorFromPalette(PartyColors_p, v, 255, LINEARBLEND);
  }
}

// Effect 34: Confetti Palettes
// Random colored specks that fade, cycling palettes
void effectConfettiPalettes() {
  fadeToBlackBy(leds, ledCount, 10);
  int pos = random16(ledCount);
  leds[pos] += CHSV(millis()/50, 255, 255);
}

// Effect 35: Sinelon Dual
// Two moving dots chasing each other with trails
void effectSinelonDual() {
  fadeToBlackBy(leds, ledCount, 20);
  int pos1 = beatsin16(13, 0, ledCount-1);
  int pos2 = beatsin16(17, 0, ledCount-1);
  leds[pos1] += CRGB::Cyan;
  leds[pos2] += CRGB::Magenta;
}

// Effect 36: BPM
// Color pulses matching a specific beat per minute
void effectBPM() {
  uint8_t beat = beatsin8(62, 64, 255);
  for(int i = 0; i < ledCount; i++) {
    leds[i] = ColorFromPalette(PartyColors_p, millis()/20 + (i*2), beat-millis()/10 + (i*10));
  }
}

// Effect 37: Juggle
// Multiple colored dots weaving in and out
void effectJuggle() {
  fadeToBlackBy(leds, ledCount, 20);
  byte dothue = 0;
  for(int i = 0; i < 8; i++) {
    leds[beatsin16(i+7, 0, ledCount-1)] |= CHSV(dothue, 200, 255);
    dothue += 32;
  }
}

// Effect 38: Glitter Rainbow
// Rainbow with random white sparkles
void effectGlitterRainbow() {
  fill_rainbow(leds, ledCount, millis()/20, 7);
  if(random8() < 80) {
    leds[random16(ledCount)] += CRGB::White;
  }
}

// Effect 39: Pacific
// Gentle blue/aqua ocean waves using noise
void effectPacific() {
  CRGBPalette16 pacifica_palette_1 = 
    { 0x000507, 0x000409, 0x00030B, 0x00030D, 0x000210, 0x000212, 0x000114, 0x000117, 
      0x000019, 0x00001C, 0x000026, 0x000031, 0x00003B, 0x000046, 0x000051, 0x00005C };
  CRGBPalette16 pacifica_palette_2 = 
    { 0x000208, 0x00030E, 0x000514, 0x00061A, 0x000820, 0x000927, 0x000B2D, 0x000C33, 
      0x000E39, 0x001040, 0x001450, 0x001860, 0x001C70, 0x002080, 0x002490, 0x0028A0 };
  
  uint32_t ms = millis();
  for(int i = 0; i < ledCount; i++) {
     uint8_t index = inoise8(i*10, ms/4);
     leds[i] = ColorFromPalette(pacifica_palette_2, index, 255, LINEARBLEND);
  }
}

// Effect 40: Twinkle Fox
// Twinkling stars with specific colors
void effectTwinkleFox() {
  // Simplified implementation of TwinkleFox
  EVERY_N_MILLISECONDS(50) {
     if (random8() < 30) {
        int pos = random16(ledCount);
        if (leds[pos].getAverageLight() < 40) { // Only if dim
            leds[pos] = ColorFromPalette(FairyLight_p, random8(), 255, LINEARBLEND);
        }
     }
  }
  
  // Fade everything slowly
  for(int i=0; i<ledCount; i++) {
      leds[i].fadeToBlackBy(4);
  }
}

// Effect 41: Color Waves
// Smooth flowing color waves
void effectColorWaves() {
  static uint16_t sPseudotime = 0;
  static uint16_t sLastMillis = 0;
  static uint16_t sHue16 = 0;
 
  uint8_t bright = 255;
  uint16_t hue16 = sHue16;
  uint16_t hueinc16 = beatsin16(11, 200, 1500);
  
  uint16_t ms = millis();
  uint16_t deltams = ms - sLastMillis ;
  sLastMillis  = ms;
  sPseudotime += deltams * map(effectSpeed, 0, 255, 1, 3);
  sHue16 += deltams * beatsin8(5, 50, 255);
  
  for( int i = 0 ; i < ledCount; i++) {
    hue16 += hueinc16;
    uint8_t hue8 = hue16 / 256;
    uint16_t h16_128 = hue16 >> 7;
    if( h16_128 & 0x100) {
      hue8 = 255 - (h16_128 >> 1);
    } else {
      hue8 = h16_128 >> 1;
    }

    leds[i] = CHSV( hue8, 255, bright);
  }
}

// Effect 42: Perlin Move
// Perlin noise moving across the strip
void effectPerlinMove() {
  uint32_t ms = millis();
  int scale = 10;
  for (int i = 0; i < ledCount; i++) {
    uint8_t noise = inoise8(i * scale + ms / 3);
    leds[i] = CHSV(noise, 255, 255);
  }
}
// ============================================================================
// ORGANIC LAMP EFFECTS (43-52)
// ============================================================================

// Effect 43: Plasma Lamp (Classic)
// Smoothly shifting plasma colors using multiple noise layers
void effectPlasmaLamp() {
  uint32_t ms = millis();
  for (int i = 0; i < ledCount; i++) {
    uint8_t noise1 = inoise8(i * 15 + ms / 5, ms / 12);
    uint8_t noise2 = inoise8(ms / 10, i * 20 - ms / 8);
    uint8_t index = (noise1 + noise2) / 2;
    leds[i] = ColorFromPalette(PartyColors_p, index, 255, LINEARBLEND);
  }
}

// Effect 44: Bioluminescence
// Deep sea bioluminescent forest feel
void effectBiolume() {
  fadeToBlackBy(leds, ledCount, 10);
  uint32_t ms = millis();
  for (int i = 0; i < ledCount; i++) {
    uint8_t val = inoise8(i * 40, ms / 15);
    if (val > 180) {
      uint8_t hue = 140 + (val % 30); // Teal to Blue
      leds[i] |= CHSV(hue, 200, val - 180);
    }
  }
}

// Effect 45: Deep Sea Volcano
// Dark blues with pulsing orange/red cracks
void effectDeepSeaVolcano() {
  uint32_t ms = millis();
  CRGBPalette16 volcano_p = CRGBPalette16(
    CRGB::Black, CRGB::DarkBlue, CRGB::DarkBlue, CRGB::Blue,
    CRGB::Blue, CRGB::DarkRed, CRGB::Red, CRGB::OrangeRed,
    CRGB::Orange, CRGB::Red, CRGB::DarkRed, CRGB::Black,
    CRGB::Black, CRGB::Black, CRGB::Black, CRGB::Black
  );
  
  for (int i = 0; i < ledCount; i++) {
    uint8_t noise = inoise8(i * 25, ms / 8);
    leds[i] = ColorFromPalette(volcano_p, noise, 255, LINEARBLEND);
  }
}

// Effect 46: Magical Aurora
// Shifting green and purple ribbons
void effectMagicalAurora() {
  uint32_t ms = millis();
  for (int i = 0; i < ledCount; i++) {
    uint8_t hue = inoise8(i * 10, ms / 20);
    hue = map(hue, 0, 255, 96, 200); // Green to Purple
    uint8_t bri = inoise8(ms / 15, i * 15);
    leds[i] = CHSV(hue, 255, bri);
  }
  blur1d(leds, ledCount, 32);
}

// Effect 47: Solar Winds
// Hot gas movement simulation
void effectSolarWinds() {
  uint32_t ms = millis();
  for (int i = 0; i < ledCount; i++) {
    uint8_t val = inoise8(i * 15 - ms / 5, ms / 10);
    leds[i] = ColorFromPalette(HeatColors_p, val, 255, LINEARBLEND);
  }
}

// Effect 48: Ethereal Mist
// Soft, slow white/cyan/blue movement
void effectEtherealMist() {
  uint32_t ms = millis();
  for (int i = 0; i < ledCount; i++) {
    uint8_t n = inoise8(i * 30, ms / 30);
    uint8_t hue = 160 + (n / 8); // Cyan range
    uint8_t sat = qsub8(255, n / 2);
    leds[i] = CHSV(hue, sat, n);
  }
}

// Effect 49: Bio-Pulse
// Heartbeat-like pulsing of organic matter
void effectBioPulse() {
  uint8_t pulse = beatsin8(15, 40, 255);
  uint32_t ms = millis();
  for (int i = 0; i < ledCount; i++) {
    uint8_t n = inoise8(i * 50, ms / 10);
    leds[i] = CHSV(100, 255, scale8(n, pulse)); // Green organic pulse
  }
}

// Effect 50: Radioactive Glow
// Toxic green with bubbling effects
void effectRadioactiveGlow() {
  uint32_t ms = millis();
  for (int i = 0; i < ledCount; i++) {
    uint8_t n = inoise8(i * 40 + ms / 4, ms / 12);
    uint8_t hue = 80 + (n / 10); // Green to Lime
    leds[i] = CHSV(hue, 255, n);
    if (random8() < 2) leds[i] += CRGB::White; // Bubbles
  }
}

// Effect 51: Supernova
// Intense white center expanding outwards
void effectSupernova() {
  fadeToBlackBy(leds, ledCount, 40);
  static uint8_t stage = 0;
  uint16_t center = ledCount / 2;
  
  uint8_t radius = beatsin8(10, 0, ledCount / 2);
  for (int i = center - radius; i < center + radius; i++) {
    if (i >= 0 && i < ledCount) {
      uint8_t dist = abs(i - center);
      uint8_t bri = qsub8(255, dist * (255 / (radius + 1)));
      leds[i] |= ColorFromPalette(HeatColors_p, bri, bri, LINEARBLEND);
    }
  }
}

// Effect 52: Enchanted Stream
// Moving water with sparkling magic
void effectEnchantedStream() {
  uint32_t ms = millis();
  for (int i = 0; i < ledCount; i++) {
    uint8_t n = inoise8(i * 20 - ms / 6, ms / 20);
    leds[i] = ColorFromPalette(OceanColors_p, n, 255, LINEARBLEND);
    if (random8() < 5) leds[i] += CRGB::White; // Magic sparkles
  }
}


// ============================================================================
// HELPERS (Compat wrappers where needed)
// ============================================================================

void setAllLeds(uint32_t color) {
  fill_solid(leds, ledCount, CRGB(color));
}

uint32_t Wheel(byte WheelPos) {
  // Legacy compatibility for any missed calls
  CRGB color = CHSV(WheelPos, 255, 255);
  return ((uint32_t)color.r << 16) | ((uint32_t)color.g << 8) | color.b;
}

uint32_t colorBlend(uint32_t color1, uint32_t color2, uint8_t blend) {
    CRGB c1 = CRGB(color1);
    CRGB c2 = CRGB(color2);
    CRGB blended = nblend(c1, c2, blend);
    return ((uint32_t)blended.r << 16) | ((uint32_t)blended.g << 8) | blended.b;
}

uint32_t EffectHeatColor(uint8_t temperature) {
    CRGB color = HeatColor(temperature);
    return ((uint32_t)color.r << 16) | ((uint32_t)color.g << 8) | color.b;
}