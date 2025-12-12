#include "effects.h"
#include <Arduino.h>


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
// Effect 22: Frequency Response
void effectFrequencyResponse() {
  // Update frequency detection
  updateFrequencyDetection();
  
  // Check if the detected signal is strong enough (not noise)
  // Adjust this threshold as needed for your environment
  if (frequencyMagnitude > 2000.0) {
    // Valid frequency detected - map to color
    double freq = detectedFrequency;
    uint32_t freqColor;
    
    // Frequency to color mapping (included directly in the function)
    if (freq < 200) {
      freqColor = strip.Color(255, 0, 0);    // Red - low frequencies
    } else if (freq < 400) {
      freqColor = strip.Color(255, 128, 0);  // Orange
    } else if (freq < 600) {
      freqColor = strip.Color(255, 255, 0);  // Yellow
    } else if (freq < 800) {
      freqColor = strip.Color(128, 255, 0);  // Lime
    } else if (freq < 1000) {
      freqColor = strip.Color(0, 255, 0);    // Green
    } else if (freq < 1200) {
      freqColor = strip.Color(0, 255, 128);  // Teal
    } else if (freq < 1400) {
      freqColor = strip.Color(0, 255, 255);  // Cyan
    } else if (freq < 1600) {
      freqColor = strip.Color(0, 128, 255);  // Light Blue
    } else if (freq < 1800) {
      freqColor = strip.Color(0, 0, 255);    // Blue
    } else if (freq < 2000) {
      freqColor = strip.Color(128, 0, 255);  // Purple
    } else {
      freqColor = strip.Color(255, 0, 255);  // Magenta - high frequencies
    }
    
    // Display the color on all LEDs
    for(int i = 0; i < strip.numPixels(); i++) {
      strip.setPixelColor(i, freqColor);
    }
    
    // Optional: Print frequency for debugging
    // Serial.printf("Frequency: %.0f Hz, Magnitude: %.0f\n", detectedFrequency, frequencyMagnitude);
  } else {
    // Too noisy or no clear frequency - turn off LEDs
    for(int i = 0; i < strip.numPixels(); i++) {
      strip.setPixelColor(i, 0, 0, 0);
    }
    
    // Optional: Print noise floor message
    // Serial.println("Noise floor - LEDs off");
  }
}
// Effect 23: Piano Tiles - Multi-Frequency Spectrum Analyzer
void effectPianoTiles() {
  const int TOTAL_LEDS = strip.numPixels();
    const int LEDS_PER_BAND = TOTAL_LEDS / NUM_FREQ_BANDS;
  // Update multi-frequency detection
  updateFrequencyDetection();
  
  // Check if any frequency band has significant signal
  bool hasSignal = false;
  for (int band = 0; band < NUM_FREQ_BANDS; band++) {
    if (bandMagnitudes[band] > frequencyThreshold) {
      hasSignal = true;
      break;
    }
  }
  
  if (hasSignal) {
    const int TOTAL_LEDS = strip.numPixels();
    const int LEDS_PER_BAND = TOTAL_LEDS / NUM_FREQ_BANDS;
    
    // Colors for each frequency band (rainbow spectrum from low to high freq)
    const uint32_t bandColors[NUM_FREQ_BANDS] = {
      strip.Color(255, 0, 0),     // Red - lowest frequencies
      strip.Color(255, 128, 0),   // Orange
      strip.Color(255, 255, 0),   // Yellow
      strip.Color(128, 255, 0),   // Lime
      strip.Color(0, 255, 0),     // Green
      strip.Color(0, 255, 128),   // Teal
      strip.Color(0, 255, 255),   // Cyan
      strip.Color(0, 128, 255)    // Blue - highest frequencies
    };
    
    // Clear all LEDs first
    for(int i = 0; i < TOTAL_LEDS; i++) {
      strip.setPixelColor(i, 0, 0, 0);
    }
    
    // Light up LEDs for each active frequency band
    for (int band = 0; band < NUM_FREQ_BANDS; band++) {
      if (bandMagnitudes[band] > frequencyThreshold) {
        // Calculate LED positions for this band
        // Lower frequencies (lower bands) use higher LED positions (like piano keys)
        int bandPosition = NUM_FREQ_BANDS - 1 - band; // Reverse mapping
        int startLed = bandPosition * LEDS_PER_BAND;
        
        // Calculate brightness based on magnitude (normalized)
        float brightness = bandMagnitudes[band] / 10000.0; // Adjust divisor based on your mic sensitivity
        if (brightness > 1.0) brightness = 1.0;
        if (brightness < 0.1) brightness = 0.1; // Minimum visibility
        
        // Light up the LEDs for this frequency band
        for (int i = 0; i < LEDS_PER_BAND && (startLed + i) < TOTAL_LEDS; i++) {
          uint32_t baseColor = bandColors[band];
          uint8_t r = ((baseColor >> 16) & 0xFF) * brightness;
          uint8_t g = ((baseColor >> 8) & 0xFF) * brightness;
          uint8_t b = (baseColor & 0xFF) * brightness;
          strip.setPixelColor(startLed + i, r, g, b);
        }
      }
    }
    
    // Optional: Debug output
    /*
    Serial.print("Spectrum: ");
    for (int band = 0; band < NUM_FREQ_BANDS; band++) {
      Serial.printf("[%d]:%.0f ", band, bandMagnitudes[band]);
    }
    Serial.println();
    */
    
  } else {
    // No significant frequencies detected - turn off all LEDs
    for(int i = 0; i < strip.numPixels(); i++) {
      strip.setPixelColor(i, 0, 0, 0);
    }
  }
}
// Effect 24: Piano Tiles - Frequency Bars (shows intensity as height)
void effectPianoTilesBars() {
  const int TOTAL_LEDS = strip.numPixels();
    const int LEDS_PER_BAND = TOTAL_LEDS / NUM_FREQ_BANDS;
  // Update multi-frequency detection
  updateFrequencyDetection();
  
  // Check if any frequency band has significant signal
  bool hasSignal = false;
  for (int band = 0; band < NUM_FREQ_BANDS; band++) {
    if (bandMagnitudes[band] > frequencyThreshold) {
      hasSignal = true;
      break;
    }
  }
  
  if (hasSignal) {
    const int TOTAL_LEDS = strip.numPixels();
    const int LEDS_PER_BAND = TOTAL_LEDS / NUM_FREQ_BANDS;
    const int MAX_BAR_HEIGHT = LEDS_PER_BAND; // Maximum bars can use all LEDs in their band
    
    // Colors for each frequency band
    const uint32_t bandColors[NUM_FREQ_BANDS] = {
      strip.Color(255, 0, 0),     // Red
      strip.Color(255, 128, 0),   // Orange
      strip.Color(255, 255, 0),   // Yellow
      strip.Color(128, 255, 0),   // Lime
      strip.Color(0, 255, 0),     // Green
      strip.Color(0, 255, 128),   // Teal
      strip.Color(0, 255, 255),   // Cyan
      strip.Color(0, 128, 255)    // Blue
    };
    
    // Clear all LEDs
    for(int i = 0; i < TOTAL_LEDS; i++) {
      strip.setPixelColor(i, 0, 0, 0);
    }
    
    // Create bar graph for each frequency band
    for (int band = 0; band < NUM_FREQ_BANDS; band++) {
      if (bandMagnitudes[band] > frequencyThreshold) {
        // Calculate bar height based on magnitude
        int barHeight = (bandMagnitudes[band] / 15000.0) * MAX_BAR_HEIGHT; // Adjust divisor as needed
        if (barHeight > MAX_BAR_HEIGHT) barHeight = MAX_BAR_HEIGHT;
        if (barHeight < 1) barHeight = 1;
        
        // Calculate starting position (bars grow from bottom)
        int bandPosition = NUM_FREQ_BANDS - 1 - band; // Reverse for piano layout
        int baseLed = bandPosition * LEDS_PER_BAND;
        
        // Draw the bar from bottom to top
        for (int height = 0; height < barHeight && height < LEDS_PER_BAND; height++) {
          int ledIndex = baseLed + (LEDS_PER_BAND - 1 - height); // Bottom to top
          if (ledIndex < TOTAL_LEDS) {
            strip.setPixelColor(ledIndex, bandColors[band]);
          }
        }
      }
    }
    
  } else {
    // No significant frequencies - turn off LEDs
    for(int i = 0; i < strip.numPixels(); i++) {
      strip.setPixelColor(i, 0, 0, 0);
    }
  }
}
void effectFrequencySpectrum() {
  updateFrequencyDetection();
  
  const int TOTAL_LEDS = strip.numPixels();
  const int LEDS_PER_BAND = TOTAL_LEDS / NUM_FREQ_BANDS;
  
  // Clear strip with dark background
  for (int i = 0; i < TOTAL_LEDS; i++) {
    strip.setPixelColor(i, 2, 2, 5);
  }
  
  // Color palette for each frequency band
  const uint32_t bandColors[NUM_FREQ_BANDS] = {
    strip.Color(255, 0, 0),      // Red - Bass
    strip.Color(255, 80, 0),     // Orange
    strip.Color(255, 255, 0),    // Yellow
    strip.Color(100, 255, 0),    // Lime
    strip.Color(0, 255, 0),      // Green
    strip.Color(0, 255, 150),    // Cyan
    strip.Color(0, 100, 255),    // Blue
    strip.Color(200, 0, 255)     // Magenta - Treble
  };
  
  // Draw frequency bars with gradient effect
  for (int band = 0; band < NUM_FREQ_BANDS; band++) {
    double magnitude = bandMagnitudes[band];
    int startLed = band * LEDS_PER_BAND;
    
    // Calculate bar height based on magnitude
    int barHeight = (int)(magnitude * LEDS_PER_BAND * 1.5);
    if (barHeight > LEDS_PER_BAND) barHeight = LEDS_PER_BAND;
    
    // Draw bar from bottom to top with gradient
    for (int i = 0; i < barHeight; i++) {
      int ledPos = startLed + (LEDS_PER_BAND - 1 - i);
      if (ledPos < TOTAL_LEDS) {
        // Gradient brightness (brighter at top)
        float brightness = 0.3 + (0.7 * i / (float)barHeight);
        
        uint32_t color = bandColors[band];
        uint8_t r = ((color >> 16) & 0xFF) * brightness;
        uint8_t g = ((color >> 8) & 0xFF) * brightness;
        uint8_t b = (color & 0xFF) * brightness;
        
        strip.setPixelColor(ledPos, r, g, b);
      }
    }
    
    // Draw peak indicator (brighter accent at top of bar)
    if (barHeight > 0) {
      int peakLed = startLed + (LEDS_PER_BAND - 1 - (barHeight - 1));
      if (peakLed < TOTAL_LEDS) {
        strip.setPixelColor(peakLed, 255, 255, 255);
      }
    }
  }
}

