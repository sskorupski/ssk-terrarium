#include "supervisor.h"

#include <Arduino.h>
#include <WiFi.h>
#include <esp_mac.h>

namespace terrarium::network {

namespace {

constexpr uint32_t kTaskStackBytes = 10 * 1024;  // TLS handshake included.
constexpr UBaseType_t kTaskPriority = 1;
constexpr BaseType_t kTaskCore = 0;  // The Arduino loop runs on core 1.
constexpr uint32_t kStepMs = 20;
// The Wi-Fi driver reconnects by itself; this only restarts a join that gave up.
constexpr uint32_t kRejoinMs = 60UL * 1000UL;
constexpr uint8_t kDnsPort = 53;
constexpr size_t kStateBufferSize = 512;

}  // namespace

void Supervisor::begin() {
  uint8_t mac[6] = {};
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  supervision::formatDeviceId(mac, deviceId_);
  supervision::formatApSsid(mac, apSsid_);
  if (!store_.loadOrCreateApPassword(apPassword_)) {
    Serial.println("ERREUR mot de passe du reseau de configuration non enregistre");
  }
  hasSettings_ = store_.loadNetwork(settings_);

  Serial.printf("WIFI reseau de configuration %s, mot de passe %s, page http://192.168.4.1\r\n",
                apSsid_, apPassword_);
  Serial.printf("MQTT identifiant ssk-terrarium/%s\r\n", deviceId_);
  if (hasSettings_) {
    Serial.printf("WIFI enregistre : %s, courtier %s:%u\r\n", settings_.wifiSsid,
                  settings_.brokerHost, settings_.brokerPort);
    webUi_.showCurrent(settings_);
    mqtt_.configure(settings_, deviceId_);
  } else {
    Serial.println("WIFI aucun reseau enregistre : reseau de configuration ouvert");
  }

  WiFi.persistent(false);  // Settings live in our own NVS keys only.
  WiFi.setAutoReconnect(true);
  WiFi.mode(WIFI_STA);
  webUi_.begin();
  if (hasSettings_) joinWifi(millis());

  xTaskCreatePinnedToCore(taskEntry, "supervision", kTaskStackBytes, this, kTaskPriority, nullptr,
                          kTaskCore);
}

void Supervisor::publish(const supervision::StateSnapshot& snapshot) {
  portENTER_CRITICAL(&lock_);
  snapshot_ = snapshot;
  snapshotFresh_ = true;
  portEXIT_CRITICAL(&lock_);
}

void Supervisor::taskEntry(void* self) { static_cast<Supervisor*>(self)->run(); }

void Supervisor::run() {
  for (;;) {
    step(millis());
    vTaskDelay(pdMS_TO_TICKS(kStepMs));
  }
}

void Supervisor::step(uint32_t nowMs) {
  applyPosted(nowMs);
  if (portalRequested_.exchange(false)) plan_.requestPortal(nowMs);

  const bool connected = WiFi.status() == WL_CONNECTED;
  reportWifi(connected);
  setPortal(plan_.update(nowMs, hasSettings_, connected));
  if (portalOpen_) dns_.processNextRequest();
  if (hasSettings_ && !connected && nowMs - lastJoinMs_ >= kRejoinMs) joinWifi(nowMs);

  mqtt_.loop(nowMs, connected);
  publishPending();
}

void Supervisor::applyPosted(uint32_t nowMs) {
  supervision::NetworkSettings posted{};
  if (!webUi_.takePosted(posted)) return;
  settings_ = posted;
  hasSettings_ = true;
  if (store_.saveNetwork(settings_)) {
    Serial.printf("WIFI reglages enregistres : %s, courtier %s:%u\r\n", settings_.wifiSsid,
                  settings_.brokerHost, settings_.brokerPort);
  } else {
    Serial.println("ERREUR reglages reseau non enregistres, perdus a la prochaine coupure");
  }
  plan_.settingsSaved(nowMs);
  mqtt_.configure(settings_, deviceId_);
  WiFi.disconnect();
  joinWifi(nowMs);
}

void Supervisor::setPortal(bool open) {
  if (open == portalOpen_) return;
  portalOpen_ = open;
  if (open) {
    // Keep trying the saved Wi-Fi while the portal is up.
    WiFi.mode(hasSettings_ ? WIFI_AP_STA : WIFI_AP);
    WiFi.softAP(apSsid_, apPassword_);
    dns_.start(kDnsPort, "*", WiFi.softAPIP());
    Serial.printf("PORTAIL ouvert : reseau %s, page http://192.168.4.1\r\n", apSsid_);
  } else {
    dns_.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    Serial.println("PORTAIL ferme");
  }
}

void Supervisor::joinWifi(uint32_t nowMs) {
  lastJoinMs_ = nowMs;
  WiFi.begin(settings_.wifiSsid, settings_.wifiPassword);
}

void Supervisor::reportWifi(bool connected) {
  if (connected == wifiWasConnected_) return;
  wifiWasConnected_ = connected;
  if (connected) {
    Serial.printf("WIFI connecte a %s, adresse %s\r\n", settings_.wifiSsid,
                  WiFi.localIP().toString().c_str());
  } else {
    Serial.println("WIFI perdu, nouvel essai automatique");
  }
}

void Supervisor::publishPending() {
  if (!mqtt_.connected()) return;
  supervision::StateSnapshot snapshot{};
  portENTER_CRITICAL(&lock_);
  const bool fresh = snapshotFresh_;
  if (fresh) snapshot = snapshot_;
  snapshotFresh_ = false;
  portEXIT_CRITICAL(&lock_);
  if (!fresh) return;
  char json[kStateBufferSize];
  const size_t length = supervision::formatState(snapshot, json, sizeof(json));
  if (length > 0) mqtt_.publishState(json, length);
}

}  // namespace terrarium::network
