#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <FastLED.h>
#include <LittleFS.h>
#include <NimBLEDevice.h>
#include <WiFi.h>
#include <set>

// --- SYSTEM CONFIG ---
#define LOG_FILE "/unknown.txt"
std::set<String> loggedPackets;
AsyncWebServer server(80);
bool webServerStarted = false;

#ifndef LED_PIN
#if defined(CONFIG_IDF_TARGET_ESP32C3)
#define LED_PIN 8
#else
#define LED_PIN 15
#endif
#endif

#ifndef NUM_LEDS
#define NUM_LEDS 148
#endif

#define MOSFET_PIN 4
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB
CRGB leds[NUM_LEDS];
#define BRIGHTNESS 64 // Maximum brightness of the LED strip; max, max_bright

#if defined(CONFIG_IDF_TARGET_ESP32C3)
#define ONBOARD_LED_PIN 9
#define LED_ACTIVE_STATE LOW
#else
#define ONBOARD_LED_PIN 2
#define LED_ACTIVE_STATE HIGH
#endif

uint32_t indicatorTimer = 0;
bool indicatorActive = false;

// --- LAYOUT & ZONES ---
uint8_t zoneMap[NUM_LEDS];
uint8_t relativeMap[NUM_LEDS];
uint8_t
    cometMap[NUM_LEDS]; // Explicit virtual path for the Cyan Comet (19 -> 0)
uint8_t currentZoneIntensity[5] = {0, 0, 0, 0,
                                   0}; // Persistent for decay animations
uint32_t sparkleUntil = 0; // Sparkle overlay timer for recognized packets

void setZoneRange(uint8_t zone, int start, int end) {
  if (zone < 1 || zone > 5)
    return;
  uint8_t zIdx = zone - 1;
  for (int i = start; i <= end; i++) {
    if (i >= 0 && i < NUM_LEDS) {
      zoneMap[i] = zIdx;
      relativeMap[i] = i - start;
    }
  }
}

// --- ANIMATION ENGINE ---
enum AnimationMode {
  MODE_SOLID,
  MODE_DUAL,
  MODE_BURST,
  MODE_COMET,
  MODE_FADE,
  MODE_STROBE,
  MODE_STEPPER,
  MODE_RAINBOW,
  MODE_FLASH_CHOOSE,
  MODE_ZONE_TWINKLE,
  MODE_CANDLES,
  MODE_PULSE_Z3,
  MODE_BLOOD_ORANGE_CYCLE,
  MODE_CYAN_DIM_COMET,
  MODE_WILD_SPARKLE,
  MODE_YELLOW_DOUBLE_COMET,
  MODE_DUAL_COLOR_FADE,
  MODE_GREEN_DOUBLE_COMET,
  MODE_TWO_COLOR_TOGGLE,
  MODE_COLOR_ROTATE,
  MODE_PULSE_BURST,
  MODE_ZONE_ALTERNATE
};

struct AnimationState {
  AnimationMode mode;
  uint32_t durationMs;
  uint32_t fadeOutMs;
  bool isAlwaysOn;
  CRGB colors[5];
  CRGB stepColors[5][3];
  uint8_t stepHues[5];
  uint32_t zoneTimers[5];
  uint32_t zoneFlashEnd[5]; // Hold brightness until this time
  bool zoneFlashing[5];
  uint32_t triggerTime;
  bool active;
};

volatile bool newCommandReceived = false;
AnimationState nextState;
AnimationState activeState = {MODE_SOLID, 0, 0, false};

struct LastPacket {
  std::string data;
  std::string addr;
  uint32_t timestamp;
};
LastPacket lastSeen = {"", "", 0};

// --- HELPERS ---
void parseTiming(uint8_t b, AnimationState &state) {
  uint8_t value = b & 0x0F;
  uint8_t fadeBits = (b >> 4) & 0x03;
  bool multiplierBit = (b >> 6) & 0x01;
  bool alwaysOnBit = (b >> 7) & 0x01;
  float secs = (float)value;
  secs *= multiplierBit ? 3.1f : 1.5f;
  if (secs < 0.1f)
    secs = 0.1f;
  state.durationMs = (uint32_t)(secs * 1000.0f);
  state.fadeOutMs = (uint32_t)fadeBits * 1000;
  state.isAlwaysOn = alwaysOnBit;
}