// Effect 26: Reactive Waveform - Draws audio signal as waveform patterns
void effectReactiveWaveform() {
  updateFrequencyDetection();
  
  const int TOTAL_LEDS = strip.numPixels();
  static float waveOffset = 0;
  
  // Clear with very dark background
  for (int i = 0; i < TOTAL_LEDS; i++) {
    strip.setPixelColor(i, 1, 1, 3);
  }
  
  // Draw waveform based on band magnitudes
  for (int i = 0; i < TOTAL_LEDS; i++) {
    // Map LED position to frequency band
    int band = (i * NUM_FREQ_BANDS) / TOTAL_LEDS;
    if (band >= NUM_FREQ_BANDS) band = NUM_FREQ_BANDS - 1;
    
    // Get magnitude for this band
    double magnitude = bandMagnitudes[band];
    
    // Create oscillating waveform
    float wave = sin(waveOffset + (i * 0.1)) * magnitude;
    wave = (wave + 1.0) / 2.0; // Normalize to 0-1
    
    // Color based on frequency band
    uint8_t hue = (band * 255) / NUM_FREQ_BANDS;
    uint32_t color = Wheel(hue);
    
    uint8_t r = ((color >> 16) & 0xFF) * wave;
    uint8_t g = ((color >> 8) & 0xFF) * wave;
    uint8_t b = (color & 0xFF) * wave;
    
    strip.setPixelColor(i, r, g, b);
  }
  
  waveOffset += 0.3 * globalAudioLevel;
}

