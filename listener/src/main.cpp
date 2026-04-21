#include <Arduino.h>
#include <FastLED.h>
#include <NimBLEDevice.h>

#ifndef LED_PIN
  #if defined(CONFIG_IDF_TARGET_ESP32C3)
    #define LED_PIN     8   // Common WS2812 pin for ESP32-C3
  #else
    #define LED_PIN     15  // Default for ESP32 WROOM
  #endif
#endif
#ifndef NUM_LEDS
#define NUM_LEDS    148
#endif
#define LED_TYPE    WS2812B
#define COLOR_ORDER GRB
CRGB leds[NUM_LEDS];
#define BRIGHTNESS  128

// Indicator Config
#if defined(CONFIG_IDF_TARGET_ESP32C3)
  #define ONBOARD_LED_PIN 9 // Many C3 boards use 9 or 8. (Change this if your blue LED is on another pin)
  #define LED_ACTIVE_STATE LOW // C3 blue LEDs are often active-low
#else
  #define ONBOARD_LED_PIN 2 // WROOM built-in blue LED
  #define LED_ACTIVE_STATE HIGH // WROOM blue LEDs are active-high
#endif

uint32_t indicatorTimer = 0;
bool indicatorActive = false;

// Layout Config
struct Section { int start; int end; };
Section sections[5]; // Dynamically sized in setup()

// Animation State
enum AnimationMode { MODE_SOLID, MODE_DUAL, MODE_BURST, MODE_COMET, MODE_FADE, MODE_STROBE, MODE_STEPPER, MODE_RAINBOW, MODE_FLASH_WHITE, MODE_FLASH_PURPLE, MODE_FLASH_CHOOSE };

struct AnimationState {
    AnimationMode mode;
    uint32_t durationMs;
    uint32_t fadeOutMs;
    bool isAlwaysOn;
    CRGB colors[5];
    uint8_t colorIndices[5];
    CRGB stepColors[5][3]; // Sub-palette for Stepper mode
    uint8_t stepHues[5];   // Hue offsets for Zone Rainbow
    uint32_t zoneTimers[5];
    bool zoneFlashing[5];
    uint8_t zoneFlashColor[5];
    uint32_t triggerTime;
    bool active;
    uint32_t lastSubStep; 
};

volatile bool newCommandReceived = false;
AnimationState nextState;
AnimationState activeState = {MODE_SOLID, 0, 0, false};

// Debouncing
struct LastPacket { std::string data; std::string addr; uint32_t timestamp; };
LastPacket lastSeen = {"", "", 0};

// --- Disney Timing Decoder ---
void parseTiming(uint8_t b, AnimationState &state) {
    uint8_t value = b & 0x0F;
    uint8_t fadeBits = (b >> 4) & 0x03;
    bool alwaysOnBit = (b >> 7) & 0x01;
    float secs = 1.75f * value - 0.25f;
    if (secs < 0.1f) secs = 0.1f;
    state.durationMs = (uint32_t)(secs * 1000.0f);
    state.fadeOutMs = (uint32_t)fadeBits * 1000;
    state.isAlwaysOn = alwaysOnBit;
}

// --- Disney Palette Logic ---
const char* getDisneyColorName(uint8_t rawByte) {
    uint8_t index = (rawByte & 0x1F);
    switch (index) {
        case 0:  return "cyan";
        case 1:  return "light blue";
        case 2:  return "blue";
        case 3:  return "dim purple";
        case 4:  return "midnight blue";
        case 5:  return "bright lavender";
        case 6:  return "white";
        case 7:  return "purple";
        case 8:  return "bright pink";
        case 9:  return "light pink";
        case 10: return "lighter pink";
        case 11: return "lighter pink 2";
        case 12: return "bright pink 2";
        case 13: return "bright pink 3";
        case 14: return "pink/red";
        case 15: return "yellow orange";
        case 16: return "light yellow";
        case 17: return "yellow";
        case 18: return "lime";
        case 19: return "orange";
        case 20: return "red orange";
        case 21: return "red";
        case 22: return "bright cyan";
        case 23: return "bright cyan 2";
        case 24: return "dark cyan";
        case 25: return "green";
        case 26: return "lime green";
        case 27: return "bright light blue";
        case 28: return "bright light blue 2";
        case 29: return "black";
        case 30: return "purple-ish";
        case 31: return "random";
        default: return "Unknown";
    }
}