CRGB getDisneyColor(uint8_t rawByte) {
  uint8_t index = (rawByte & 0x1F);
  switch (index) {
  case 0:
    return CRGB::Cyan;
  case 1:
    return 0xADD8E6;
  case 2:
    return CRGB::Blue;
  case 3:
    return 0x4B0082;
  case 4:
    return CRGB::MidnightBlue;
  case 5:
    return 0xE6E6FA;
  case 6:
    return CRGB::White;
  case 7:
    return CRGB::Purple;
  case 8:
    return 0xFF007F;
  case 9:
    return CRGB::LightPink;
  case 10:
    return 0xFFB6C1;
  case 11:
    return 0xFFC0CB;
  case 12:
    return 0xFF1493;
  case 13:
    return 0xFF69B4;
  case 14:
    return 0xFF0033;
  case 15:
    return 0xFFAB00;
  case 16:
    return 0xFFFFE0;
  case 17:
    return CRGB::Yellow;
  case 18:
    return CRGB::Lime;
  case 19:
    return CRGB::Orange;
  case 20:
    return 0xFF4500;
  case 21:
    return CRGB::Red;
  case 22:
    return 0x00FFFF;
  case 23:
    return 0x00CED1;
  case 24:
    return 0x008B8B;
  case 25:
    return CRGB::Green;
  case 26:
    return CRGB::LimeGreen;
  case 27:
    return 0x00BFFF;
  case 28:
    return 0x87CEFA;
  case 29:
    return CRGB::Black;
  case 30:
    return 0x800080;
  case 31:
    return CHSV(random8(), 255, 255);
  default:
    return CRGB::Black;
  }
}

bool logUnknown(std::string data) {
  String hex = "";
  for (size_t i = 0; i < data.length(); i++) {
    uint8_t b = (uint8_t)data[i];
    if (b < 0x10)
      hex += "0";
    hex += String(b, HEX);
  }
  if (loggedPackets.count(hex) > 0)
    return false;
  loggedPackets.insert(hex);
  File file = LittleFS.open(LOG_FILE, FILE_APPEND);
  if (file) {
    file.println(hex);
    file.close();
    return true;
  }
  return false;
}

void loadLogs() {
  if (!LittleFS.exists(LOG_FILE))
    return;
  File file = LittleFS.open(LOG_FILE, FILE_READ);
  if (!file)
    return;
  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.length() > 0)
      loggedPackets.insert(line);
  }
  file.close();
}

