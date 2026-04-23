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
#define BRIGHTNESS 64

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

void setZoneRange(uint8_t zone, int start, int end) {
  uint8_t zIdx = (zone > 0) ? zone - 1 : 0; // Convert 1-indexed to 0-indexed
  for (int i = start; i <= end; i++) {
    if (i >= 0 && i < NUM_LEDS && zIdx < 5) {
      zoneMap[i] = zIdx;
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
  MODE_YELLOW_DOUBLE_COMET
};

struct AnimationState {
  AnimationMode mode;
  uint32_t durationMs;
  uint32_t fadeOutMs;
  bool isAlwaysOn;
  CRGB colors[5];
  uint8_t colorIndices[5];
  CRGB stepColors[5][3];
  uint8_t stepHues[5];
  uint32_t zoneTimers[5];
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
  bool alwaysOnBit = (b >> 7) & 0x01;
  float secs = 1.75f * value - 0.25f;
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

    String showCode = "";
    for (size_t i = actionIdx; i < mfgData.length(); i++) {
      uint8_t b = (uint8_t)mfgData[i];
      if (b < 0x10)
        showCode += "0";
      showCode += String(b, HEX);
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
      nextState.mode = MODE_COMET;
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
      } else if (s0 == 0x4F && s1 == 0x4F && s2 == 0x5B) {
        nextState.mode = MODE_STROBE;
      } else if (s0 == 0xB1 && s1 == 0xB9 && s2 == 0xB5) {
        nextState.mode = MODE_STEPPER;
        CRGB palette[] = {CRGB::Red, CRGB::Green, CRGB::Yellow, CRGB::Blue};
        for (int s = 0; s < 5; s++) {
          for (int p = 0; p < 3; p++) {
            nextState.stepColors[s][p] = palette[random8(4)];
          }
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
      }
      triggered = true;
    } else if (subAction == 0x0F) {
      if (showCode.indexOf("2a0717b8") != -1)
        nextState.mode = MODE_CANDLES;
      else if (showCode.indexOf("021200b0") != -1)
        nextState.mode = MODE_PULSE_Z3;
      triggered = true;
    } else if (subAction == 0x10) {
      if (showCode.indexOf("2102") != -1) {
        nextState.mode = MODE_BLOOD_ORANGE_CYCLE;
        nextState.durationMs = 30000;
      } else if (showCode.indexOf("4e07b0") != -1)
        nextState.mode = MODE_CYAN_DIM_COMET;
      triggered = true;
    }

    if (triggered) {
      Serial.println("[BLE] Triggered: " + showCode);
      nextState.active = true;
      newCommandReceived = true;
      indicatorActive = true;
      indicatorTimer = millis();
      digitalWrite(ONBOARD_LED_PIN, LED_ACTIVE_STATE);
    } else {
      if (logUnknown(mfgData)) {
        nextState.mode = MODE_WILD_SPARKLE;
        nextState.durationMs = 20000;
      } else {
        nextState.mode = MODE_YELLOW_DOUBLE_COMET;
        nextState.durationMs = 5000;
      }
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
            "</p><a href='/download'>Download</a><br><a "
            "href='/clear'>Clear</a></body></html>";
        request->send(200, "text/html", html);
      });
      server.on("/download", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(LittleFS, LOG_FILE, "text/plain", true);
      });
      server.on("/clear", HTTP_GET, [](AsyncWebServerRequest *request) {
        LittleFS.remove(LOG_FILE);
        loggedPackets.clear();
        request->redirect("/");
      });
      server.begin();
      webServerStarted = true;
      Serial.print("[WEB] Started at http://");
      Serial.println(IP);
    } else if (cmd == "stopweb") {
      if (!webServerStarted)
        return;
      server.end();
      WiFi.softAPdisconnect(true);
      webServerStarted = false;
      Serial.println("[WEB] Server stopped.");
    }
  }
}