// Effect 27: Beat Pulse - Reacts to bass beats with expanding pulse
void effectBeatPulse() {
  updateFrequencyDetection();
  
  const int TOTAL_LEDS = strip.numPixels();
  static float pulseCenter = 0;
  static float pulseWidth = 0;
  static unsigned long lastBeatTime = 0;
  
  // Background
  for (int i = 0; i < TOTAL_LEDS; i++) {
    strip.setPixelColor(i, 3, 1, 8);
  }
  
  // Start new pulse on beat
  if (beatDetected) {
    pulseCenter = TOTAL_LEDS / 2;
    pulseWidth = 1;
    lastBeatTime = millis();
  }
  
  // Expand pulse
  if (millis() - lastBeatTime < 500) {
    pulseWidth += 2.0 + (beatEnergy * 5.0);
    
    for (int i = 0; i < TOTAL_LEDS; i++) {
      float distance = abs(i - pulseCenter);
      if (distance < pulseWidth) {
        // Color intensity decreases with distance from center
        float intensity = 1.0 - (distance / pulseWidth);
        intensity = intensity * intensity; // Quadratic falloff
        
        // Color transitions based on beat energy
        uint8_t r = (uint8_t)(200 * intensity * beatEnergy + 100 * intensity * (1 - beatEnergy));
        uint8_t g = (uint8_t)(50 * intensity);
        uint8_t b = (uint8_t)(150 * intensity);
        
        strip.setPixelColor(i, r, g, b);
      }
    }
  }
}

