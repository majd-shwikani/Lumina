#include "globals.h"
#include "effects.h"
#include <Arduino.h>

uint32_t stripColor(uint8_t r, uint8_t g, uint8_t b) {
  return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

uint32_t getPixelColor(int i) {
  if (i < 0 || i >= ledCount) return 0;
  return ((uint32_t)leds[i].r << 16) | ((uint32_t)leds[i].g << 8) | leds[i].b;
}

void setPixelColor(int i, uint8_t r, uint8_t g, uint8_t b) {
  if (i >= 0 && i < ledCount) leds[i] = stripColor(r, g, b);
}

void setPixelColor(int i, uint32_t c) {
  if (i >= 0 && i < ledCount) leds[i] = c;
}

// Helper function for rainbow effect
uint32_t Wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if(WheelPos < 85) {
    return stripColor(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if(WheelPos < 170) {
    WheelPos -= 85;
    return stripColor(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return stripColor(WheelPos * 3, 255 - WheelPos * 3, 0);
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
  
  return stripColor(r, g, b);
}

// Effect 0: Rainbow cycle
void effectRainbow() {
  static uint16_t j = 0;
  for(int i = 0; i < ledCount; i++) {
    leds[i] = Wheel((i + j) & 255);
  }
  j++;
  if(j >= 256) j = 0;
}

// Effect 1: Meteor shower
void effectMeteorShower() {
  static int meteors[3] = {-50, -100, -150};
  static uint32_t meteorColors[3] = {0xFF5500, 0x00AAFF, 0xAA00FF};
  static int meteorSpeeds[3] = {3, 2, 4};
  
  for(int i = 0; i < ledCount; i++) {
    leds[i].fadeToBlackBy(40);
  }
  
  for(int m = 0; m < 3; m++) {
    meteors[m] += meteorSpeeds[m];
    for(int i = 0; i < 15; i++) {
      int pos = meteors[m] - i;
      if(pos >= 0 && pos < ledCount) {
        float intensity = 1.0 - (i * 0.07);
        uint32_t color = meteorColors[m];
        uint8_t r = ((color >> 16) & 0xFF) * intensity;
        uint8_t g = ((color >> 8) & 0xFF) * intensity;
        uint8_t b = (color & 0xFF) * intensity;
        setPixelColor(pos, r, g, b);
      }
    }
    if(meteors[m] > ledCount + 20) {
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
  
  for(int i = 0; i < ledCount; i++) {
    leds[i].fadeToBlackBy(10);
  }
  
  if(millis() - lastDrop > 150) {
    lastDrop = millis();
    int col = random(16);
    columns[col] = 1;
    columnHeights[col] = random(5, 12);
  }
  
  for(int col = 0; col < 16; col++) {
    if(columns[col] > 0) {
      for(int row = 0; row < columnHeights[col]; row++) {
        int pos = col + (row * 16);
        if(pos < ledCount) {
          float intensity = 1.0 - (row * 0.15);
          uint8_t brightness = 255 * intensity;
          if(row == 0) setPixelColor(pos, brightness, brightness, brightness);
          else setPixelColor(pos, 0, brightness, 0);
        }
      }
      columns[col]++;
      if(columns[col] > ledCount / 16 + columnHeights[col]) columns[col] = 0;
    }
  }
}

// Effect 3: Pulsing spheres
void effectPulsingSpheres() {
  static float spheres[3][3] = {{0.3, 0.2, 0.0}, {0.7, 0.5, 0.0}, {0.5, 0.8, 0.0}};
  static uint8_t sphereHues[3] = {0, 85, 170};
  
  fill_solid(leds, ledCount, stripColor(5, 5, 10));
  
  for(int s = 0; s < 3; s++) {
    spheres[s][1] += 0.02 + (s * 0.01);
    spheres[s][2] = 0.1 + 0.15 * (sin(spheres[s][1]) + 1.0) / 2.0;
    sphereHues[s] += 1;
  }
  
  for(int i = 0; i < ledCount; i++) {
    float pos = (float)i / ledCount;
    for(int s = 0; s < 3; s++) {
      float dx = pos - spheres[s][0];
      float distance = abs(dx);
      if(distance < spheres[s][2]) {
        float intensity = 1.0 - (distance / spheres[s][2]);
        intensity = intensity * intensity;
        uint32_t sphereColor = Wheel(sphereHues[s]);
        uint8_t r = ((sphereColor >> 16) & 0xFF) * intensity;
        uint8_t g = ((sphereColor >> 8) & 0xFF) * intensity;
        uint8_t b = (sphereColor & 0xFF) * intensity;
        leds[i].r = qadd8(leds[i].r, r);
        leds[i].g = qadd8(leds[i].g, g);
        leds[i].b = qadd8(leds[i].b, b);
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
    for(int i = 0; i < ledCount; i++) {
      if((i / 16) % 2 == (i % 2)) setPixelColor(i, 2, 5, 2);
      else setPixelColor(i, 1, 3, 1);
    }
    for(int bit = 0; bit < 8; bit++) {
      if(binaryValue & (1 << bit)) {
        int startPos = bit * (ledCount / 8);
        int barHeight = (bit + 1) * 2;
        for(int j = 0; j < barHeight && j < 11; j++) {
          int pos = startPos + j * 16;
          if(pos < ledCount) setPixelColor(pos, 0, 255 - (j * 20), 0);
        }
      }
    }
  }
}

// Effect 5: Vortex
void effectVortex() {
  static float angle = 0;
  static float twist = 0;
  for(int i = 0; i < ledCount; i++) {
    float pos = (float)i / ledCount;
    float vortexAngle = angle + pos * 15.0 + twist;
    float radius = pos * 8.0;
    float pattern = (sin(vortexAngle) + cos(vortexAngle + radius) + 2.0) / 4.0;
    uint32_t color = Wheel((int)(i * 3 + angle * 50) % 256);
    setPixelColor(i, ((color >> 16) & 0xFF) * pattern, ((color >> 8) & 0xFF) * pattern, (color & 0xFF) * pattern);
  }
  angle += 0.05;
  twist += 0.02;
}

// Effect 6: DNA helix
void effectDNAHelix() {
  static float phase = 0;
  static float rotation = 0;
  for(int i = 0; i < ledCount; i++) {
    float pos = (float)i / ledCount;
    float helix1 = sin(pos * 20.0 + phase);
    float helix2 = sin(pos * 20.0 + phase + 3.14159);
    float backbone = sin(pos * 5.0 + rotation) * 0.3 + 0.7;
    if(helix1 > 0.7) setPixelColor(i, 255 * backbone, 50 * backbone, 50 * backbone);
    else if(helix2 > 0.7) setPixelColor(i, 50 * backbone, 50 * backbone, 255 * backbone);
    else if(abs(helix1) < 0.2) setPixelColor(i, 200 * backbone, 200 * backbone, 0);
    else setPixelColor(i, 1, 2, 3);
  }
  phase += 0.1;
  rotation += 0.02;
}

// Effect 8: Lava lamp
void effectLavaLamp() {
  static float blobs[4][4] = {{0.2, 0.3, 0.1, 0.0}, {0.7, 0.2, 0.15, 1.57}, {0.3, 0.7, 0.12, 3.14}, {0.8, 0.6, 0.18, 4.71}};
  static float velocities[4][2] = {{0.008, 0.012}, {-0.01, 0.008}, {0.007, -0.009}, {-0.006, -0.011}};
  static uint8_t blobHues[4] = {0, 30, 60, 90};
  fill_solid(leds, ledCount, stripColor(15, 0, 25));
  for(int b = 0; b < 4; b++) {
    blobs[b][0] += velocities[b][0];
    blobs[b][1] += velocities[b][1];
    blobs[b][2] = 0.1 + 0.1 * (sin(blobs[b][3]) + 1.0) / 2.0;
    blobs[b][3] += 0.03;
    blobHues[b] += 1;
    if(blobs[b][0] < 0.1 || blobs[b][0] > 0.9) velocities[b][0] *= -1.0;
    if(blobs[b][1] < 0.1 || blobs[b][1] > 0.9) velocities[b][1] *= -1.0;
  }
  for(int i = 0; i < ledCount; i++) {
    float x = (float)(i % 16) / 16.0;
    float y = (float)(i / 16) / 11.0;
    float totalBrightness = 0;
    uint32_t blendedColor = 0;
    for(int b = 0; b < 4; b++) {
      float dx = x - blobs[b][0], dy = y - blobs[b][1];
      float distance = sqrt(dx * dx + dy * dy);
      if(distance < blobs[b][2]) {
        float brightness = pow(1.0 - (distance / blobs[b][2]), 3);
        totalBrightness += brightness;
        if(blendedColor == 0) blendedColor = Wheel(blobHues[b]);
        else blendedColor = colorBlend(blendedColor, Wheel(blobHues[b]), (uint8_t)(brightness * 128));
      }
    }
    if(totalBrightness > 0) {
      if(totalBrightness > 1.0) totalBrightness = 1.0;
      setPixelColor(i, ((blendedColor >> 16) & 0xFF) * totalBrightness, ((blendedColor >> 8) & 0xFF) * totalBrightness, (blendedColor & 0xFF) * totalBrightness);
    }
  }
}

// Effect 10: Quantum particles
void effectQuantumParticles() {
  static float particles[10][3] = {0};
  static uint8_t particleColors[10] = {0};
  static uint32_t lastSpawn = 0;
  for(int i = 0; i < ledCount; i++) {
    if(random(1000) < 2) setPixelColor(i, 50, 50, 100);
    else setPixelColor(i, 5, 5, 15);
  }
  if(millis() - lastSpawn > 200 && random(100) < 30) {
    lastSpawn = millis();
    for(int i = 0; i < 10; i++) {
      if(particles[i][0] == 0) {
        particles[i][0] = 0.01;
        particles[i][1] = random(10, 30) / 100.0;
        particles[i][2] = random(100) / 100.0 * 2 * 3.14159;
        particleColors[i] = random(256);
        break;
      }
    }
  }
  for(int i = 0; i < 10; i++) {
    if(particles[i][0] > 0) {
      particles[i][0] += particles[i][1];
      particles[i][2] += 0.2;
      int pos = particles[i][0] * ledCount;
      if(pos < ledCount) {
        for(int j = -2; j <= 2; j++) {
          int qPos = pos + j;
          if(qPos >= 0 && qPos < ledCount) {
            float prob = 1.0 / (1.0 + abs(j));
            uint8_t intensity = (uint8_t)(255 * prob * (sin(particles[i][2]) + 1.0) / 2.0);
            uint32_t color = Wheel(particleColors[i]);
            setPixelColor(qPos, ((color >> 16) & 0xFF) * intensity / 255, ((color >> 8) & 0xFF) * intensity / 255, (color & 0xFF) * intensity / 255);
          }
        }
      }
      if(particles[i][0] > 1.2) particles[i][0] = 0;
    }
  }
}

// Effect 11: Neural network
void effectNeuralNetwork() {
  static uint8_t neurons[20] = {0};
  static uint8_t connections[20][20] = {0};
  static uint32_t lastFire = 0;
  fill_solid(leds, ledCount, stripColor(1, 1, 3));
  if(millis() - lastFire > 50) {
    lastFire = millis();
    if(random(100) < 40) {
      int n = random(20);
      neurons[n] = 255;
      for(int i = 0; i < 5; i++) {
        int t = random(20);
        if(t != n) connections[n][t] = 200;
      }
    }
  }
  for(int i = 0; i < 20; i++) {
    int nPos = i * (ledCount / 20);
    if(neurons[i] > 0) {
      setPixelColor(nPos, neurons[i], neurons[i] / 2, neurons[i]);
      neurons[i] = neurons[i] * 8 / 10;
      if(neurons[i] < 5) neurons[i] = 0;
    }
    for(int j = 0; j < 20; j++) {
      if(connections[i][j] > 0) {
        int sPos = i * (ledCount / 20), ePos = j * (ledCount / 20);
        int steps = abs(ePos - sPos);
        for(int k = 0; k <= steps; k++) {
          int pos = sPos + (ePos - sPos) * k / steps;
          if(pos < ledCount) leds[pos].b = qadd8(leds[pos].b, connections[i][j] / 3);
        }
        connections[i][j] = connections[i][j] * 9 / 10;
        if(connections[i][j] < 5) connections[i][j] = 0;
      }
    }
  }
}

// Effect 12: Galaxy spin
void effectGalaxySpin() {
  static float angle = 0;
  for(int i = 0; i < ledCount; i++) {
    float pos = (float)i / ledCount;
    float arm1 = sin(pos * 15.0 + angle) * 0.5 + 0.5;
    float arm2 = sin(pos * 15.0 + angle + 2.094) * 0.5 + 0.5;
    float arm3 = sin(pos * 15.0 + angle + 4.189) * 0.5 + 0.5;
    float br = max(max(arm1, arm2), arm3);
    br = br * br;
    float core = 1.0 - abs(pos - 0.5) * 2.0;
    if(core > 0) br = max(br, core * core);
    uint32_t color = Wheel(170 + (pos * 50));
    if(random(1000) < 3 && br < 0.3) setPixelColor(i, 255, 255, 255);
    else setPixelColor(i, ((color >> 16) & 0xFF) * br, ((color >> 8) & 0xFF) * br, (color & 0xFF) * br);
  }
  angle += 0.03;
}

// Effect 13: Crystal growth
void effectCrystalGrowth() {
  static uint8_t crystals[200] = {0};
  static uint8_t crystalColors[200] = {0};
  static uint32_t lastGrowth = 0;
  static uint16_t activeSeeds = 0;
  if(activeSeeds < 5 && random(100) < 10) {
    int p = random(ledCount);
    if(crystals[p] == 0) { crystals[p] = 1; crystalColors[p] = random(256); activeSeeds++; }
  }
  if(millis() - lastGrowth > 100) {
    lastGrowth = millis();
    for(int i = 0; i < ledCount; i++) {
      if(crystals[i] > 0 && crystals[i] < 255) {
        for(int dir = -1; dir <= 1; dir += 2) {
          int n = i + dir;
          if(n >= 0 && n < ledCount && random(100) < 30 && crystals[n] == 0) {
            crystals[n] = 1; crystalColors[n] = crystalColors[i]; activeSeeds++;
          }
        }
        crystals[i] = qadd8(crystals[i], 5);
      }
    }
  }
  for(int i = 0; i < ledCount; i++) {
    if(crystals[i] > 0) {
      uint32_t c = Wheel(crystalColors[i]);
      setPixelColor(i, ((c >> 16) & 0xFF) * crystals[i] / 255, ((c >> 8) & 0xFF) * crystals[i] / 255, (c & 0xFF) * crystals[i] / 255);
    } else setPixelColor(i, 0);
  }
  if(activeSeeds >= ledCount * 0.8 && random(100) < 5) { memset(crystals, 0, 200); activeSeeds = 0; }
}

// Effect 14: Lightning storm
void effectLightningStorm() {
  static uint8_t lightning[200] = {0};
  static uint32_t lastStrike = 0;
  static uint8_t strikeActive = 0, flash = 0;
  for(int i = 0; i < ledCount; i++) setPixelColor(i, 10 + random(10), 10 + random(10), 20 + random(10));
  if(!strikeActive && millis() - lastStrike > 1000 && random(100) < 10) {
    strikeActive = 1; flash = 255; lastStrike = millis();
    memset(lightning, 0, 200);
    int p = random(ledCount);
    for(int i = 0; i < 20; i++) {
      if(p >= 0 && p < ledCount) { lightning[p] = 255; p += random(-2, 3); }
    }
  }
  if(strikeActive) {
    for(int i = 0; i < ledCount; i++) if(lightning[i] > 0) { setPixelColor(i, flash, flash, 255); lightning[i] = lightning[i] * 7 / 8; }
    flash = flash * 8 / 10;
    if(flash < 10) strikeActive = 0;
  }
}

// Effect 15: Ocean depth
void effectOceanDepth() {
  static float wavePhase = 0;
  for(int i = 0; i < ledCount; i++) {
    float depth = (float)i / ledCount;
    float w1 = sin(i * 0.1 + wavePhase) * 0.3 + 0.7;
    float w2 = sin(i * 0.05 + wavePhase * 0.7) * 0.2 + 0.8;
    uint8_t g = (uint8_t)((50 * (1.0 - depth) + 100 * depth) * w2);
    uint8_t b = (uint8_t)((100 + 155 * depth) * (w1 + w2) / 2.0);
    setPixelColor(i, 0, g, b);
  }
  wavePhase += 0.05;
}

// Effect 16: Northern lights
void effectNorthernLights() {
  static float p1 = 0, p2 = 0, p3 = 0;
  for(int i = 0; i < ledCount; i++) {
    float pos = (float)i / ledCount;
    float a1 = sin(pos * 8.0 + p1) * 0.5 + 0.5;
    float a2 = sin(pos * 12.0 + p2) * 0.3 + 0.7;
    float a3 = sin(pos * 6.0 + p3) * 0.4 + 0.6;
    float br = pow((a1 + a2 + a3) / 3.0, 2);
    setPixelColor(i, (uint8_t)(50 * br + 100 * a3), (uint8_t)(200 * br + 50 * a1), (uint8_t)(150 * br + 100 * a2));
  }
  p1 += 0.02; p2 += 0.015; p3 += 0.025;
}

// Effect 17: Time tunnel
void effectTimeTunnel() {
  static float depth = 0, rot = 0;
  for(int i = 0; i < ledCount; i++) {
    float pos = (float)i / ledCount;
    float val = (sin(rot + pos * 20.0) * cos(rot + pos * 20.0 * 2.0) * (0.5 + 0.3 * sin(pos * 10.0 + depth)) + 1.0) / 2.0;
    uint32_t c = Wheel((uint8_t)(depth * 50 + pos * 100) % 256);
    setPixelColor(i, ((c >> 16) & 0xFF) * val, ((c >> 8) & 0xFF) * val, (c & 0xFF) * val);
  }
  depth += 0.05; rot += 0.03;
}

// Effect 18: Cyber city
void effectCyberCity() {
  static uint8_t scan = 0;
  FastLED.clear();
  for(int col = 0; col < 16; col++) {
    for(int row = 0; row < 8; row++) {
      int pos = col + row * 16;
      if(pos < ledCount) {
        if(random(100) < 20) setPixelColor(pos, col % 2 ? 0xFF : 0, col % 3 ? 0xFF : 0, 0xFF);
        else setPixelColor(pos, 5, 5, 10);
      }
    }
  }
  scan = (scan + 1) % ledCount;
  if(scan < ledCount) setPixelColor(scan, 0, 255, 0);
}

// Effect 19: Solar flare
void effectSolarFlare() {
  static float phase = 0;
  for(int i = 0; i < ledCount; i++) {
    float turb = sin((float)i / ledCount * 20.0 + phase) * 0.3 + 0.7;
    setPixelColor(i, 255 * turb, 100 * turb, 50 * turb);
  }
  phase += 0.04;
}

// Effect 20: Realistic fire simulation
void effectFireSimulation() {
  for (int i = 0; i < ledCount; i++) {
    int f = random(0, 150);
    setPixelColor(i, max(0, 255 - f), max(0, 100 - f / 2), 0);
  }
}

// Effect 21: Solid Color
void effectSolidColor() {
  fill_solid(leds, ledCount, effectColor);
}

// Effect 31: Energy Orbits
void effectEnergyOrbits() {
  static float ang = 0;
  fill_solid(leds, ledCount, stripColor(2, 2, 8));
  for(int o = 0; o < 3; o++) {
    float r = 0.3 + 0.1 * o;
    float a = (ang + o * 120) * 3.14159 / 180.0;
    int p = (0.5 + r * cos(a)) * ledCount;
    if(p >= 0 && p < ledCount) setPixelColor(p, Wheel(o * 85));
  }
  ang += 5;
}
