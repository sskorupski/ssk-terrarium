// Supervision task: Wi-Fi, configuration portal and MQTT, on core 0, apart
// from the regulation loop (INV-3, RG-regulation-sans-reseau). The loop only
// hands it a snapshot and requests; it never waits for it.
#pragma once

#include <DNSServer.h>
#include <freertos/FreeRTOS.h>

#include <atomic>
#include <cstdint>

#include "identity.h"
#include "mqtt_link.h"
#include "network_settings.h"
#include "settings_store.h"
#include "state_message.h"
#include "web_ui.h"
#include "wifi_plan.h"

namespace terrarium::network {

class Supervisor {
 public:
  explicit Supervisor(storage::SettingsStore& store) : store_(store) {}

  // From setup(): reads the saved settings, prints the access point name and
  // password and the device id on the serial link, then starts the task.
  void begin();

  // From the regulation loop, once per measurement cycle. Never blocks.
  void publish(const supervision::StateSnapshot& snapshot);

  // WIFI CONFIGURER on the serial link. Never blocks.
  void requestPortal() { portalRequested_ = true; }

 private:
  static void taskEntry(void* self);
  void run();
  void step(uint32_t nowMs);
  void applyPosted(uint32_t nowMs);
  void setPortal(bool open);
  void joinWifi(uint32_t nowMs);
  void reportWifi(bool connected);
  void publishPending();

  storage::SettingsStore& store_;
  supervision::WifiPlan plan_{5UL * 60UL * 1000UL, 15UL * 60UL * 1000UL};
  web::WebUi webUi_;
  mqtt::MqttLink mqtt_;
  DNSServer dns_;

  supervision::NetworkSettings settings_{};
  bool hasSettings_ = false;
  char deviceId_[supervision::kDeviceIdSize] = {};
  char apSsid_[supervision::kApSsidSize] = {};
  char apPassword_[supervision::kApPasswordLength + 1] = {};
  bool portalOpen_ = false;
  bool wifiWasConnected_ = false;
  uint32_t lastJoinMs_ = 0;

  portMUX_TYPE lock_ = portMUX_INITIALIZER_UNLOCKED;
  supervision::StateSnapshot snapshot_{};
  bool snapshotFresh_ = false;
  std::atomic<bool> portalRequested_{false};
};

}  // namespace terrarium::network