// Effect 28: Frequency Bloom - Frequencies bloom outward from center
void effectFrequencyBloom() {
  updateFrequencyDetection();
  
  const int TOTAL_LEDS = strip.numPixels();
  const int CENTER = TOTAL_LEDS / 2;
  static float bloomPhase[NUM_FREQ_BANDS] = {0};
  
  // Dark background
  for (int i = 0; i < TOTAL_LEDS; i++) {
    strip.setPixelColor(i, 2, 1, 4);
  }
  
  // Each band blooms outward from center
  for (int band = 0; band < NUM_FREQ_BANDS; band++) {
    double magnitude = bandMagnitudes[band];
    
    if (magnitude > 0.1) {
      bloomPhase[band] = (bloomPhase[band] + 0.2 * magnitude) > 360 ? 0 : bloomPhase[band] + (0.2 * magnitude);
      
      // Distance from center that this band reaches
      int bloomDistance = (int)((magnitude + 0.2) * (TOTAL_LEDS / 2));
      
      // Draw bloom points left and right of center
      for (int offset = 0; offset < bloomDistance; offset++) {
        int leftPos = CENTER - offset;
        int rightPos = CENTER + offset;
        
        if (leftPos >= 0) {
          float falloff = 1.0 - (offset / (float)bloomDistance);
          falloff = falloff * falloff; // Smooth falloff
          
          uint8_t hue = (band * 255) / NUM_FREQ_BANDS;
          uint32_t color = Wheel(hue);
          
          uint8_t r = ((color >> 16) & 0xFF) * falloff;
          uint8_t g = ((color >> 8) & 0xFF) * falloff;
          uint8_t b = (color & 0xFF) * falloff;
          
          strip.setPixelColor(leftPos, r, g, b);
        }
        
        if (rightPos < TOTAL_LEDS) {
          strip.setPixelColor(rightPos, 
            ((Wheel((band * 255) / NUM_FREQ_BANDS) >> 16) & 0xFF) * (1.0 - (offset / (float)bloomDistance)),
            ((Wheel((band * 255) / NUM_FREQ_BANDS) >> 8) & 0xFF) * (1.0 - (offset / (float)bloomDistance)),
            (Wheel((band * 255) / NUM_FREQ_BANDS) & 0xFF) * (1.0 - (offset / (float)bloomDistance))
          );
        }
      }
    }
  }
}