// --- BLE CALLBACKS ---
class MyDescriptiveCallbacks : public NimBLEAdvertisedDeviceCallbacks {
  void onResult(NimBLEAdvertisedDevice *advertisedDevice) {
    std::string mfgData = advertisedDevice->getManufacturerData();
    if (mfgData.length() < 3)
      return;
    uint16_t mfgId = (uint8_t)mfgData[0] | ((uint8_t)mfgData[1] << 8);
    if (mfgId != 0x0183)
      return;
    std::string addr = advertisedDevice->getAddress().toString();
    if (mfgData == lastSeen.data && addr == lastSeen.addr &&
        (millis() - lastSeen.timestamp < 3000))
      return;
    lastSeen.data = mfgData;
    lastSeen.addr = addr;
    lastSeen.timestamp = millis();

    int actionIdx = -1;
    for (int i = 0; i < mfgData.length(); i++) {
      uint8_t c = (uint8_t)mfgData[i];
      if (c == 0xE9 || c == 0xCC) {
        actionIdx = i;
        break;
      }
    }
    if (actionIdx == -1 || (uint8_t)mfgData[actionIdx] == 0xCC)
      return;

    String hex = "";
    for (size_t i = actionIdx; i < mfgData.length(); i++) {
      uint8_t b = (uint8_t)mfgData[i];
      if (b < 0x10)
        hex += "0";
      hex += String(b, HEX);
    }

    uint8_t subAction = (uint8_t)mfgData[actionIdx + 1];
    parseTiming((uint8_t)mfgData[actionIdx + 3], nextState);
    nextState.triggerTime = millis();
    nextState.active = false;
    uint8_t vib = (uint8_t)mfgData[mfgData.length() - 1];
    bool triggered = false;

    if (subAction == 0x05) {
      nextState.mode = MODE_SOLID;
      uint8_t c = (uint8_t)mfgData[actionIdx + 5];
      uint8_t mask = (c >> 5) & 0x07;
      CRGB color = getDisneyColor(c);
      for (int i = 0; i < 5; i++)
        nextState.colors[i] =
            (mask == 0 || mask == 5 || mask == 7) ? color : CRGB::Black;
      if (mask == 3)
        nextState.colors[0] = color;
      if (mask == 4)
        nextState.colors[1] = color;
      if (mask == 1 || mask == 6)
        nextState.colors[2] = color;
      if (mask == 2)
        nextState.colors[3] = color;
      triggered = true;
    } else if (subAction == 0x06) {
      nextState.mode = MODE_DUAL;
      nextState.colors[2] = getDisneyColor((uint8_t)mfgData[actionIdx + 5]);
      nextState.colors[0] = nextState.colors[1] = nextState.colors[3] =
          nextState.colors[4] = getDisneyColor((uint8_t)mfgData[actionIdx + 6]);
      triggered = true;
    } else if (subAction == 0x09) {
      nextState.mode = MODE_BURST;
      for (int i = 0; i < 5; i++)
        nextState.colors[i] =
            getDisneyColor((uint8_t)mfgData[actionIdx + 5 + i]);
      triggered = true;
    } else if (subAction == 0x0B) {
      nextState.mode = MODE_GREEN_DOUBLE_COMET;
      triggered = true;
    } else if (subAction == 0x0C) {
      uint8_t s0 = (uint8_t)mfgData[actionIdx + 5];
      uint8_t s1 = (uint8_t)mfgData[actionIdx + 6];
      uint8_t s2 = (uint8_t)mfgData[actionIdx + 7];
      if (s0 == 0x5D && s1 == 0x46 && s2 == 0x5B) {
        if (vib == 0x95)
          nextState.mode = MODE_FADE;
        else {
          nextState.mode = MODE_RAINBOW;
          for (int s = 0; s < 5; s++)
            nextState.stepHues[s] = s * 51;
        }
      } else if (s0 == 0x4F && s1 == 0x4F && s2 == 0x5B)
        nextState.mode = MODE_STROBE;
      else if (s0 == 0xB1 && s1 == 0xB9 && s2 == 0xB5) {
        nextState.mode = MODE_STEPPER;
        CRGB palette[] = {CRGB::Red, CRGB::Green, CRGB::Yellow, CRGB::Blue};
        for (int s = 0; s < 5; s++) {
          for (int p = 0; p < 3; p++)
            nextState.stepColors[s][p] = palette[random8(4)];
        }
      }
      triggered = true;
    } else if (subAction == 0x0E) {
      uint8_t sig = (uint8_t)mfgData[mfgData.length() - 4];
      nextState.mode = (sig == 0x0B) ? MODE_ZONE_TWINKLE : MODE_FLASH_CHOOSE;
      if (sig == 0x0B)
        nextState.durationMs = 20000;
      for (int i = 0; i < 5; i++) {
        nextState.colors[i] =
            getDisneyColor((uint8_t)mfgData[actionIdx + 5 + i]);
        nextState.zoneFlashing[i] = false;
        nextState.zoneTimers[i] = 0;
        nextState.zoneFlashEnd[i] = 0;
      }

      // Detect 2-color pattern: bytes = [marker, colorA, colorB, colorA,
      // colorB] If byte0 differs from both byte1 and byte2, it's a pattern
      // marker, not a zone color
      uint8_t rb0 = (uint8_t)mfgData[actionIdx + 5];
      uint8_t rb1 = (uint8_t)mfgData[actionIdx + 6];
      uint8_t rb2 = (uint8_t)mfgData[actionIdx + 7];
      uint8_t rb3 = (uint8_t)mfgData[actionIdx + 8];
      uint8_t rb4 = (uint8_t)mfgData[actionIdx + 9];
      if (rb1 == rb3 && rb2 == rb4 && rb0 != rb1 && rb0 != rb2) {
        nextState.colors[0] =
            nextState.colors[3]; // Zone 1 = Color A (same as Zone 4)
        nextState.colors[1] =
            nextState.colors[4]; // Zone 2 = Color B (same as Zone 5)
        nextState.colors[2] =
            nextState.colors[3]; // Zone 3 = Color A (same as Zone 4)
      }

      triggered = true;
    } else if (subAction == 0x0F) {
      if (hex.indexOf("2a0717b8") != -1)
        nextState.mode = MODE_CANDLES;
      else if (hex.indexOf("021200b0") != -1)
        nextState.mode = MODE_PULSE_Z3;
      triggered = true;
    } else if (subAction == 0x10) {
      if (hex.indexOf("2102") != -1) {
        nextState.mode = MODE_BLOOD_ORANGE_CYCLE;
        nextState.durationMs = 30000;
        triggered = true;
      } else if (hex.indexOf("4e07b0") != -1) {
        nextState.mode = MODE_CYAN_DIM_COMET;
        triggered = true;
      }
    } else if (subAction == 0x11) {
      CRGB c1 = getDisneyColor((uint8_t)mfgData[actionIdx + 5]);
      CRGB c2 = getDisneyColor((uint8_t)mfgData[actionIdx + 6]);
      if (hex.indexOf("f44882") != -1) {
        // Smooth fade between two colors (out-of-phase center)
        nextState.mode = MODE_DUAL_COLOR_FADE;
        nextState.colors[0] = c1;
        nextState.colors[1] = c2;
      } else {
        // Pulse burst: center = color1, outside = color2
        nextState.mode = MODE_PULSE_BURST;
        nextState.colors[0] = c2; // Z1 outside
        nextState.colors[1] = c2; // Z2 outside
        nextState.colors[2] = c1; // Z3 center
        nextState.colors[3] = c2; // Z4 outside
        nextState.colors[4] = c2; // Z5 outside
      }
      triggered = true;
    } else if (subAction == 0x12) {
      uint8_t flagByte = (uint8_t)mfgData[actionIdx + 4];
      // Color bytes are in zone order: Z2, Z3, Z4, Z5, Z1
      CRGB rawC[5];
      for (int i = 0; i < 5; i++)
        rawC[i] = getDisneyColor((uint8_t)mfgData[actionIdx + 5 + i]);
      nextState.colors[1] = rawC[0]; // Z2
      nextState.colors[2] = rawC[1]; // Z3
      nextState.colors[3] = rawC[2]; // Z4
      nextState.colors[4] = rawC[3]; // Z5
      nextState.colors[0] = rawC[4]; // Z1
      if (flagByte == 0x0F) {
        nextState.mode = MODE_COLOR_ROTATE;
      } else {
        nextState.mode = MODE_TWO_COLOR_TOGGLE;
      }
      triggered = true;
    } else if (subAction == 0x13) {
      nextState.mode = MODE_ZONE_ALTERNATE;
      // byte0 = Z3 center; byte1,3 = Group B; byte2,4 = Group A
      CRGB center = getDisneyColor((uint8_t)mfgData[actionIdx + 5]);
      CRGB groupB = getDisneyColor((uint8_t)mfgData[actionIdx + 6]);
      CRGB groupA = getDisneyColor((uint8_t)mfgData[actionIdx + 7]);
      nextState.colors[0] = groupA; // Z1
      nextState.colors[1] = groupB; // Z2
      nextState.colors[2] = center; // Z3
      nextState.colors[3] = groupA; // Z4
      nextState.colors[4] = groupB; // Z5
      triggered = true;
    }

    // Log every unique packet once
    bool isNewPacket = logUnknown(mfgData);

    if (isNewPacket && triggered) {
      // New + recognized: sparkle 1s then animate
      sparkleUntil = millis() + 1000;
      nextState.active = true;
      newCommandReceived = true;
      indicatorActive = true;
      indicatorTimer = millis();
      digitalWrite(ONBOARD_LED_PIN, LED_ACTIVE_STATE);
    } else if (isNewPacket && !triggered) {
      // New + unrecognized: sparkle 15s
      nextState.mode = MODE_WILD_SPARKLE;
      nextState.durationMs = 15000;
      nextState.active = true;
      newCommandReceived = true;
      indicatorActive = true;
      indicatorTimer = millis();
      digitalWrite(ONBOARD_LED_PIN, LED_ACTIVE_STATE);
    } else if (triggered) {
      // Seen before + recognized: just animate, no sparkle
      nextState.active = true;
      newCommandReceived = true;
      indicatorActive = true;
      indicatorTimer = millis();
      digitalWrite(ONBOARD_LED_PIN, LED_ACTIVE_STATE);
    } else {
      // Seen before + unrecognized: yellow comet
      nextState.mode = MODE_YELLOW_DOUBLE_COMET;
      nextState.durationMs = 5000;
      nextState.active = true;
      newCommandReceived = true;
    }
  }
};