// --- ANIMATION CORE ---
void updateAnimations() {
  if (newCommandReceived) {
    activeState = nextState;
    newCommandReceived = false;
  }
  if (!activeState.active) {
    FastLED.clear();
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
    if (elapsed > fadeStart) {
      masterIntensity = map(elapsed, fadeStart, activeState.durationMs, 255, 0);
    }
  }

  switch (activeState.mode) {
  case MODE_SOLID:
  case MODE_DUAL:
  case MODE_BURST:
    for (int s = 0; s < 5; s++) {
      CRGB c = activeState.colors[s];
      c.nscale8(masterIntensity);
      for (int i = 0; i < NUM_LEDS; i++)
        if (zoneMap[i] == s)
          leds[i] = c;
    }
    break;

  case MODE_COMET:
    FastLED.clear();
    {
      uint16_t pos1 = (elapsed % 4000) * NUM_LEDS / 4000;
      uint16_t pos2 = (pos1 + NUM_LEDS / 2) % NUM_LEDS;
      for (int i = 0; i < NUM_LEDS / 4; i++) {
        uint8_t f = 255 - (i * 255 / (NUM_LEDS / 4));
        leds[(pos1 - i + NUM_LEDS) % NUM_LEDS] = CRGB::Green;
        leds[(pos1 - i + NUM_LEDS) % NUM_LEDS].nscale8(f);
        leds[(pos2 - i + NUM_LEDS) % NUM_LEDS] = CRGB::Green;
        leds[(pos2 - i + NUM_LEDS) % NUM_LEDS].nscale8(f);
      }
    }
    break;

  case MODE_RAINBOW: {
    uint8_t h = (elapsed * 255 / 3000) % 256;
    for (int i = 0; i < NUM_LEDS; i++) {
      leds[i] = CHSV(activeState.stepHues[zoneMap[i]] + h, 255, 255);
    }
  } break;

  case MODE_FADE: {
    uint32_t cycle = elapsed % 1500;
    CRGB c1 = CRGB::White, c2 = CRGB(5, 2, 10);
    uint8_t ratio = map(cycle, 0, 1500, 255, 0);
    CRGB c = blend(c2, c1, ratio);
    for (int i = 0; i < NUM_LEDS; i++)
      leds[i] = c;
  } break;

  case MODE_STROBE: {
    uint32_t cycle = elapsed % 5000;
    CRGB c = (cycle < 500) ? CRGB::Yellow
                           : (cycle < 1000 ? CRGB::Orange : CRGB::Black);
    for (int i = 0; i < NUM_LEDS; i++)
      leds[i] = c;
  } break;

  case MODE_STEPPER: {
    uint8_t idx = (elapsed / 500) % 3;
    for (int i = 0; i < NUM_LEDS; i++)
      leds[i] = activeState.stepColors[zoneMap[i]][idx];
  } break;

    case MODE_CANDLES: {
      // Use a ~60ms interval to slow down the flicker rate
      static uint32_t lastFlicker = 0;
      static uint8_t f1 = 255;
      static uint8_t f2 = 255;
      if (millis() - lastFlicker > 60) {
        f1 = random8(60, 255);  // Wider range for more "on/off" feel
        f2 = random8(40, 255);
        lastFlicker = millis();
      }
      CRGB blue = CRGB::Blue;
      blue.nscale8(f1);
      CRGB orange = CRGB(255, 60, 0);
      orange.nscale8(f2);
      for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = (zoneMap[i] == 2) ? orange : blue;
      }
    } break;

  case MODE_PULSE_Z3: {
    CRGB bg = CRGB::DarkBlue;
    bg.nscale8(60);
    uint8_t p = beatsin8(40, 40, 255, activeState.triggerTime);
    CRGB pink = CRGB::DeepPink;
    pink.nscale8(p);
    for (int i = 0; i < NUM_LEDS; i++) {
      leds[i] = (zoneMap[i] == 2) ? pink : bg;
    }
  } break;

    case MODE_BLOOD_ORANGE_CYCLE: {
      CRGB color = CRGB(255, 60, 0); // Blood Orange
      
      // Zones 1 and 5: Pulse opposite over 2 seconds (30 BPM)
      // p1 starts bright, p2 starts dim.
      uint8_t p1 = beatsin8(30, 0, 255, activeState.triggerTime, 64);  // Phase 64 = Peak
      uint8_t p2 = beatsin8(30, 0, 255, activeState.triggerTime, 192); // Phase 192 = Trough
      
      // Zones 2, 3, 4: 1.25s cycle (125ms fade in, 1000ms pulse, 125ms fade out)
      uint32_t localMs = elapsed % 1250;
      uint8_t zMid = 0;
      if (localMs < 125) {
        zMid = map(localMs, 0, 125, 0, 255);
      } else if (localMs < 1125) {
        // Dim to 50% (128) and back to Full (255) over 1s.
        // Phase 64 is peak (255), Phase 192 is trough (128).
        zMid = beatsin8(60, 128, 255, activeState.triggerTime + 125, 64);
      } else {
        zMid = map(localMs, 1125, 1250, 255, 0);
      }

      for (int i = 0; i < NUM_LEDS; i++) {
        uint8_t z = zoneMap[i];
        if (z == 0) leds[i] = color.nscale8(p1);      // Zone 1
        else if (z == 4) leds[i] = color.nscale8(p2); // Zone 5
        else if (z >= 1 && z <= 3) leds[i] = color.nscale8(zMid); // Zones 2, 3, 4
        else leds[i] = CRGB::Black; // Safety for unmapped LEDs
      }
    } break;

  case MODE_CYAN_DIM_COMET:
    FastLED.clear();
    {
      uint16_t p = (elapsed % 1200) * NUM_LEDS / 1200;
      for (int i = 0; i < 15; i++) {
        uint8_t f = 255 - (i * 17);
        leds[(p - i + NUM_LEDS) % NUM_LEDS] = CRGB::Cyan;
        leds[(p - i + NUM_LEDS) % NUM_LEDS].nscale8(f);
      }
    }
    break;

  case MODE_WILD_SPARKLE:
    for (int i = 0; i < NUM_LEDS; i++) {
      if (random8() < 25)
        leds[i] = CRGB::White;
      else
        leds[i].fadeToBlackBy(60);
    }
    break;

  case MODE_YELLOW_DOUBLE_COMET:
    FastLED.clear();
    {
      uint16_t p1 = (elapsed % 1000) * NUM_LEDS / 1000;
      uint16_t p2 = (p1 + NUM_LEDS / 2) % NUM_LEDS;
      for (int i = 0; i < 8; i++) {
        uint8_t f = 255 - (i * 30);
        leds[(p1 - i + NUM_LEDS) % NUM_LEDS] = CRGB::Yellow;
        leds[(p1 - i + NUM_LEDS) % NUM_LEDS].nscale8(f);
        leds[(p2 - i + NUM_LEDS) % NUM_LEDS] = CRGB::Yellow;
        leds[(p2 - i + NUM_LEDS) % NUM_LEDS].nscale8(f);
      }
    }
    break;

  case MODE_FLASH_CHOOSE:
  case MODE_ZONE_TWINKLE:
    FastLED.clear();
    for (int s = 0; s < 5; s++) {
      if (!activeState.zoneFlashing[s]) {
        if (random8() < (activeState.mode == MODE_ZONE_TWINKLE ? 2 : 32)) {
          activeState.zoneFlashing[s] = true;
          activeState.zoneTimers[s] = elapsed;
        }
      } else {
        uint32_t f = elapsed - activeState.zoneTimers[s];
        uint32_t d = (activeState.mode == MODE_ZONE_TWINKLE ? 1500 : 400);
        if (f > d)
          activeState.zoneFlashing[s] = false;
        else {
          CRGB c = activeState.colors[s];
          c.nscale8(map(f, 0, d, 255, 0));
          for (int i = 0; i < NUM_LEDS; i++)
            if (zoneMap[i] == s)
              leds[i] = c;
        }
      }
    }
    break;

  default:
    FastLED.clear();
    break;
  }
  FastLED.show();
}

// --- SETUP & LOOP ---
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

  // Initialize all LEDs to Zone 1 by default
  for (int i = 0; i < NUM_LEDS; i++) zoneMap[i] = 0;

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

  // Map the rest of the 148-LED strand to zones
  setZoneRange(1, 50, 69);
  setZoneRange(2, 70, 89);
  setZoneRange(3, 90, 109);
  setZoneRange(4, 110, 129);
  setZoneRange(5, 130, 147);

  NimBLEDevice::init("MB-Listener");
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