// Effect 29: Audio-Reactive Fire - Fire effect that reacts to music intensity
void effectAudioReactiveFire() {
  updateFrequencyDetection();
  
  const int TOTAL_LEDS = strip.numPixels();
  static uint8_t fireMap[256];
  static unsigned long lastUpdate = 0;
  
  if (millis() - lastUpdate < 30) return;
  lastUpdate = millis();
  
  // Initialize fire map
  if (fireMap[0] == 0) {
    memset(fireMap, 0, sizeof(fireMap));
  }
  
  // Add new fire at bottom based on audio
  fireMap[0] = 100 + (uint8_t)(bassLevel * 155);
  
  // Propagate fire upward
  for (int i = TOTAL_LEDS - 1; i > 0; i--) {
    fireMap[i] = fireMap[i - 1];
  }
  
  // Draw fire with color gradient
  for (int i = 0; i < TOTAL_LEDS; i++) {
    uint8_t heat = fireMap[i];
    uint32_t color;
    
    if (heat < 85) {
      // Black to red
      color = strip.Color(heat * 3, 0, 0);
    } else if (heat < 170) {
      // Red to yellow
      color = strip.Color(255, (heat - 85) * 3, 0);
    } else {
      // Yellow to white
      color = strip.Color(255, 255, (heat - 170) * 3);
    }
    
    strip.setPixelColor(i, color);
  }
}

// Effect 30: Musical Rainbow - Colors shift with frequency content
void effectMusicalRainbow() {
  updateFrequencyDetection();
  
  const int TOTAL_LEDS = strip.numPixels();
  static float hueShift = 0;
  
  // Base hue shifts with treble frequencies
  hueShift = (hueShift + 2 + (trebleLevel * 5)) > 360 ? 0 : hueShift + 2 + (trebleLevel * 5);
  
  for (int i = 0; i < TOTAL_LEDS; i++) {
    float pos = (float)i / TOTAL_LEDS;
    
    // Map position to frequency band
    int band = (int)(pos * NUM_FREQ_BANDS);
    if (band >= NUM_FREQ_BANDS) band = NUM_FREQ_BANDS - 1;
    
    // Base hue from position
    uint8_t baseHue = (pos * 255) + hueShift;
    uint32_t color = Wheel(baseHue);
    
    // Brightness from audio magnitude
    double brightness = bandMagnitudes[band] + 0.2;
    if (brightness > 1.0) brightness = 1.0;
    
    uint8_t r = ((color >> 16) & 0xFF) * brightness;
    uint8_t g = ((color >> 8) & 0xFF) * brightness;
    uint8_t b = (color & 0xFF) * brightness;
    
    strip.setPixelColor(i, r, g, b);
  }
}

// Effect 31: Reactive Strobe - Strobes based on beat intensity
void effectReactiveStrobe() {
  updateFrequencyDetection();
  
  const int TOTAL_LEDS = strip.numPixels();
  static unsigned long lastStrobeTime = 0;
  static bool strobeOn = false;
  
  // Strobe frequency based on beat energy
  int strobePeriod = (int)(100 - (beatEnergy * 80));
  if (strobePeriod < 20) strobePeriod = 20;
  
  // Toggle strobe state
  if (millis() - lastStrobeTime > strobePeriod) {
    lastStrobeTime = millis();
    strobeOn = !strobeOn;
  }
  
  // Strobe color based on audio frequency content
  uint32_t strobeColor;
  
  if (bassLevel > midLevel && bassLevel > trebleLevel) {
    strobeColor = strip.Color(255, 0, 0);    // Red for bass
  } else if (midLevel > bassLevel && midLevel > trebleLevel) {
    strobeColor = strip.Color(0, 255, 0);    // Green for mids
  } else {
    strobeColor = strip.Color(0, 0, 255);    // Blue for treble
  }
  
  if (strobeOn) {
    for (int i = 0; i < TOTAL_LEDS; i++) {
      strip.setPixelColor(i, strobeColor);
    }
  } else {
    // Dim strobe when off
    for (int i = 0; i < TOTAL_LEDS; i++) {
      uint8_t r = ((strobeColor >> 16) & 0xFF) / 10;
      uint8_t g = ((strobeColor >> 8) & 0xFF) / 10;
      uint8_t b = (strobeColor & 0xFF) / 10;
      strip.setPixelColor(i, r, g, b);
    }
  }
}