// --- LOG MANAGEMENT ---
void handleSerialCommands() {
  if (Serial.available() > 0) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd == "printlogs") {
      if (LittleFS.exists(LOG_FILE)) {
        File file = LittleFS.open(LOG_FILE, FILE_READ);
        while (file.available())
          Serial.println(file.readStringUntil('\n'));
        file.close();
      }
    } else if (cmd == "clearlogs") {
      LittleFS.remove(LOG_FILE);
      loggedPackets.clear();
    } else if (cmd == "startweb") {
      if (webServerStarted)
        return;
      WiFi.softAP("MB-Scanner-Logs", "magicband123");
      IPAddress IP = WiFi.softAPIP();
      server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        String html =
            "<html><body><h1>MagicBand+ Logs</h1><p>Unique Captured: " +
            String(loggedPackets.size()) +
            "</p><a href='/download'>Download</a></body></html>";
        request->send(200, "text/html", html);
      });
      server.on("/download", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(LittleFS, LOG_FILE, "text/plain", true);
      });
      server.begin();
      webServerStarted = true;
    }
  }
}

// --- ANIMATION CORE ---
void updateAnimations() {
  if (newCommandReceived) {
    activeState = nextState;
    newCommandReceived = false;
    for (int s = 0; s < 5; s++)
      currentZoneIntensity[s] = 0;
  }
  if (!activeState.active) {
    FastLED.clear();
    FastLED.show();
    return;
  }

  // Sparkle overlay for recognized packets (1s flash before real animation)
  if (millis() < sparkleUntil) {
    for (int i = 0; i < NUM_LEDS; i++) {
      if (zoneMap[i] < 5) {
        if (random8() < 25) leds[i] = CRGB::White;
        else leds[i].fadeToBlackBy(60);
      } else {
        leds[i] = CRGB::Black;
      }
    }
    FastLED.show();
    return;
  }

  uint32_t elapsed = millis() - activeState.triggerTime;
  if (elapsed > 30000 ||
      (!activeState.isAlwaysOn && elapsed >= activeState.durationMs)) {
    activeState.active = false;
    FastLED.clear();
    FastLED.show();
    return;
  }

  uint8_t masterIntensity = 255;
  if (!activeState.isAlwaysOn && activeState.fadeOutMs > 0) {
    uint32_t fadeStart = (activeState.durationMs > activeState.fadeOutMs)
                             ? (activeState.durationMs - activeState.fadeOutMs)
                             : 0;
    if (elapsed > fadeStart)
      masterIntensity = map(elapsed, fadeStart, activeState.durationMs, 255, 0);
  }

  uint8_t zoneIntensity[5] = {0, 0, 0, 0, 0};

  // Fading Logic for Lightning / Twinkle with SUSTAIN
  if (activeState.mode == MODE_FLASH_CHOOSE ||
      activeState.mode == MODE_ZONE_TWINKLE) {
    for (int s = 0; s < 5; s++) {
      if (millis() < activeState.zoneFlashEnd[s]) {
        currentZoneIntensity[s] = 255; // SUSTAIN at full brightness
      } else {
        // Time-based linear fade over 500ms after sustain ends
        uint32_t fadeElapsed = millis() - activeState.zoneFlashEnd[s];
        uint32_t fadeDuration = 500;
        if (fadeElapsed < fadeDuration) {
          currentZoneIntensity[s] = map(fadeElapsed, 0, fadeDuration, 255, 0);
        } else {
          currentZoneIntensity[s] = 0;
        }
      }

      if (millis() > activeState.zoneTimers[s]) {
        bool shouldFlash = (activeState.mode == MODE_FLASH_CHOOSE)
                               ? (random8() < 50)
                               : (random8() < 20);
        if (shouldFlash) {
          currentZoneIntensity[s] = 255;
          // Determine sustain based on duration: 0x01 and 0x83 codes are slower
          uint32_t sustain =
              (activeState.durationMs < 2000 || activeState.durationMs > 14000)
                  ? 250
                  : 125;
          activeState.zoneFlashEnd[s] = millis() + sustain;

          // Gap between flashes to avoid "strobing"
          uint32_t gap = (activeState.durationMs < 2000) ? random(400, 1500)
                                                         : random(100, 400);
          activeState.zoneTimers[s] = millis() + sustain + gap;
        } else {
          activeState.zoneTimers[s] = millis() + 100;
        }
      }
      zoneIntensity[s] = currentZoneIntensity[s];
    }
  }

  switch (activeState.mode) {
  case MODE_SOLID:
  case MODE_DUAL:
  case MODE_BURST:
  case MODE_RAINBOW:
  case MODE_FADE:
  case MODE_STROBE:
  case MODE_STEPPER:
    for (int s = 0; s < 5; s++)
      zoneIntensity[s] = 255;
    break;

  case MODE_CANDLES: {
    static uint32_t lastFlicker = 0;
    static uint8_t f1 = 255, f2 = 255;
    if (millis() - lastFlicker > 60) {
      f1 = random8(60, 255);
      f2 = random8(40, 255);
      lastFlicker = millis();
    }
    zoneIntensity[0] = zoneIntensity[1] = zoneIntensity[3] = zoneIntensity[4] =
        f1;
    zoneIntensity[2] = f2;
  } break;

  case MODE_PULSE_Z3:
    zoneIntensity[0] = zoneIntensity[1] = zoneIntensity[3] = zoneIntensity[4] =
        60;
    zoneIntensity[2] = beatsin8(40, 40, 255, activeState.triggerTime);
    break;

  case MODE_BLOOD_ORANGE_CYCLE: {
    zoneIntensity[0] = beatsin8(30, 0, 255, activeState.triggerTime, 64);
    zoneIntensity[4] = beatsin8(30, 0, 255, activeState.triggerTime, 192);
    uint32_t localMs = elapsed % 1250;
    uint8_t mid = 0;
    if (localMs < 125)
      mid = map(localMs, 0, 125, 0, 255);
    else if (localMs < 1125)
      mid = beatsin8(60, 128, 255, activeState.triggerTime + 125, 64);
    else
      mid = map(localMs, 1125, 1250, 255, 0);
    zoneIntensity[1] = zoneIntensity[2] = zoneIntensity[3] = mid;
  } break;

  case MODE_CYAN_DIM_COMET:
    for (int s = 0; s < 5; s++)
      zoneIntensity[s] = beatsin8(40, 40, 192, activeState.triggerTime);
    break;

  case MODE_DUAL_COLOR_FADE:
  case MODE_TWO_COLOR_TOGGLE:
  case MODE_COLOR_ROTATE:
  case MODE_PULSE_BURST:
  case MODE_ZONE_ALTERNATE:
    for (int s = 0; s < 5; s++)
      zoneIntensity[s] = 255;
    break;

  default:
    break;
  }

  CRGB color = CRGB::White;
  if (activeState.mode == MODE_BLOOD_ORANGE_CYCLE)
    color = CRGB(255, 60, 0);
  else if (activeState.mode == MODE_CYAN_DIM_COMET)
    color = CRGB::Cyan;

  for (int i = 0; i < NUM_LEDS; i++) {
    uint8_t z = zoneMap[i];
    if (z >= 5) {
      leds[i] = CRGB::Black;
      continue;
    }

    CRGB targetColor = color;
    if (activeState.mode == MODE_SOLID || activeState.mode == MODE_DUAL ||
        activeState.mode == MODE_BURST ||
        activeState.mode == MODE_FLASH_CHOOSE ||
        activeState.mode == MODE_ZONE_TWINKLE) {
      targetColor = activeState.colors[z];

      if (activeState.mode == MODE_FLASH_CHOOSE &&
          activeState.durationMs > 2000) {
        // Smoothly blend color towards black as it decays
        targetColor =
            blend(CRGB::Black, activeState.colors[z], zoneIntensity[z]);
      }
    } else if (activeState.mode == MODE_STEPPER)
      targetColor = activeState.stepColors[z][(elapsed / 500) % 3];
    else if (activeState.mode == MODE_RAINBOW)
      targetColor = CHSV(activeState.stepHues[z] + (elapsed * 255 / 3000) % 256,
                         255, 255);
    else if (activeState.mode == MODE_FADE)
      targetColor = blend(CRGB(5, 2, 10), CRGB::White,
                          map(elapsed % 1500, 0, 1500, 255, 0));
    else if (activeState.mode == MODE_STROBE) {
      uint32_t cycle = elapsed % 5000;
      targetColor = (cycle < 500) ? CRGB::Yellow
                                  : (cycle < 1000 ? CRGB::Orange : CRGB::Black);
    } else if (activeState.mode == MODE_CANDLES)
      targetColor = (z == 2) ? CRGB(255, 60, 0) : CRGB::Blue;
    else if (activeState.mode == MODE_PULSE_Z3)
      targetColor = (z == 2) ? CRGB::DeepPink : CRGB::DarkBlue;
    else if (activeState.mode == MODE_DUAL_COLOR_FADE) {
      uint8_t ratio = beatsin8(25, 0, 255, activeState.triggerTime, 192);
      CRGB c1 = activeState.colors[0];
      CRGB c2 = activeState.colors[1];
      targetColor =
          (z == 2) ? blend(c1, c2, 255 - ratio) : blend(c1, c2, ratio);
    }
    else if (activeState.mode == MODE_PULSE_BURST) {
      // 2 pulses over 1s, then 0.5s off = 1.5s cycle
      uint32_t pos = elapsed % 1500;
      if (pos < 1000) {
        // 2 triangle-wave pulses, each 500ms
        uint32_t subPos = pos % 500;
        uint8_t brightness;
        if (subPos < 250)
          brightness = map(subPos, 0, 250, 0, 255);
        else
          brightness = map(subPos, 250, 500, 255, 0);
        targetColor = blend(CRGB::Black, activeState.colors[z], brightness);
      } else {
        targetColor = CRGB::Black; // 0.5s pause
      }
    } else if (activeState.mode == MODE_TWO_COLOR_TOGGLE) {
      // Find two distinct colors and oscillate between them
      CRGB colorA = activeState.colors[0];
      CRGB colorB = colorA;
      for (int c = 1; c < 5; c++) {
        if (activeState.colors[c].r != colorA.r ||
            activeState.colors[c].g != colorA.g ||
            activeState.colors[c].b != colorA.b) {
          colorB = activeState.colors[c];
          break;
        }
      }
      uint8_t phase = beatsin8(60, 0, 255, activeState.triggerTime);
      targetColor = (phase < 128) ? colorA : colorB;
    }
    else if (activeState.mode == MODE_ZONE_ALTERNATE) {
      // 1s cycle: 500ms Group A (Z1,Z4), 500ms Group B (Z2,Z5)
      bool groupAOn = (elapsed % 1000) < 500;
      if (z == 0 || z == 3)
        targetColor = groupAOn ? activeState.colors[z] : CRGB::Black;
      else if (z == 1 || z == 4)
        targetColor = groupAOn ? CRGB::Black : activeState.colors[z];
      else
        targetColor = activeState.colors[z]; // Z3 center
    } else if (activeState.mode == MODE_COLOR_ROTATE) {
      // Rotation order: Z2(1) -> Z3(2) -> Z4(3) -> Z5(4) -> Z1(0) -> Z2...
      static const uint8_t zoneToRotPos[] = {4, 0, 1, 2, 3};
      // Colors stored in zone order [Z1..Z5], rotation reads them in
      // [Z2,Z3,Z4,Z5,Z1] order
      static const uint8_t rotToZone[] = {1, 2, 3, 4, 0};
      uint32_t cycleMs = 750;
      uint32_t cycleElapsed = elapsed % cycleMs;
      uint32_t stepMs = cycleMs / 5; // 100ms per step
      uint8_t step = cycleElapsed / stepMs;
      uint8_t frac = map(cycleElapsed % stepMs, 0, stepMs, 0, 255);
      uint8_t pos = zoneToRotPos[z];
      uint8_t srcZone = rotToZone[(pos + step) % 5];
      uint8_t nxtZone = rotToZone[(pos + step + 1) % 5];
      targetColor =
          blend(activeState.colors[srcZone], activeState.colors[nxtZone], frac);
    }

    leds[i] = targetColor;
    uint8_t intensity = zoneIntensity[z];

    if (activeState.mode == MODE_CYAN_DIM_COMET) {
      if (i < 40) {
        static const uint8_t path[] = {19, 18, 17, 16, 15, 14, 13, 12, 11, 10,
                                       9,  8,  7,  6,  5,  4,  3,  2,  1,  0};
        uint32_t step = (elapsed % 750) * 20 / 750;
        uint8_t mI = i % 20;
        uint8_t pos = 255;
        for (uint8_t k = 0; k < 20; k++) {
          if (path[k] == mI) {
            pos = k;
            break;
          }
        }
        if (pos != 255) {
          int32_t dist = (int32_t)step - (int32_t)pos;
          if (dist < 0)
            dist += 20;
          if (dist >= 0 && dist < 6) {
            intensity = qadd8(intensity, 255 - (dist * 42));
          }
        }
      }
    }

    leds[i].nscale8(intensity);
    leds[i].nscale8(masterIntensity);
  }

  if (activeState.mode == MODE_COMET) {
    FastLED.clear();
    uint16_t p1 = (elapsed % 4000) * NUM_LEDS / 4000;
    uint16_t p2 = (p1 + NUM_LEDS / 2) % NUM_LEDS;
    for (int i = 0; i < NUM_LEDS / 4; i++) {
      uint8_t f = 255 - (i * 255 / (NUM_LEDS / 4));
      leds[(p1 - i + NUM_LEDS) % NUM_LEDS] = CRGB::Green;
      leds[(p1 - i + NUM_LEDS) % NUM_LEDS].nscale8(f);
      leds[(p2 - i + NUM_LEDS) % NUM_LEDS] = CRGB::Green;
      leds[(p2 - i + NUM_LEDS) % NUM_LEDS].nscale8(f);
    }
  } else if (activeState.mode == MODE_WILD_SPARKLE) {
    for (int i = 0; i < NUM_LEDS; i++)
      if (zoneMap[i] < 5) {
        if (random8() < 25)
          leds[i] = CRGB::White;
        else
          leds[i].fadeToBlackBy(60);
      }
  } else if (activeState.mode == MODE_YELLOW_DOUBLE_COMET ||
             activeState.mode == MODE_GREEN_DOUBLE_COMET) {
    FastLED.clear();
    CRGB cometColor = (activeState.mode == MODE_YELLOW_DOUBLE_COMET)
                          ? CRGB::Yellow
                          : CRGB::Green;
    uint16_t p1 = (elapsed % 1000) * NUM_LEDS / 1000;
    uint16_t p2 = (p1 + NUM_LEDS / 2) % NUM_LEDS;
    for (int i = 0; i < 8; i++) {
      uint8_t f = 255 - (i * 30);
      leds[(p1 - i + NUM_LEDS) % NUM_LEDS] = cometColor;
      leds[(p1 - i + NUM_LEDS) % NUM_LEDS].nscale8(f);
      leds[(p2 - i + NUM_LEDS) % NUM_LEDS] = cometColor;
      leds[(p2 - i + NUM_LEDS) % NUM_LEDS].nscale8(f);
    }
  }
  FastLED.show();
}

