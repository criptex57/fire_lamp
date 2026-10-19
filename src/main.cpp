#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266mDNS.h>
#include <DNSServer.h>
#include <WiFiUdp.h>
#include "sync_version.h"
#include "lamp_registry.h"
#include "animation.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <FastLED.h>
#include "config.h"
#include "fire.h"
#include "fire_variants.h"
#include "effects.h"
#include "modes.h"
#include "touch_button.h"
#include "web_ui.generated.h"

ESP8266WebServer server(80);
DNSServer dns;
CRGB leds[LED_COUNT], effectFrame[LED_COUNT], transitionFrom[LED_COUNT];
AnimationClock animationClocks[MODE_COUNT];
uint32_t animationStepRemainder[MODE_COUNT] = {};
uint16_t speed[MODE_COUNT] = {100,100,100,100,100,100,100};
uint32_t transitionStarted = 0;
uint64_t lastVisualKey = UINT64_MAX;
uint32_t lastDisplay = 0, displayFrames = 0;
String deviceId, lampName, wifiSSID, wifiPassword;
bool powerOn = true, dirty = false, fsReady = false, apActive = false, mdnsReady = false;
LampMode mode = LampMode::Fire;
uint8_t brightness[MODE_COUNT] = {128, 128, 160, 128, 128, 160,128};
uint8_t saturation = 255;
uint16_t hue = 30;
uint16_t effectHue[MODE_COUNT] = {};
uint8_t effectSaturation[MODE_COUNT] = {255,255,255,255,255,255,255};
bool effectCustomColor[MODE_COUNT] = {};
uint32_t saveAt = 0, lastFrame = 0;
WiFiUDP syncUDP;
LampRegistry lampRegistry;
MDNSResponder::hMDNSServiceQuery lampQuery = nullptr;
uint32_t lastHello = 0;
bool helloSent = false;
void pollPresence();
void receivePresence(const char* id, const char* name, IPAddress address, bool direct = true) {
  if (!id || strncmp(id, "firelamp-", 9) || deviceId == id || address == WiFi.localIP()) return;
  IPAddress local=WiFi.localIP(), mask=WiFi.subnetMask();
  for (uint8_t i=0;i<4;++i) if ((address[i]&mask[i])!=(local[i]&mask[i])) return;
  lampRegistry.observe(id,name,uint32_t(address),millis(),direct);
}
void observeMDNS(MDNSResponder::MDNSServiceInfo info) {
  if (!info.hostDomainAvailable() || !info.IP4AddressAvailable()) return;
  String id=info.hostDomain();
  const int dot=id.indexOf('.');
  if (dot>=0) id=id.substring(0,dot);
  for (const auto& address : info.IP4Adresses()) receivePresence(id.c_str(),id.c_str(),address,false);
}
constexpr uint16_t SYNC_PORT = 4210;
SyncSession syncSession;
bool& syncEnabled = syncSession.enabled;
SyncVersion& syncConfigVersion = syncSession.config;
SyncVersion& syncStateVersion = syncSession.state;
bool syncPending = false;
uint32_t syncLastSent = 0;
uint8_t syncRepeats = 0;
void syncLocalChange();
void pollSync();
TouchButton touch;
int8_t touchPin = 4; // D2, user wiring; stored in device configuration
bool touchActiveLow = false, brighterOnNextHold = true;
int8_t holdDirection = 1;
bool validTouchPin(int pin) {
  return pin == -1 || pin == 5 || pin == 4 || pin == 14 || pin == 12 || pin == 13;
}
bool touchIsActive() {
  return touchPin >= 0 && (digitalRead(touchPin) == (touchActiveLow ? LOW : HIGH));
}
void setupTouch() {
  if (touchPin >= 0) pinMode(touchPin, touchActiveLow ? INPUT_PULLUP : INPUT);
  touch.reset(touchIsActive(), millis());
}
void markDirty() { dirty = true; saveAt = millis(); }
void factoryReset() {
  dirty = false;
  // Delete app configuration only; do not format the filesystem or touch firmware.
  bool ok = fsReady;
  if (ok && LittleFS.exists("/config.tmp")) ok = LittleFS.remove("/config.tmp");
  if (ok && LittleFS.exists("/config.json")) ok = LittleFS.remove("/config.json");
  if (!ok) { Serial.println("Reset failed: configuration storage unavailable"); return; }
  Serial.println("Factory reset: restarting into open access point");
  FastLED.clear(true);
  delay(100);
  ESP.restart();
}
void pollTouch() {
  if (touchPin < 0) return;
  const auto event = touch.poll(touchIsActive(), millis());
  if (event == TouchButton::Event::None) return;
  switch (event) {
    case TouchButton::Event::FactoryReset: factoryReset(); return;
    case TouchButton::Event::Single: powerOn = !powerOn; break;
    case TouchButton::Event::Double: mode = nextMode(mode); powerOn = true; break;
    case TouchButton::Event::Triple:
      mode = LampMode::Color; powerOn = true; saturation = 255; hue = (hue + 30) % 360;
      break;
    case TouchButton::Event::HoldStart: {
      powerOn = true;
      const uint8_t level = brightness[static_cast<uint8_t>(mode)];
      holdDirection = level <= 5 ? 1 : level >= 255 ? -1 : (brighterOnNextHold ? 1 : -1);
      brighterOnNextHold = holdDirection < 0;
      break;
    }
    case TouchButton::Event::HoldStep: {
      uint8_t& level = brightness[static_cast<uint8_t>(mode)];
      level = constrain(int(level) + holdDirection * 4, 5, 255);
      break;
    }
    default: break;
  }
  markDirty();
  syncLocalChange();
}