// Effect 32: Guitar Visualization - Optimized for guitar frequency ranges
void effectGuitarVisualizer() {
  updateFrequencyDetection();
  
  const int TOTAL_LEDS = strip.numPixels();
  const int LEDS_PER_BAND = TOTAL_LEDS / NUM_FREQ_BANDS;
  
  // Dark background
  for (int i = 0; i < TOTAL_LEDS; i++) {
    strip.setPixelColor(i, 1, 2, 2);
  }
  
  // Guitar color palette (warm tones)
  const uint32_t guitarColors[NUM_FREQ_BANDS] = {
    strip.Color(139, 69, 19),     // Dark brown - low E
    strip.Color(184, 115, 51),    // Brown - A
    strip.Color(210, 140, 60),    // Light brown - D
    strip.Color(218, 165, 32),    // Goldenrod - G
    strip.Color(255, 165, 0),     // Orange - B
    strip.Color(255, 200, 100),   // Light orange - high E
    strip.Color(255, 220, 130),   // Light tan - harmonics 1
    strip.Color(255, 240, 160)    // Light cream - harmonics 2
  };
  
  // Draw reactive bars with smooth animation
  for (int band = 0; band < NUM_FREQ_BANDS; band++) {
    double magnitude = bandMagnitudes[band];
    int startLed = band * LEDS_PER_BAND;
    
    // Calculate bar height with compression
    int barHeight = (int)(magnitude * LEDS_PER_BAND * 1.3);
    if (barHeight > LEDS_PER_BAND) barHeight = LEDS_PER_BAND;
    
    // Draw bar with smooth gradient
    for (int i = 0; i < barHeight; i++) {
      int ledPos = startLed + (LEDS_PER_BAND - 1 - i);
      if (ledPos < TOTAL_LEDS) {
        float brightness = 0.2 + (0.8 * i / (float)(barHeight + 1));
        
        uint32_t color = guitarColors[band];
        uint8_t r = ((color >> 16) & 0xFF) * brightness;
        uint8_t g = ((color >> 8) & 0xFF) * brightness;
        uint8_t b = (color & 0xFF) * brightness;
        
        strip.setPixelColor(ledPos, r, g, b);
      }
    }
  }
}

// Effect 33: Cascading Frequency - Frequencies cascade down like a waterfall
void effectCascadingFrequency() {
  updateFrequencyDetection();
  
  const int TOTAL_LEDS = strip.numPixels();
  const int ROWS = 16;
  const int COLS = TOTAL_LEDS / ROWS;
  
  static uint8_t cascade[16][32];
  
  // Shift cascade down
  for (int row = ROWS - 1; row > 0; row--) {
    for (int col = 0; col < COLS; col++) {
      cascade[row][col] = cascade[row - 1][col];
    }
  }
  
  // Add new data at top based on frequency bands
  for (int col = 0; col < COLS && col < NUM_FREQ_BANDS; col++) {
    cascade[0][col] = (uint8_t)(bandMagnitudes[col] * 255);
  }
  
  // Draw cascade
  for (int row = 0; row < ROWS; row++) {
    for (int col = 0; col < COLS; col++) {
      int ledPos = col + (row * COLS);
      if (ledPos < TOTAL_LEDS) {
        uint8_t intensity = cascade[row][col];
        uint8_t hue = (col * 255) / COLS;
        uint32_t color = Wheel(hue);
        
        uint8_t r = ((color >> 16) & 0xFF) * intensity / 255;
        uint8_t g = ((color >> 8) & 0xFF) * intensity / 255;
        uint8_t b = (color & 0xFF) * intensity / 255;
        
        strip.setPixelColor(ledPos, r, g, b);
      }
    }
  }
}