CRGB getDisneyColor(uint8_t rawByte) {
    uint8_t index = (rawByte & 0x1F);
    switch (index) {
        case 0:  return CRGB::Cyan;
        case 1:  return 0xADD8E6; // light blue
        case 2:  return CRGB::Blue;
        case 3:  return 0x4B0082; // dim purple
        case 4:  return CRGB::MidnightBlue;
        case 5:  return 0xE6E6FA; // bright lavender
        case 6:  return CRGB::White;
        case 7:  return CRGB::Purple;
        case 8:  return 0xFF007F; // bright pink
        case 9:  return CRGB::LightPink;
        case 10: return 0xFFB6C1; // lighter pink
        case 11: return 0xFFC0CB; // lighter pink 2
        case 12: return 0xFF1493; // bright pink 2
        case 13: return 0xFF69B4; // bright pink 3
        case 14: return 0xFF0033; // pink/red
        case 15: return 0xFFAB00; // yellow orange
        case 16: return 0xFFFFE0; // light yellow
        case 17: return CRGB::Yellow;
        case 18: return CRGB::Lime;
        case 19: return CRGB::Orange;
        case 20: return 0xFF4500; // red orange
        case 21: return CRGB::Red;
        case 22: return 0x00FFFF; // bright cyan
        case 23: return 0x00CED1; // bright cyan 2
        case 24: return 0x008B8B; // dark cyan
        case 25: return CRGB::Green;
        case 26: return CRGB::LimeGreen;
        case 27: return 0x00BFFF; // bright light blue
        case 28: return 0x87CEFA; // bright light blue 2
        case 29: return CRGB::Black;
        case 30: return 0x800080; // purple-ish
        case 31: return CHSV(random8(), 255, 255);
        default: return CRGB::Black;
    }
}

