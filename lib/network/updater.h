// Remote firmware update (ADR-mise-a-jour-distribuee-par-skaron): asks Skaron
// for the version offered to this board, downloads the image into the
// inactive partition in a task of its own, checks the owner's minisign
// signature, and only then makes the partition bootable. After the restart,
// the new firmware validates itself or lets the bootloader return to the old
// one (RG-retour-arriere-automatique). The decisions live in lib/update; this
// is the network and flash side.
#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <atomic>
#include <cstdint>

#include "firmware_version.h"
#include "install_flow.h"
#include "minisign.h"
#include "settings_store.h"
#include "update_command.h"

namespace terrarium::network {

class Supervisor;

class Updater {
 public:
  Updater(storage::SettingsStore& store, Supervisor& supervisor)
      : store_(store), supervisor_(supervisor) {}

  // From Supervisor::begin(), events queue ready: reads the licence, the
  // version and key compiled in, and whether this firmware is under
  // verification or was just restored.
  void begin(const char* firmwareVersion, const char* publicKey);

  // A licence from cmd/mise-a-jour or the serial link: kept, saved, and
  // answered on event. Any task.
  update::LicenceOutcome setLicence(const char* licence);

  // verifier or installer, from the supervision task: refused at once, or
  // run in the update task. Never blocks.
  void handle(const update::UpdateCommand& command);

  // From the regulation loop, every cycle: the faults under way. They never
  // stop an update; they are named (RG-mise-a-jour-pendant-un-defaut).
  void setFaults(update::UpdateFaults faults) { faults_ = faults; }

  // From the regulation loop: validates a new firmware once it has retained
  // an air measure and joined the broker, or restarts into the old one after
  // 5 minutes; clears the note of a restored firmware once "annulee" is out.
  void watchBoot(uint32_t nowMs, bool airRetained, bool brokerConnected, bool eventsSent);

 private:
  static void taskEntry(void* self);
  void run();
  void check();
  void install(const update::SemVer& requested);
  update::Answer ask(update::Refusal& refusal);
  bool download(const update::SemVer& requested, update::Failure& failure);
  void restart(const update::SemVer& version);
  bool hasLicence();
  void send(const char (&event)[supervision::kEventSize]);
  void refuse(update::Refusal refusal);

  storage::SettingsStore& store_;
  Supervisor& supervisor_;

  update::SemVer current_{};
  bool versionKnown_ = false;
  update::PublicKey key_{};
  bool keyValid_ = false;

  SemaphoreHandle_t licenceLock_ = nullptr;
  char licence_[update::kLicenceSize] = {};

  std::atomic<update::UpdateFaults> faults_{0};
  std::atomic<bool> busy_{false};
  update::UpdateCommand job_{};

  // Boot of a new firmware (ESP_OTA_IMG_PENDING_VERIFY), or of one restored.
  std::atomic<bool> pendingVerify_{false};
  update::ValidationWatch validation_{5UL * 60UL * 1000UL};
  bool cancelledPending_ = false;

  // The update task's working memory: one offer, one read buffer.
  update::Offer offer_{};
  uint8_t buffer_[1024] = {};
};

}  // namespace terrarium::network
