#include "mqtt_link.h"

#include <Arduino.h>

#include <cstring>

#include "isrg_root_x1.h"

namespace terrarium::mqtt {

namespace {

constexpr uint16_t kBufferSize = 512;  // The state JSON is about 330 bytes.
constexpr uint16_t kKeepAliveS = 60;
constexpr uint16_t kSocketTimeoutS = 10;
constexpr uint32_t kHandshakeTimeoutS = 10;
constexpr char kOnline[] = "en-ligne";
constexpr char kOffline[] = "hors-ligne";

}  // namespace

void MqttLink::configure(const supervision::NetworkSettings& settings, const char* deviceId) {
  if (client_.connected()) client_.disconnect();
  std::strcpy(host_, settings.brokerHost);
  port_ = settings.brokerPort;
  std::strcpy(user_, settings.mqttUser);
  std::strcpy(password_, settings.mqttPassword);
  std::strncpy(clientId_, deviceId, sizeof(clientId_) - 1);
  configured_ = supervision::formatTopic(deviceId, "state", stateTopic_, sizeof(stateTopic_)) &&
                supervision::formatTopic(deviceId, "status", statusTopic_, sizeof(statusTopic_));

  tls_.setCACert(kIsrgRootX1);
  tls_.setHandshakeTimeout(kHandshakeTimeoutS);
  client_.setServer(host_, port_);
  client_.setBufferSize(kBufferSize);
  client_.setKeepAlive(kKeepAliveS);
  client_.setSocketTimeout(kSocketTimeoutS);
  backoff_.succeeded();
  wasConnected_ = false;
}

void MqttLink::loop(uint32_t nowMs, bool wifiUp) {
  if (!configured_) return;
  if (client_.connected()) {
    client_.loop();
    return;
  }
  if (wasConnected_) {
    wasConnected_ = false;
    Serial.println("MQTT deconnecte");
  }
  if (wifiUp && backoff_.ready(nowMs)) connect(nowMs);
}

void MqttLink::connect(uint32_t nowMs) {
  // The last will marks the board offline if the connection drops.
  if (client_.connect(clientId_, user_, password_, statusTopic_, 0, true, kOffline)) {
    backoff_.succeeded();
    wasConnected_ = true;
    client_.publish(statusTopic_, kOnline, true);
    Serial.printf("MQTT connecte a %s:%u\r\n", host_, port_);
    return;
  }
  backoff_.failed(nowMs);
  // state() < 0: network or TLS failure; > 0: refused by the broker (5 = bad account).
  Serial.printf("MQTT echec de connexion a %s:%u (code %d), nouvel essai plus tard\r\n", host_,
                port_, client_.state());
}

bool MqttLink::publishState(const char* json, size_t length) {
  return client_.connected() && client_.publish(stateTopic_, reinterpret_cast<const uint8_t*>(json),
                                                static_cast<unsigned int>(length), true);
}

}  // namespace terrarium::mqtt
