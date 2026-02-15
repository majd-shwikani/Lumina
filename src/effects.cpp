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

// Effect 20: VERY REALISTIC FIRE
// Based on Fire2012WithPalette by Mark Kriegsman
void effectFireSimulation() {
  // Array of temperature readings at each simulation cell
  static byte heat[500]; // Fixed buffer, adjust if ledCount > 500

  // 1. Cool down every cell a little
  for( int i = 0; i < ledCount; i++) {
    heat[i] = qsub8( heat[i],  random8(0, ((55 * 10) / ledCount) + 2));
  }

  // 2. Heat from each cell drifts 'up' and diffuses a little
  for( int k= ledCount - 1; k >= 2; k--) {
    heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2] ) / 3;
  }

  // 3. Randomly ignite new 'sparks' of heat near the bottom
  if( random8() < 120 ) {
    int y = random8(7);
    if(y < ledCount) heat[y] = qadd8( heat[y], random8(160,255) );
  }

  // 4. Map from heat cells to LED colors using a realistic palette
  // Define a custom fire palette for extra realism
  // Black -> Red -> Orange -> Yellow -> White
  CRGBPalette16 firePalette = CRGBPalette16(
    CRGB::Black, CRGB::Maroon, CRGB::Red, CRGB::OrangeRed,
    CRGB::Orange, CRGB::Gold, CRGB::Yellow, CRGB::White,
    CRGB::White, CRGB::White, CRGB::White, CRGB::White,
    CRGB::White, CRGB::White, CRGB::White, CRGB::White
  );

  for( int j = 0; j < ledCount; j++) {
    // Scale the heat value from 0-255 down to 0-240
    // for best results with color palettes.
    byte colorindex = scale8( heat[j], 240);
    leds[j] = ColorFromPalette( firePalette, colorindex);
  }
  
  // Add occasional sparks that fly up faster
  if (random8() < 5) {
    int sparkPos = random8(ledCount/4);
    leds[sparkPos] += CRGB::White;
  }
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