class MyDescriptiveCallbacks : public NimBLEAdvertisedDeviceCallbacks {
    void onResult(NimBLEAdvertisedDevice* advertisedDevice) {
        std::string mfgData = advertisedDevice->getManufacturerData();
        if (mfgData.length() < 3) return;
        uint16_t mfgId = (uint8_t)mfgData[0] | ((uint8_t)mfgData[1] << 8);
        if (mfgId != 0x0183) return;

        std::string addr = advertisedDevice->getAddress().toString();
        if (mfgData == lastSeen.data && addr == lastSeen.addr && (millis() - lastSeen.timestamp < 3000)) return;
        lastSeen.data = mfgData; lastSeen.addr = addr; lastSeen.timestamp = millis();

        int actionIdx = -1;
        uint8_t primaryCode = 0;
        for (int i = 2; i < mfgData.length(); i++) {
            uint8_t c = (uint8_t)mfgData[i];
            if (c == 0xE9 || c == 0xCC) { actionIdx = i; primaryCode = c; break; }
        }

        if (actionIdx == -1) return;

        // PING: Logic to ignore MB respondents
        if (primaryCode == 0xCC) {
            // Broadcasters (Show Nodes) are usually 5 bytes: 83 01 CC 03 00 (length 5)
            // MB replies are usually 18-20+ bytes.
            if (mfgData.length() < 7) {
                Serial.println("[PING] Broadcaster Node detected.");
            }
            return; 
        }

        // SHOW ACTION (E9)
        if (primaryCode == 0xE9 && mfgData.length() > actionIdx + 3) {
            static uint32_t lastE9Trigger = 0;
            if (millis() - lastE9Trigger < 2500) return; // Hard debounce on ANY E9 packet to fix broadcaster spam
            lastE9Trigger = millis();

            uint8_t subAction = (uint8_t)mfgData[actionIdx + 1];
            nextState.active = false;
            
            if (subAction == 0x0E && mfgData.length() > actionIdx + 4) {
                parseTiming((uint8_t)mfgData[actionIdx + 4], nextState); // 0E timing byte is shifted right
            } else {
                parseTiming((uint8_t)mfgData[actionIdx + 3], nextState);
            }
            
            nextState.triggerTime = millis();
            nextState.lastSubStep = 0;
            
            char logBuffer[128];
            float secs = nextState.durationMs / 1000.0f;
            uint8_t vib = (uint8_t)mfgData[mfgData.length() - 1];

            if (subAction == 0x05 && mfgData.length() >= actionIdx + 6) {
                nextState.mode = MODE_SOLID;
                uint8_t c = (uint8_t)mfgData[actionIdx + 5];
                for(int i=0; i<5; i++) { nextState.colors[i] = getDisneyColor(c); nextState.colorIndices[i] = c; }
                snprintf(logBuffer, sizeof(logBuffer), "Action: Solid %s for %.1fs", getDisneyColorName(c), secs);
            } else if (subAction == 0x06 && mfgData.length() >= actionIdx + 7) {
                nextState.mode = MODE_DUAL;
                uint8_t inner = (uint8_t)mfgData[actionIdx + 5];
                uint8_t outer = (uint8_t)mfgData[actionIdx + 6];
                nextState.colors[2] = getDisneyColor(inner); nextState.colorIndices[2] = inner;
                nextState.colors[0] = nextState.colors[1] = nextState.colors[3] = nextState.colors[4] = getDisneyColor(outer);
                snprintf(logBuffer, sizeof(logBuffer), "Action: Dual Colors %s (Outer) & %s (Inner) for %.1fs", getDisneyColorName(outer), getDisneyColorName(inner), secs);
            } else if (subAction == 0x09 && mfgData.length() >= actionIdx + 10) {
                nextState.mode = MODE_BURST;
                uint8_t c_cn = (uint8_t)mfgData[actionIdx + 5];
                uint8_t c_tr = (uint8_t)mfgData[actionIdx + 6];
                uint8_t c_br = (uint8_t)mfgData[actionIdx + 7];
                uint8_t c_bl = (uint8_t)mfgData[actionIdx + 8];
                uint8_t c_tl = (uint8_t)mfgData[actionIdx + 9];
                nextState.colors[2] = getDisneyColor(c_cn); nextState.colors[3] = getDisneyColor(c_tr);
                nextState.colors[4] = getDisneyColor(c_br); nextState.colors[0] = getDisneyColor(c_bl);
                nextState.colors[1] = getDisneyColor(c_tl);
                snprintf(logBuffer, sizeof(logBuffer), "Action: Multi-Burst [TL:%s, BL:%s, BR:%s, TR:%s, CN:%s] for %.1fs", 
                         getDisneyColorName(c_tl), getDisneyColorName(c_bl), getDisneyColorName(c_br), getDisneyColorName(c_tr), getDisneyColorName(c_cn), secs);
            } else if (subAction == 0x0B) {
                nextState.mode = MODE_COMET;
                snprintf(logBuffer, sizeof(logBuffer), "Action: Dual Green Comet Chase for %.1fs", secs);
            } else if (subAction == 0x0C && mfgData.length() >= actionIdx + 7) {
                uint8_t sig0 = (uint8_t)mfgData[actionIdx + 5], sig1 = (uint8_t)mfgData[actionIdx + 6], sig2 = (uint8_t)mfgData[actionIdx + 7];
                if (sig0 == 0x5D && sig1 == 0x46 && sig2 == 0x5B) {
                    if (vib == 0x95) { nextState.mode = MODE_FADE; snprintf(logBuffer, sizeof(logBuffer), "Action: Sequential White/Pink Fade for %.1fs", secs); }
                    else { 
                        nextState.mode = MODE_RAINBOW; 
                        for(int s=0; s<5; s++) nextState.stepHues[s] = s * 51; // Space hues evenly around wheel
                        snprintf(logBuffer, sizeof(logBuffer), "Action: 3s Zone Rainbow for %.1fs", secs); 
                    }
                } else if (sig0 == 0x4F && sig1 == 0x4F && sig2 == 0x5B) {
                    nextState.mode = MODE_STROBE; snprintf(logBuffer, sizeof(logBuffer), "Action: Yellow/Orange/Off Strobe Cycle for %.1fs", secs);
                } else if (sig0 == 0xB1 && sig1 == 0xB9 && sig2 == 0xB5) {
                    nextState.mode = MODE_STEPPER; 
                    CRGB palette[] = {CRGB::Red, CRGB::Green, CRGB::Yellow, CRGB::Blue};
                    for(int s=0; s<5; s++) {
                        for(int p=0; p<3; p++) nextState.stepColors[s][p] = palette[random8(4)];
                    }
                    snprintf(logBuffer, sizeof(logBuffer), "Action: Multi-Zone Color Step for %.1fs", secs);
                } else {
                    nextState.mode = MODE_SOLID; for(int i=0; i<5; i++) nextState.colors[i] = CRGB::White;
                    snprintf(logBuffer, sizeof(logBuffer), "Action: Unknown 0C Show Signature for %.1fs", secs);
                }
            } else if (subAction == 0x0E && mfgData.length() >= actionIdx + 5) {
                uint8_t sig = (uint8_t)mfgData[actionIdx + 3];
                if (sig == 0x01) nextState.mode = MODE_FLASH_WHITE;
                else if (sig == 0x02) nextState.mode = MODE_FLASH_PURPLE;
                else if (sig == 0x11) nextState.mode = MODE_FLASH_CHOOSE;
                else nextState.mode = MODE_FLASH_WHITE;
                
                for(int i=0; i<5; i++) { nextState.zoneFlashing[i] = false; nextState.zoneTimers[i] = 0; nextState.zoneFlashColor[i] = 0; }
                snprintf(logBuffer, sizeof(logBuffer), "Action: Fast Zone Flash (%02X) for %.1fs", sig, secs);
            } else {
                nextState.mode = MODE_SOLID; for(int i=0; i<5; i++) nextState.colors[i] = CRGB::White;
                snprintf(logBuffer, sizeof(logBuffer), "Action: Unknown Mode (%02X) Default for %.1fs", subAction, secs);
            }
            
            Serial.println(logBuffer);
            nextState.active = true;
            newCommandReceived = true;
            
            // Trigger onboard flash
            indicatorActive = true;
            indicatorTimer = millis();
            digitalWrite(ONBOARD_LED_PIN, LED_ACTIVE_STATE);
        }
    }
};