// Effect 34: Energy Orbits - Orbiting particles driven by audio energy
void effectEnergyOrbits() {
  updateFrequencyDetection();
  
  const int TOTAL_LEDS = strip.numPixels();
  const int NUM_ORBITS = 3;
  static float orbitAngles[NUM_ORBITS] = {0, 120, 240};
  
  // Clear background
  for (int i = 0; i < TOTAL_LEDS; i++) {
    strip.setPixelColor(i, 2, 2, 8);
  }
  
  // Update orbit speeds based on frequency
  float speedMultiplier = 1.0 + (globalAudioLevel * 3.0);
  orbitAngles[0] = fmod(orbitAngles[0] + 3.0 * speedMultiplier, 360);
  orbitAngles[1] = fmod(orbitAngles[1] + 2.5 * speedMultiplier, 360);
  orbitAngles[2] = fmod(orbitAngles[2] + 2.0 * speedMultiplier, 360);
  
  // Draw orbiting particles
  for (int orbit = 0; orbit < NUM_ORBITS; orbit++) {
    // Orbit radius depends on audio levels
    float radius = 0.3 + (0.2 * (orbit / (float)NUM_ORBITS)) + (bassLevel * 0.15);
    
    // Calculate particle position on circle
    float angle = orbitAngles[orbit] * 3.14159 / 180.0;
    float posX = 0.5 + radius * cos(angle);
    float posY = 0.5 + radius * sin(angle);
    
    // Project to LED position
    int ledPos = posX * TOTAL_LEDS;
    if (ledPos < 0) ledPos = 0;
    if (ledPos >= TOTAL_LEDS) ledPos = TOTAL_LEDS - 1;
    
    // Particle color and brightness
    uint8_t hue = (orbit * 85);
    uint32_t color = Wheel(hue);
    float brightness = 0.5 + (0.5 * globalAudioLevel);
    
    uint8_t r = ((color >> 16) & 0xFF) * brightness;
    uint8_t g = ((color >> 8) & 0xFF) * brightness;
    uint8_t b = (color & 0xFF) * brightness;
    
    strip.setPixelColor(ledPos, r, g, b);
  }
}

// Effect 35: Audio Ripples - Sound creates ripples that expand outward
void effectAudioRipples() {
  updateFrequencyDetection();
  
  const int TOTAL_LEDS = strip.numPixels();
  const int CENTER = TOTAL_LEDS / 2;
  static float ripples[5] = {0};
  static float rippleSpeeds[5] = {2, 2.5, 3, 3.5, 4};
  static unsigned long lastRippleTime = 0;
  
  // Clear background
  for (int i = 0; i < TOTAL_LEDS; i++) {
    strip.setPixelColor(i, 1, 1, 3);
  }
  
  // Create new ripple on beat
  if (beatDetected) {
    for (int i = 4; i > 0; i--) {
      ripples[i] = ripples[i - 1];
    }
    ripples[0] = 0;
  }
  
  // Expand ripples
  for (int r = 0; r < 5; r++) {
    ripples[r] += rippleSpeeds[r];
    if (ripples[r] > CENTER) ripples[r] = CENTER + 100;
    
    if (ripples[r] < CENTER) {
      // Draw ripple circle
      int ripplePos = (int)ripples[r];
      
      for (int i = 0; i < TOTAL_LEDS; i++) {
        int distance = abs(i - CENTER);
        if (distance > ripplePos - 3 && distance < ripplePos + 3) {
          float intensity = 1.0 - (abs(distance - ripplePos) / 3.0);
          
          uint8_t hue = (r * 51) + (trebleLevel * 50);
          uint32_t color = Wheel(hue);
          
          uint8_t cr = ((color >> 16) & 0xFF) * intensity;
          uint8_t cg = ((color >> 8) & 0xFF) * intensity;
          uint8_t cb = (color & 0xFF) * intensity;
          
          strip.setPixelColor(i, cr, cg, cb);
        }
      }
    }
  }
}