void writeState(JsonDocument& doc) {
  doc["id"] = deviceId;
  doc["name"] = lampName;
  doc["power"] = powerOn;
  doc["mode"] = modeName(mode);
  for (uint8_t i = 0; i < MODE_COUNT; ++i) doc[brightnessKey(static_cast<LampMode>(i))] = brightness[i];
  for (uint8_t i = 0; i < MODE_COUNT; ++i) doc[speedKey(static_cast<LampMode>(i))] = speed[i];
  doc["hue"] = hue;
  doc["saturation"] = saturation;
  const uint8_t current = static_cast<uint8_t>(mode);
  doc["effectHue"] = effectHue[current];
  doc["effectSaturation"] = effectSaturation[current];
  doc["effectCustomColor"] = effectCustomColor[current];
  doc["ip"] = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
  doc["connected"] = WiFi.status() == WL_CONNECTED;
  doc["syncEnabled"] = syncEnabled;
}
void jsonReply(int code, const JsonDocument& doc) {
  String body;
  serializeJson(doc, body);
  server.sendHeader("Cache-Control", "no-store");
  server.send(code, "application/json", body);
}
void error(int code, const char* message) {
  StaticJsonDocument<160> doc;
  doc["error"] = message;
  jsonReply(code, doc);
}
bool saveConfig() {
  if (!fsReady) return false;
  DynamicJsonDocument doc(3072);
  writeState(doc);
  doc["ssid"] = wifiSSID;
  doc["password"] = wifiPassword;
  doc["syncConfigCounter"] = syncConfigVersion.counter;
  doc["syncConfigOwner"] = syncConfigVersion.owner;
  doc["syncStateCounter"] = syncStateVersion.counter;
  doc["syncStateOwner"] = syncStateVersion.owner;
  doc["touchPin"] = touchPin;
  doc["touchActiveLow"] = touchActiveLow;
  JsonArray colors = doc.createNestedArray("effectColors");
  for (uint8_t i = 0; i < MODE_COUNT; ++i) {
    JsonArray color = colors.createNestedArray();
    color.add(effectHue[i]); color.add(effectSaturation[i]); color.add(effectCustomColor[i]);
  }
  File file = LittleFS.open("/config.tmp", "w");
  if (!file) return false;
  const size_t written = serializeJson(doc, file);
  file.close();
  return written > 0 && LittleFS.rename("/config.tmp", "/config.json");
}
void loadConfig() {
  if (!fsReady) return;
  File file = LittleFS.open("/config.json", "r");
  if (!file) return;
  DynamicJsonDocument doc(3072);
  if (deserializeJson(doc, file)) return;
  lampName = doc["name"] | lampName;
  wifiSSID = doc["ssid"] | "";
  wifiPassword = doc["password"] | "";
  syncEnabled = doc["syncEnabled"] | false;
  syncConfigVersion.counter = doc["syncConfigCounter"] | 0u;
  syncConfigVersion.owner = doc["syncConfigOwner"] | 0u;
  syncStateVersion.counter = doc["syncStateCounter"] | 0u;
  syncStateVersion.owner = doc["syncStateOwner"] | 0u;
  powerOn = doc["power"] | true;
  parseMode(doc["mode"] | "fire", mode);
  for (uint8_t i = 0; i < MODE_COUNT; ++i) brightness[i] = constrain(doc[brightnessKey(static_cast<LampMode>(i))] | int(brightness[i]), 0, 255);
  for (uint8_t i = 0; i < MODE_COUNT; ++i) speed[i] = constrain(doc[speedKey(static_cast<LampMode>(i))] | 100, 25, 300);
  hue = constrain(doc["hue"] | 30, 0, 359);
  saturation = constrain(doc["saturation"] | 255, 0, 255);
  int configuredPin = doc["touchPin"] | 4;
  touchPin = validTouchPin(configuredPin) ? configuredPin : -1;
  touchActiveLow = doc["touchActiveLow"] | false;
  for (uint8_t i = 0; i < MODE_COUNT; ++i) {
    effectHue[i] = constrain(doc["effectColors"][i][0] | 0, 0, 359);
    effectSaturation[i] = constrain(doc["effectColors"][i][1] | 255, 0, 255);
    effectCustomColor[i] = doc["effectColors"][i][2] | false;
  }
}
bool validNumber(JsonVariantConst v, int maxValue) {
  return v.is<int>() && v.as<int>() >= 0 && v.as<int>() <= maxValue;
}
bool validStatePatch(JsonObjectConst doc) {
  LampMode requestedMode = mode;
  for (JsonPairConst p : doc) {
    String key = p.key().c_str();
    JsonVariantConst v = p.value();
    bool valid = false;
    if (key == "effectCustomColor") valid = v.is<bool>();
    else if (key == "effectHue") valid = validNumber(v, 359);
    else if (key == "effectSaturation") valid = validNumber(v, 255);
    else if (key == "power") valid = v.is<bool>();
    else if (key == "mode") valid = v.is<const char*>() && parseMode(v.as<const char*>(), requestedMode);
    else if (key == "name") valid = v.is<const char*>() && strlen(v.as<const char*>()) > 0 && strlen(v.as<const char*>()) <= 48;
    else if (key == "hue") valid = validNumber(v, 359);
    else if (key == "saturation") valid = validNumber(v, 255);
    else for (uint8_t i = 0; i < MODE_COUNT; ++i) {
      if (key == brightnessKey(static_cast<LampMode>(i))) { valid = validNumber(v, 255); break; }
      if (key == speedKey(static_cast<LampMode>(i))) { valid = validNumber(v, 300) && v.as<int>() >= 25; break; }
    }
    if (!valid) return false;
  }
  return true;
}
void applyStatePatch(JsonObjectConst doc) {
  if (doc.containsKey("power")) powerOn = doc["power"];
  if (doc.containsKey("mode")) parseMode(doc["mode"].as<const char*>(), mode);
  const uint8_t current = static_cast<uint8_t>(mode);
  if (doc.containsKey("effectHue")) effectHue[current] = doc["effectHue"];
  if (doc.containsKey("effectSaturation")) effectSaturation[current] = doc["effectSaturation"];
  if (doc.containsKey("effectCustomColor")) effectCustomColor[current] = doc["effectCustomColor"];
  if (doc.containsKey("name")) { lampName = doc["name"].as<String>(); helloSent=false; }
  if (doc.containsKey("hue")) hue = doc["hue"];
  if (doc.containsKey("saturation")) saturation = doc["saturation"];
  for (uint8_t i = 0; i < MODE_COUNT; ++i) {
    const char* key = brightnessKey(static_cast<LampMode>(i));
    if (doc.containsKey(key)) brightness[i] = doc[key];
    const char* velocity = speedKey(static_cast<LampMode>(i));
    if (doc.containsKey(velocity)) speed[i] = doc[velocity];
  }
}
void updateState() {
  if (server.arg("plain").length() > 1024) return error(413, "Request too large");
  DynamicJsonDocument doc(1536);
  if (deserializeJson(doc, server.arg("plain")) || !doc.is<JsonObject>()) return error(400, "Invalid JSON object");
  if (!validStatePatch(doc.as<JsonObjectConst>())) return error(400, "Unknown field or invalid value");
  applyStatePatch(doc.as<JsonObjectConst>());
  markDirty();
  if (doc.size() > (doc.containsKey("name") ? 1u : 0u)) syncLocalChange();
  DynamicJsonDocument result(1280);
  writeState(result);
  jsonReply(200, result);
}
// Broadcast settings and light state; received messages are never immediately relayed.
// Monotonic revisions prevent duplicates, stale packets and toggle/command loops.
void syncLocalChange() {
  if (!syncEnabled) return;
  syncSession.localChange(ESP.getChipId());
  syncPending = true;
  syncRepeats = 3;
}
void sendSync() {
  StaticJsonDocument<1024> doc;
  doc["protocol"] = 1;
  doc["id"] = deviceId;
  doc["name"] = lampName;
  doc["configCounter"] = syncConfigVersion.counter;
  doc["configOwner"] = syncConfigVersion.owner;
  doc["enabled"] = syncEnabled;
  doc["stateCounter"] = syncStateVersion.counter;
  doc["stateOwner"] = syncStateVersion.owner;
  if (syncEnabled) {
    JsonObject state = doc.createNestedObject("state");
    state["power"] = powerOn; state["mode"] = modeName(mode);
    state["hue"] = hue; state["saturation"] = saturation;
    const uint8_t current = static_cast<uint8_t>(mode);
    state[brightnessKey(mode)] = brightness[current];
    doc["speed"] = speed[current]; // Optional extension: older receivers ignore it.
    state["effectHue"] = effectHue[current];
    state["effectSaturation"] = effectSaturation[current];
    state["effectCustomColor"] = effectCustomColor[current];
  }
  IPAddress address = WiFi.localIP(), mask = WiFi.subnetMask();
  for (uint8_t i = 0; i < 4; ++i) address[i] |= uint8_t(~mask[i]);
  if (syncUDP.beginPacket(address, SYNC_PORT)) {
    serializeJson(doc, syncUDP);
    syncUDP.endPacket();
  }
  syncLastSent = millis();
}
void pollSync() {
  if (WiFi.status() != WL_CONNECTED) return;
  const int length = syncUDP.parsePacket();
  if (length) {
    if (length > 768) { syncUDP.flush(); return; }
    IPAddress source = syncUDP.remoteIP(), local = WiFi.localIP(), mask = WiFi.subnetMask();
    bool localSubnet = source != local;
    for (uint8_t i = 0; i < 4; ++i) localSubnet &= (source[i] & mask[i]) == (local[i] & mask[i]);
    static char packet[769];
    const int read = syncUDP.read(packet, 768);
    if (!localSubnet || read <= 0) return;
    DynamicJsonDocument doc(1536);
    if (deserializeJson(doc, packet, read)) return;
    if ((doc["discovery"] == 1 || doc["protocol"] == 1) && doc["id"].is<const char*>() && doc["name"].is<const char*>()) {
      receivePresence(doc["id"],doc["name"],source);
      if (doc["discovery"] == 1) return;
    }
    if (doc["protocol"] != 1 ||
        !doc["configCounter"].is<uint32_t>() || !doc["configOwner"].is<uint32_t>() ||
        !doc["stateCounter"].is<uint32_t>() || !doc["stateOwner"].is<uint32_t>() ||
        !doc["enabled"].is<bool>()) return;
    SyncVersion config, state;
    config.counter = doc["configCounter"]; config.owner = doc["configOwner"];
    state.counter = doc["stateCounter"]; state.owner = doc["stateOwner"];
    if (!config.counter || (!config.newerThan(syncConfigVersion) && !config.equals(syncConfigVersion))) return;
    const bool enabled = doc["enabled"];
    JsonObjectConst patch = doc["state"].as<JsonObjectConst>();
    if (enabled && (patch.isNull() || !patch["power"].is<bool>() || !patch["mode"].is<const char*>() ||
                    patch.containsKey("name") || !validStatePatch(patch))) return;
    if (doc.containsKey("speed") && (!validNumber(doc["speed"], 300) || doc["speed"].as<int>() < 25)) return;
    const auto accepted = syncSession.receive(config, state, enabled);
    if (accepted.configChanged) {
      syncPending = false; syncRepeats = 0;
      markDirty();
    }
    if (accepted.stateChanged) {
      applyStatePatch(patch);
      if (doc.containsKey("speed")) speed[static_cast<uint8_t>(mode)] = doc["speed"];
      markDirty();
    }
  }
  const uint32_t elapsed = millis() - syncLastSent;
  if (syncPending && elapsed >= 120) {
    sendSync();
    if (--syncRepeats == 0) syncPending = false;
  } else if (syncConfigVersion.counter && elapsed >= 4000) sendSync();
}
void configureSync() {
  StaticJsonDocument<128> doc;
  if (server.arg("plain").length() > 96 || deserializeJson(doc, server.arg("plain")) ||
      !doc["enabled"].is<bool>()) return error(400, "Expected enabled boolean");
  syncSession.configure(doc["enabled"].as<bool>(), ESP.getChipId());
  syncPending = true; syncRepeats = 3;
  markDirty();
  StaticJsonDocument<128> result;
  result["enabled"] = syncEnabled;
  result["connected"] = WiFi.status() == WL_CONNECTED;
  jsonReply(200, result);
}
// Proxy only to another IPv4 lamp on our station subnet; fixed destination path.
// Browser talks to its own lamp, so no cross-origin/private-network browser restrictions.
void remoteState() {
  IPAddress target;
  if (WiFi.status() != WL_CONNECTED || !target.fromString(server.arg("host"))) return error(400, "Expected lamp IPv4 address");
  IPAddress local = WiFi.localIP(), mask = WiFi.subnetMask();
  bool sameSubnet = true, network = true, broadcast = true;
  for (int i = 0; i < 4; ++i) {
    sameSubnet &= (target[i] & mask[i]) == (local[i] & mask[i]);
    network &= (target[i] & ~mask[i]) == 0;
    broadcast &= (target[i] | mask[i]) == 255;
  }
  if (!sameSubnet || network || broadcast || target == local || target[0] >= 224 || target[0] == 127) return error(400, "Address must be another lamp on the same subnet");
  String body = server.arg("plain");
  if (body.length() > 1024) return error(413, "Request too large");
  WiFiClient client;
  HTTPClient http;
  http.setTimeout(800);
  http.useHTTP10(true);
  if (!http.begin(client, "http://" + target.toString() + "/api/state")) return error(502, "Connection failed");
  int status;
  if (server.method() == HTTP_POST) {
    http.addHeader("Content-Type", "application/json");
    status = http.POST(body);
  } else status = http.GET();
  if (status < 0) { http.end(); return error(504, "Lamp unavailable"); }
  if (http.getSize() > 2048) { http.end(); return error(502, "Unexpected response"); }
  DynamicJsonDocument result(1536);
  auto parseError = deserializeJson(result, http.getStream());
  http.end();
  if (parseError) return error(502, "Invalid lamp response");
  jsonReply(status, result);
}
void pollPresence() {
  if (WiFi.status()!=WL_CONNECTED) return;
  if (helloSent && uint32_t(millis()-lastHello)<5000) return;
  StaticJsonDocument<256> doc;
  doc["discovery"]=1;doc["id"]=deviceId;doc["name"]=lampName;
  IPAddress address=WiFi.localIP(),mask=WiFi.subnetMask();
  for(uint8_t i=0;i<4;++i) address[i]|=uint8_t(~mask[i]);
  if(syncUDP.beginPacket(address,SYNC_PORT)) {serializeJson(doc,syncUDP);syncUDP.endPacket();}
  lastHello=millis();helloSent=true;
}
void discover() {
  // Dynamic mDNS query runs in the background; no blocking network scan here.
  if (lampQuery && WiFi.status()==WL_CONNECTED) {
    for (const auto& info : MDNS.answerInfo(lampQuery)) observeMDNS(info);
  }
  DynamicJsonDocument doc(4096);
  JsonArray lamps=doc.createNestedArray("lamps");
  JsonObject self=lamps.createNestedObject();
  self["id"]=deviceId;self["name"]=lampName;self["self"]=true;self["online"]=true;
  self["host"]=WiFi.status()==WL_CONNECTED?WiFi.localIP().toString():WiFi.softAPIP().toString();
  if (WiFi.status()==WL_CONNECTED) for (const auto& entry : lampRegistry.entries) {
    if (!lampRegistry.online(entry,millis())) continue;
    JsonObject lamp=lamps.createNestedObject();
    lamp["id"]=entry.id;lamp["name"]=entry.name;lamp["host"]=IPAddress(entry.address).toString();
    lamp["self"]=false;lamp["online"]=true;
  }
  jsonReply(200,doc);
}
void scanWiFi() {
  int count = WiFi.scanComplete();
  if (count == WIFI_SCAN_FAILED) count = WiFi.scanNetworks(true, false);
  if (count == WIFI_SCAN_RUNNING) {
    StaticJsonDocument<64> doc;
    doc["scanning"] = true;
    return jsonReply(202, doc);
  }
  if (count < 0) return error(503, "Wi-Fi scan failed");
  int indices[64];
  const int limit = min(count, 64);
  for (int i = 0; i < limit; ++i) {
    indices[i] = i;
    for (int j = i; j > 0 && WiFi.RSSI(indices[j]) > WiFi.RSSI(indices[j - 1]); --j) {
      const int tmp = indices[j]; indices[j] = indices[j - 1]; indices[j - 1] = tmp;
    }
  }
  DynamicJsonDocument doc(4096);
  doc["scanning"] = false;
  JsonArray networks = doc.createNestedArray("networks");
  for (int i = 0; i < limit && networks.size() < 20; ++i) {
    const int index = indices[i];
    const String ssid = WiFi.SSID(index);
    if (!ssid.length() || ssid == deviceId) continue;
    bool duplicate = false;
    for (JsonObject network : networks) {
      if (network["ssid"].as<String>() == ssid) { duplicate = true; break; }
    }
    if (duplicate) continue;
    JsonObject network = networks.createNestedObject();
    network["ssid"] = ssid;
    network["rssi"] = WiFi.RSSI(index);
    network["secured"] = WiFi.encryptionType(index) != ENC_TYPE_NONE;
  }
  jsonReply(200, doc);
  WiFi.scanDelete();
}
void setupWiFi() {
  WiFi.persistent(false);
  WiFi.mode(WIFI_AP_STA);
  WiFi.hostname(deviceId);
  WiFi.setAutoReconnect(true);
  if (!wifiSSID.length()) WiFi.disconnect();
  if (wifiSSID.length()) {
    WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
    uint32_t started = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - started < 12000) { delay(50); }
  }
  if (WiFi.status() != WL_CONNECTED) {
    apActive = WiFi.softAP(deviceId.c_str());
    dns.start(53, "*", WiFi.softAPIP());
  }
  mdnsReady = MDNS.begin(deviceId.c_str());
  if (mdnsReady) {
    MDNS.addService("firelamp", "tcp", 80);
    lampQuery=MDNS.installServiceQuery("firelamp","tcp",
      [](const MDNSResponder::MDNSServiceInfo& info, MDNSResponder::AnswerType, bool added) {
        if (added) observeMDNS(info);
      });
  }
}
void setup() {
  Serial.begin(9600);
  deviceId = "firelamp-" + String(ESP.getChipId(), HEX);
  lampName = "Лампа " + String(ESP.getChipId(), HEX);
  fsReady = LittleFS.begin();
  loadConfig();
  setupTouch();
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, LED_COUNT);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, MAX_MILLIAMPS);
  FastLED.clear(true);
  setupWiFi();
  server.on("/", HTTP_GET, [] { server.send_P(200, "text/html; charset=utf-8", WEB_UI); });
  server.on("/api/state", HTTP_GET, [] { DynamicJsonDocument doc(1280); writeState(doc); jsonReply(200, doc); });
  server.on("/api/state", HTTP_POST, updateState);
  server.on("/api/diagnostics", HTTP_GET, [] {
    StaticJsonDocument<256> doc;
    uint32_t energy = 0;
    for (const auto& led : leds) energy += uint16_t(led.r) + led.g + led.b;
    doc["frameEnergy"] = energy;
    doc["frames"] = displayFrames;
    doc["animationMs"] = animationClocks[static_cast<uint8_t>(mode)].time;
    doc["transitionActive"] = uint32_t(millis() - transitionStarted) < 700;
    jsonReply(200, doc);
  });
  server.on("/api/sync", HTTP_POST, configureSync);
  server.on("/api/remote", HTTP_GET, remoteState);
  server.on("/api/remote", HTTP_POST, remoteState);
  server.on("/api/discover", HTTP_GET, discover);
  server.on("/api/wifi/scan", HTTP_GET, scanWiFi);
  server.on("/api/wifi", HTTP_GET, [] {
    StaticJsonDocument<384> doc;
    doc["ssid"] = wifiSSID;
    doc["connected"] = WiFi.status() == WL_CONNECTED;
    doc["ip"] = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
    doc["hostname"] = deviceId;
    jsonReply(200, doc);
  });
  server.on("/api/button", HTTP_GET, [] {
    StaticJsonDocument<192> doc;
    doc["pin"] = touchPin;
    doc["activeLow"] = touchActiveLow;
    doc["pressed"] = touchIsActive();
    jsonReply(200, doc);
  });
  server.on("/api/button", HTTP_POST, [] {
    if (server.arg("plain").length() > 192) return error(413, "Request too large");
    StaticJsonDocument<256> doc;
    if (deserializeJson(doc, server.arg("plain")) || !doc["pin"].is<int>() ||
        !validTouchPin(doc["pin"].as<int>()) || !doc["activeLow"].is<bool>()) return error(400, "Invalid button configuration");
    const int8_t oldPin = touchPin;
    const bool oldPolarity = touchActiveLow;
    touchPin = doc["pin"]; touchActiveLow = doc["activeLow"];
    if (!saveConfig()) {
      touchPin = oldPin; touchActiveLow = oldPolarity;
      return error(500, "Could not save configuration");
    }
    if (oldPin >= 0) pinMode(oldPin, INPUT);
    setupTouch();
    server.send(200, "application/json", "{\"ok\":true}");
  });
  server.on("/api/wifi", HTTP_POST, [] {
    if (server.arg("plain").length() > 512) return error(413, "Request too large");
    StaticJsonDocument<768> doc;
    if (deserializeJson(doc, server.arg("plain")) || !doc["ssid"].is<const char*>() || !doc["password"].is<const char*>()) return error(400, "Expected ssid and password");
    String ssid = doc["ssid"], pass = doc["password"];
    if (!ssid.length() || ssid.length() > 32 || pass.length() > 63 || (pass.length() && pass.length() < 8)) return error(400, "Invalid Wi-Fi credentials");
    String oldSSID = wifiSSID, oldPassword = wifiPassword;
    wifiSSID = ssid; wifiPassword = pass;
    if (!saveConfig()) { wifiSSID = oldSSID; wifiPassword = oldPassword; return error(500, "Could not save configuration"); }
    server.send(200, "application/json", "{\"ok\":true,\"restarting\":true}");
    delay(500);
    ESP.restart();
  });
  server.onNotFound([] {
    if (apActive && !server.uri().startsWith("/api/")) { server.sendHeader("Location", "http://192.168.4.1/"); server.send(302); }
    else error(404, "Not found");
  });
  syncUDP.begin(SYNC_PORT);
  server.begin();
  Serial.println("\n" + deviceId + " http://" + (WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : WiFi.softAPIP().toString()));
  Serial.printf("Touch: GPIO%d, active %s, input %s\n", touchPin, touchActiveLow ? "LOW" : "HIGH", touchIsActive() ? "pressed" : "released");
}
void loop() {
  pollTouch();
  server.handleClient();
  pollTouch();
  if (apActive) dns.processNextRequest();
  if (mdnsReady) MDNS.update();
  pollPresence();
  pollSync();
  if (dirty && millis() - saveAt > 2000) {
    if (saveConfig()) dirty = false;
    else saveAt = millis();
  }
  const uint32_t now = millis();
  if (uint32_t(now - lastDisplay) >= 20) {
    const uint32_t realDelta = lastDisplay ? uint32_t(now - lastDisplay) : 20;
    lastDisplay = now;
    const uint8_t current = static_cast<uint8_t>(mode);
    const uint16_t chosenHue = mode == LampMode::Color ? hue : effectHue[current];
    const uint8_t chosenSat = mode == LampMode::Color ? saturation : effectSaturation[current];
    const uint64_t visualKey = uint64_t(current) | (uint64_t(powerOn) << 8) |
      (uint64_t(brightness[current]) << 9) | (uint64_t(chosenHue) << 17) |
      (uint64_t(chosenSat) << 26) | (uint64_t(effectCustomColor[current]) << 34);
    const bool switchedMode = (lastVisualKey & 255) != current;
    if (visualKey != lastVisualKey) {
      memcpy(transitionFrom, leds, sizeof(leds));
      transitionStarted = now;
      lastVisualKey = visualKey;
    }
    const uint32_t delta = animationClocks[current].advance(realDelta, speed[current]);
    const uint32_t effectTime = animationClocks[current].time;
    animationStepRemainder[current] += delta;
    uint8_t steps = min(uint32_t(8), animationStepRemainder[current] / FIRE_FRAME_MS);
    animationStepRemainder[current] %= FIRE_FRAME_MS;
    if (switchedMode && !steps) steps = 1;
    if (mode == LampMode::Fire) { while (steps--) renderFire(effectFrame); }
    else if (mode == LampMode::Fire2012) {
      while (steps--) renderFire2012(effectFrame);
    }
    else if (mode == LampMode::Aurora) { while (steps--) LampEffects::aurora(effectFrame); }
    else if (mode == LampMode::Plasma) LampEffects::plasma(effectFrame, effectTime);
    else if (mode == LampMode::Hiphotic) LampEffects::hiphotic(effectFrame, effectTime);
    else if (mode == LampMode::Pacifica) LampEffects::pacifica(effectFrame, effectTime);
    else {
      CRGB color;
      hsv2rgb_spectrum(CHSV(hue * 256UL / 360, saturation, 255), color);
      fill_solid(effectFrame, LED_COUNT, color);
    }
    const uint8_t mix = transitionMix(uint32_t(now - transitionStarted));
    for (uint16_t i = 0; i < LED_COUNT; ++i) {
      CRGB target = effectFrame[i];
      if (mode != LampMode::Color && effectCustomColor[current]) {
        const uint8_t value = max(target.r, max(target.g, target.b));
        hsv2rgb_spectrum(CHSV(effectHue[current] * 256UL / 360, effectSaturation[current], value), target);
      }
      target.nscale8(powerOn ? brightness[current] : 0);
      leds[i] = blend(transitionFrom[i], target, mix);
    }
    FastLED.setBrightness(255);
    FastLED.show();
    ++displayFrames;
  }
  yield();
}