void updateAnimations() {
    if (newCommandReceived) { activeState = nextState; newCommandReceived = false; }
    if (!activeState.active) { 
        FastLED.clear(); 
        FastLED.show(); 
        return; 
    }

    uint32_t elapsed = millis() - activeState.triggerTime;
    // Hard 30s cut-off or packet-specified duration
    if (elapsed > 30000 || (!activeState.isAlwaysOn && elapsed >= activeState.durationMs)) {
        activeState.active = false; 
        FastLED.clear(); 
        FastLED.show(); 
        return;
    }

    uint8_t intensity = beatsin8(60, 150, 255, activeState.triggerTime);
    if (!activeState.isAlwaysOn && activeState.fadeOutMs > 0) {
        uint32_t fadeStartTime = (activeState.durationMs > activeState.fadeOutMs) ? (activeState.durationMs - activeState.fadeOutMs) : 0;
        if (elapsed > fadeStartTime) {
            uint32_t fElap = elapsed - fadeStartTime;
            if (fElap < activeState.fadeOutMs) intensity = map(fElap, 0, activeState.fadeOutMs, intensity, 0); else intensity = 0;
        }
    }

    if (activeState.mode == MODE_SOLID || activeState.mode == MODE_DUAL || activeState.mode == MODE_BURST) {
        for (int s = 0; s < 5; s++) { CRGB c = activeState.colors[s]; c.nscale8(intensity); for (int i = sections[s].start; i <= sections[s].end; i++) leds[i] = c; }
    } else if (activeState.mode == MODE_COMET) {
        FastLED.clear(); 
        uint16_t pos1 = (elapsed % 4000) * NUM_LEDS / 4000;
        uint16_t pos2 = (pos1 + (NUM_LEDS / 2)) % NUM_LEDS;
        uint16_t tailLen = NUM_LEDS / 4; // Half distance to following comet
        for(int i=0; i<tailLen; i++) { 
            uint8_t fade = 255 - (i * (255 / tailLen));
            leds[(pos1 - i + NUM_LEDS) % NUM_LEDS] = CRGB::Green; leds[(pos1 - i + NUM_LEDS) % NUM_LEDS].nscale8(fade);
            leds[(pos2 - i + NUM_LEDS) % NUM_LEDS] = CRGB::Green; leds[(pos2 - i + NUM_LEDS) % NUM_LEDS].nscale8(fade);
        }
    } else if (activeState.mode == MODE_RAINBOW) {
        uint8_t deltaHue = (elapsed * 255 / 3000) % 256;
        for (int s = 0; s < 5; s++) {
            CRGB c = CHSV(activeState.stepHues[s] + deltaHue, 255, 255);
            for (int i = sections[s].start; i <= sections[s].end; i++) leds[i] = c;
        }
    } else if (activeState.mode == MODE_FADE) {
        uint32_t cycle = elapsed % 1500;
        CRGB c1 = CRGB::White, c2 = CRGB(5, 2, 10); // Extremely dim, almost off target color
        uint8_t ratio = map(cycle, 0, 1500, 255, 0);
        CRGB c = blend(c2, c1, ratio);
        for(int i=0; i<NUM_LEDS; i++) leds[i] = c;
    } else if (activeState.mode == MODE_STROBE) {
        uint32_t cycle = elapsed % 5000;
        CRGB c = CRGB::Black;
        if (cycle < 500) c = CRGB::Yellow;
        else if (cycle < 1000) c = CRGB::Orange;
        for(int i=0; i<NUM_LEDS; i++) leds[i] = c;
    } else if (activeState.mode == MODE_STEPPER) {
        uint8_t stepIdx = (elapsed / 500) % 3;
        for (int s = 0; s < 5; s++) { 
            CRGB c = activeState.stepColors[s][stepIdx];
            for (int i = sections[s].start; i <= sections[s].end; i++) leds[i] = c; 
        }
    } else if (activeState.mode == MODE_FLASH_WHITE || activeState.mode == MODE_FLASH_PURPLE || activeState.mode == MODE_FLASH_CHOOSE) {
        FastLED.clear();
        for (int s = 0; s < 5; s++) {
            if (!activeState.zoneFlashing[s]) {
                if (elapsed - activeState.zoneTimers[s] >= 1000) { // 1 second cooldown
                    if (random8() < 12) { // Random chance to trigger (~2 times per second at 60fps)
                        activeState.zoneFlashing[s] = true;
                        activeState.zoneTimers[s] = elapsed;
                        if (activeState.mode == MODE_FLASH_CHOOSE) {
                            if (s == 0 || s == 3) activeState.zoneFlashColor[s] = 1; // Purple
                            else if (s == 1 || s == 4) activeState.zoneFlashColor[s] = 2; // Green
                            else activeState.zoneFlashColor[s] = (random8() < 128) ? 1 : 2; // CN choose
                        }
                    }
                }
            } else {
                uint32_t fElap = elapsed - activeState.zoneTimers[s];
                if (fElap > 250) { // 250ms total flash duration
                    activeState.zoneFlashing[s] = false;
                    activeState.zoneTimers[s] = elapsed; // Start cooldown
                } else {
                    CRGB renderColor = CRGB::Black;
                    if (activeState.mode == MODE_FLASH_WHITE) {
                        uint8_t ratio = map(fElap, 0, 250, 255, 0);
                        renderColor = CRGB(ratio, ratio, ratio);
                    } else if (activeState.mode == MODE_FLASH_PURPLE) {
                        // White -> Purple -> Off
                        if (fElap < 125) {
                            uint8_t ratio = map(fElap, 0, 125, 255, 0);
                            renderColor = blend(CRGB::Purple, CRGB::White, ratio);
                        } else {
                            uint8_t ratio = map(fElap, 125, 250, 255, 0);
                            renderColor = CRGB::Purple; renderColor.nscale8(ratio);
                        }
                    } else if (activeState.mode == MODE_FLASH_CHOOSE) {
                        CRGB baseColor = (activeState.zoneFlashColor[s] == 1) ? CRGB::Purple : CRGB::Green;
                        uint8_t ratio = map(fElap, 0, 250, 255, 0);
                        renderColor = baseColor; renderColor.nscale8(ratio);
                    }
                    for (int i = sections[s].start; i <= sections[s].end; i++) leds[i] = renderColor;
                }
            }
        }
    }
    FastLED.show();
}