void setup() {
  Serial.begin(115200);
  pinMode(ONBOARD_LED_PIN, OUTPUT);
  pinMode(MOSFET_PIN, OUTPUT);
  digitalWrite(MOSFET_PIN, HIGH);
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS)
      .setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);
  FastLED.clear();
  FastLED.show();
  if (!LittleFS.begin(true))
    Serial.println("[FS] Error");
  loadLogs();

  for (int i = 0; i < NUM_LEDS; i++) {
    zoneMap[i] = 255;
    cometMap[i] = 255;
  }

  // --- MANUAL ZONE MAPPING (1-5) ---
  setZoneRange(1, 0, 4);
  setZoneRange(1, 20, 24);
  setZoneRange(2, 5, 9);
  setZoneRange(2, 25, 29);
  setZoneRange(3, 40, 49);
  setZoneRange(4, 10, 14);
  setZoneRange(4, 30, 34);
  setZoneRange(5, 15, 19);
  setZoneRange(5, 35, 39);

  // --- STRICT COMET PATH (19 -> 0, Mirrored) ---
  for (int i = 0; i < 5; i++) {
    cometMap[19 - i] = i;
    cometMap[39 - i] = i;
  }

  NimBLEDevice::init("MB-Scanner-Listener");
  NimBLEScan *pScan = NimBLEDevice::getScan();
  pScan->setAdvertisedDeviceCallbacks(new MyDescriptiveCallbacks(), false);
  pScan->setActiveScan(true);
  pScan->setInterval(200);
  pScan->setWindow(100);
  pScan->start(0, nullptr, false);
}

void loop() {
  handleSerialCommands();
  updateAnimations();
  if (indicatorActive && (millis() - indicatorTimer > 200)) {
    indicatorActive = false;
    digitalWrite(ONBOARD_LED_PIN, !LED_ACTIVE_STATE);
  }
  delay(1);
}
