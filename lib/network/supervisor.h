// Supervision task: Wi-Fi, configuration portal and MQTT, on core 0, apart
// from the regulation loop (INV-3, RG-regulation-sans-reseau). The loop only
// hands it a snapshot and requests; it never waits for it.
#pragma once

#include <DNSServer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <atomic>
#include <cstdint>

#include "commands.h"
#include "heat_settings.h"
#include "identity.h"
#include "mqtt_link.h"
#include "network_settings.h"
#include "settings_store.h"
#include "state_message.h"
#include "update_command.h"
#include "updater.h"
#include "web_ui.h"
#include "wifi_plan.h"

namespace terrarium::network {

// A command that passed parsing, for the regulation loop to apply or refuse.
struct RemoteCommand {
  enum class Kind : uint8_t { kMode, kConfig };
  Kind kind;
  control::Mode mode;                 // kMode.
  supervision::ConfigCommand config;  // kConfig: heat merged, not yet checked against bounds.
};

class Supervisor {
 public:
  explicit Supervisor(storage::SettingsStore& store) : store_(store), updater_(store, *this) {}

  // From setup(): reads the saved settings, prints the access point name and
  // password and the device id on the serial link, then starts the task.
  // `firmwareVersion` (git describe) and `updateKey` (the owner's minisign
  // public key) are compiled into the firmware.
  void begin(const char* firmwareVersion, const char* updateKey);

  // From the regulation loop, once per measurement cycle. Never blocks.
  void publish(const supervision::StateSnapshot& snapshot);

  // WIFI CONFIGURER on the serial link. Never blocks.
  void requestPortal() { portalRequested_ = true; }

  // From the regulation loop: the next command received, if any. Never blocks.
  bool takeCommand(RemoteCommand& out);

  // Number of NTP synchronisations since boot (ADR-heure-par-ntp): the first
  // makes the time known. Never blocks.
  uint32_t timeSyncCount() const;

  // From the regulation loop: an event to publish once the broker is
  // reachable. Dropped if the queue is full. Never blocks.
  void sendEvent(const char (&event)[supervision::kEventSize]);

  // Events waiting for the broker. Never blocks.
  size_t eventsPending() const;

  // Remote update (RG-mise-a-jour-a-distance), from the regulation loop:
  // the faults under way, named in its warnings, every cycle; the licence
  // typed on the serial link; and the validation of a new firmware, every
  // iteration.
  void setUpdateFaults(update::UpdateFaults faults) { updater_.setFaults(faults); }
  update::LicenceOutcome setUpdateLicence(const char* licence) {
    return updater_.setLicence(licence);
  }
  void watchUpdate(uint32_t nowMs, bool airRetained) {
    updater_.watchBoot(nowMs, airRetained, brokerConnected_.load(), eventsPending() == 0);
  }

 private:
  static void taskEntry(void* self);
  void run();
  void step(uint32_t nowMs);
  void applyPosted(uint32_t nowMs);
  void setPortal(bool open);
  void joinWifi(uint32_t nowMs);
  void reportWifi(bool connected);
  void reportWifiFailure();
  void startNtp();
  void publishPending();
  void handleCommand(mqtt::CommandTopic topic, const uint8_t* payload, size_t length);
  void handleUpdateCommand(const uint8_t* payload, size_t length);
  void rejectCommand(const char* subtopic, const char* reason);

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
  // Last disconnection reason from the Wi-Fi event task, and the last one
  // printed: the driver retries without end, so each cause prints once.
  std::atomic<uint8_t> disconnectReason_{0};
  uint8_t reportedReason_ = 0;
  bool ntpStarted_ = false;
  uint32_t lastJoinMs_ = 0;

  portMUX_TYPE lock_ = portMUX_INITIALIZER_UNLOCKED;
  supervision::StateSnapshot snapshot_{};
  bool snapshotFresh_ = false;
  std::atomic<bool> portalRequested_{false};
  QueueHandle_t commands_ = nullptr;
  QueueHandle_t events_ = nullptr;
  std::atomic<bool> brokerConnected_{false};
  Updater updater_;
};

}  // namespace terrarium::network