void setup() {
    // Dynamically calculate sections based on NUM_LEDS
    int ledsPerSection = NUM_LEDS / 5;
    for (int i = 0; i < 5; i++) {
        sections[i].start = i * ledsPerSection;
        sections[i].end = (i == 4) ? (NUM_LEDS - 1) : ((i + 1) * ledsPerSection - 1);
    }

    // Reduce heat by underclocking CPU to 80MHz (plenty for BLE and LED)
    setCpuFrequencyMhz(80);
    
    delay(2000); Serial.begin(115200);
    FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
    FastLED.setBrightness(BRIGHTNESS);
    FastLED.clear(true);
    
    pinMode(ONBOARD_LED_PIN, OUTPUT);
    digitalWrite(ONBOARD_LED_PIN, !LED_ACTIVE_STATE); // Ensure off initially
    
    NimBLEDevice::setScanDuplicateCacheSize(0);
    NimBLEDevice::init("MB_Scanner_Desk");
    NimBLEScan* pScan = NimBLEDevice::getScan();
    pScan->setAdvertisedDeviceCallbacks(new MyDescriptiveCallbacks(), false);
    pScan->setActiveScan(true); 
    pScan->setDuplicateFilter(false);
    pScan->setInterval(100);
    pScan->setWindow(75); // Sleep the radio 25% of the time to cool down
    pScan->start(0, nullptr, false);
    Serial.println("\r\n--- MagicBand+ Descriptive Decoder Ready ---");
}

void loop() { 
    updateAnimations(); 
    
    if (indicatorActive && (millis() - indicatorTimer > 100)) {
        digitalWrite(ONBOARD_LED_PIN, !LED_ACTIVE_STATE);
        indicatorActive = false;
    }
    
    // Throttle frame rate down from 100fps to ~30fps 
    // This stops FastLED from aggressively blocking CPU interrupts continuously, dropping heat massively
    delay(30); 
}
