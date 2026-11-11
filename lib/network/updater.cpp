#include "updater.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <esp_ota_ops.h>

#include <cstdio>
#include <cstring>

#include "root_certificates.h"
#include "supervisor.h"

// Arduino validates every new image as soon as it boots, unless the firmware
// says it will do so itself (RG-retour-arriere-automatique): Updater::watchBoot.
extern "C" bool verifyRollbackLater() { return true; }

namespace terrarium::network {

namespace {

using update::Failure;
using update::Refusal;

// Fixed at compile time: no command names another source (RG-image-signee).
constexpr char kSkaronHost[] = "api.skaron.ssk-it.fr";

// The TLS handshake takes most of the stack, as in the supervision task.
constexpr uint32_t kTaskStackBytes = 10 * 1024;
constexpr UBaseType_t kTaskPriority = 1;
constexpr BaseType_t kTaskCore = 0;  // Apart from the regulation loop, on core 1.
constexpr uint32_t kHandshakeTimeoutS = 15;
// Skaron may answer from a cold container.
constexpr uint32_t kConnectTimeoutMs = 20UL * 1000UL;
constexpr uint16_t kReadTimeoutMs = 20UL * 1000UL;
// The answer carries the release notes: a larger one is not Skaron's.
constexpr int kMaxAnswerBytes = 16 * 1024;
constexpr uint32_t kStallMs = 60UL * 1000UL;  // RG-regulation-pendant-la-mise-a-jour.
constexpr uint32_t kIdleWaitMs = 20;
// Before restarting, time for the supervision task to publish "redemarrage".
constexpr uint32_t kRestartDrainMs = 5UL * 1000UL;
constexpr uint32_t kRestartGraceMs = 500;
constexpr char kHttpsPrefix[] = "https://";

void printHeap(const char* what) {
  Serial.printf("MISE-A-JOUR %s : tas libre %u octets, minimum %u, plus grand bloc %u\r\n", what,
                ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getMaxAllocHeap());
}

void versionText(const update::SemVer& version, char (&out)[update::kVersionTextSize]) {
  if (!update::format(version, out)) std::strcpy(out, "?");
}

}  // namespace

void Updater::begin(const char* firmwareVersion, const char* publicKey) {
  licenceLock_ = xSemaphoreCreateMutex();
  versionKnown_ = update::fromDescribe(firmwareVersion, current_);
  keyValid_ = update::parsePublicKey(publicKey, key_);
  if (!keyValid_)
    Serial.println("ERREUR cle publique de mise a jour illisible : aucune image ne s'installera");
  if (store_.loadUpdateLicence(licence_) &&
      update::isValidLicence(licence_, std::strlen(licence_))) {
    Serial.println("MISE-A-JOUR licence enregistree");
  } else {
    licence_[0] = '\0';
    Serial.println("MISE-A-JOUR aucune licence enregistree : mise a jour a distance impossible");
  }

  esp_ota_img_states_t state = ESP_OTA_IMG_UNDEFINED;
  const esp_partition_t* running = esp_ota_get_running_partition();
  pendingVerify_ = running != nullptr && esp_ota_get_state_partition(running, &state) == ESP_OK &&
                   state == ESP_OTA_IMG_PENDING_VERIFY;
  char version[update::kVersionTextSize];
  versionText(current_, version);
  if (pendingVerify_) {
    Serial.printf(
        "MISE-A-JOUR firmware %s a verifier : valide une fois l'air mesure et le courtier "
        "rejoint, sinon retour a l'ancien dans 5 minutes\r\n",
        version);
    return;
  }
  char note[update::kVersionTextSize] = {};
  if (!store_.loadUpdateAttempt(note)) return;
  update::SemVer attempted{};
  if (update::readAttemptNote(note, versionKnown_, current_, attempted) ==
      update::AttemptNote::kStale) {
    store_.clearUpdateAttempt();
    return;
  }
  char tried[update::kVersionTextSize];
  versionText(attempted, tried);
  Serial.printf("MISE-A-JOUR %s refusee par le demarrage : retour au firmware %s\r\n", tried,
                version);
  char event[supervision::kEventSize];
  if (update::formatVersionEvent(update::VersionStep::kCancelled, attempted, event)) send(event);
  cancelledPending_ = true;
}

void Updater::watchBoot(uint32_t nowMs, bool airRetained, bool brokerConnected, bool eventsSent) {
  if (cancelledPending_ && brokerConnected && eventsSent) {
    cancelledPending_ = false;
    store_.clearUpdateAttempt();
  }
  if (!pendingVerify_) return;
  char version[update::kVersionTextSize];
  switch (validation_.update(nowMs, airRetained, brokerConnected)) {
    case update::Validation::kPending:
      return;
    case update::Validation::kValid: {
      pendingVerify_ = false;
      esp_ota_mark_app_valid_cancel_rollback();
      store_.clearUpdateAttempt();
      versionText(current_, version);
      Serial.printf("MISE-A-JOUR firmware %s valide : retour arriere annule\r\n", version);
      char event[supervision::kEventSize];
      if (update::formatVersionEvent(update::VersionStep::kValidated, current_, event)) send(event);
      return;
    }
    case update::Validation::kExpired:
      versionText(current_, version);
      Serial.printf("MISE-A-JOUR firmware %s non valide apres 5 minutes : retour a l'ancien\r\n",
                    version);
      Serial.flush();
      // Marks this image invalid and restarts: the bootloader boots the old one.
      esp_ota_mark_app_invalid_rollback_and_reboot();
      return;
  }
}

update::LicenceOutcome Updater::setLicence(const char* licence) {
  xSemaphoreTake(licenceLock_, portMAX_DELAY);
  std::strncpy(licence_, licence, sizeof(licence_) - 1);
  licence_[sizeof(licence_) - 1] = '\0';
  xSemaphoreGive(licenceLock_);
  const update::LicenceOutcome outcome = store_.saveUpdateLicence(licence)
                                             ? update::LicenceOutcome::kApplied
                                             : update::LicenceOutcome::kNotSaved;
  Serial.println(
      outcome == update::LicenceOutcome::kApplied
          ? "MISE-A-JOUR licence enregistree, conservee apres coupure"
          : "ERREUR licence appliquee mais non enregistree, perdue a la prochaine coupure");
  char event[supervision::kEventSize];
  if (update::formatLicenceEvent(outcome, event)) send(event);
  return outcome;
}

bool Updater::hasLicence() {
  xSemaphoreTake(licenceLock_, portMAX_DELAY);
  const bool has = licence_[0] != '\0';
  xSemaphoreGive(licenceLock_);
  return has;
}

void Updater::send(const char (&event)[supervision::kEventSize]) { supervisor_.sendEvent(event); }

void Updater::refuse(Refusal refusal) {
  Serial.printf("MISE-A-JOUR refusee : %s\r\n", update::describe(refusal));
  char event[supervision::kEventSize];
  if (update::formatRefusalEvent(refusal, event)) send(event);
}

void Updater::handle(const update::UpdateCommand& command) {
  // A firmware under verification keeps the old one as its way back: no
  // other image may overwrite it before it has validated itself.
  const update::Situation situation{busy_.load() || pendingVerify_, fault_.load(), hasLicence(),
                                    versionKnown_};
  Refusal refusal = Refusal::kBusy;
  const bool admitted = command.action == update::UpdateAction::kCheck
                            ? update::admitCheck(situation, refusal)
                            : update::admitInstall(situation, current_, command.version, refusal);
  if (!admitted) {
    refuse(refusal);
    return;
  }
  busy_ = true;
  job_.action = command.action;
  job_.version = command.version;
  if (xTaskCreatePinnedToCore(taskEntry, "mise-a-jour", kTaskStackBytes, this, kTaskPriority,
                              nullptr, kTaskCore) != pdPASS) {
    busy_ = false;
    printHeap("tache non creee");
    refuse(Refusal::kUnreachable);
  }
}

void Updater::taskEntry(void* self) {
  static_cast<Updater*>(self)->run();
  vTaskDelete(nullptr);
}

void Updater::run() {
  if (job_.action == update::UpdateAction::kCheck) {
    check();
  } else {
    install(job_.version);
  }
  busy_ = false;
}

void Updater::check() {
  Refusal refusal = Refusal::kUnreachable;
  char event[supervision::kEventSize];
  switch (ask(refusal)) {
    case update::Answer::kOffer:
      if (update::formatVersionEvent(update::VersionStep::kAvailable, offer_.version, event)) {
        send(event);
      }
      return;
    case update::Answer::kUpToDate:
      if (update::formatVersionEvent(update::VersionStep::kUpToDate, current_, event)) {
        send(event);
      }
      return;
    case update::Answer::kRefused:
      refuse(refusal);
      return;
  }
}

update::Answer Updater::ask(Refusal& refusal) {
  char path[160];
  char url[sizeof(kHttpsPrefix) + sizeof(kSkaronHost) + sizeof(path)];
  refusal = Refusal::kUnreachable;
  if (!update::formatSkaronPath(current_, path, sizeof(path))) return update::Answer::kRefused;
  std::snprintf(url, sizeof(url), "%s%s%s", kHttpsPrefix, kSkaronHost, path);

  WiFiClientSecure tls;
  tls.setCACert(mqtt::kBrokerRootCertificates);
  tls.setHandshakeTimeout(kHandshakeTimeoutS);
  HTTPClient http;
  http.setReuse(false);
  http.setConnectTimeout(kConnectTimeoutMs);
  http.setTimeout(kReadTimeoutMs);
  if (!http.begin(tls, url)) return update::Answer::kRefused;
  {
    static char bearer[sizeof("Bearer ") + update::kLicenceSize];
    xSemaphoreTake(licenceLock_, portMAX_DELAY);
    std::snprintf(bearer, sizeof(bearer), "Bearer %s", licence_);
    xSemaphoreGive(licenceLock_);
    http.addHeader("Authorization", bearer);
    std::memset(bearer, 0, sizeof(bearer));
  }
  const int status = http.GET();
  JsonDocument body;
  if (status == HTTP_CODE_OK) {
    const int size = http.getSize();
    if (size > kMaxAnswerBytes) {
      http.end();
      return update::Answer::kRefused;
    }
    JsonDocument filter;
    update::offerFilter(filter);
    const String text = http.getString();
    if (deserializeJson(body, text, DeserializationOption::Filter(filter)) !=
        DeserializationError::Ok) {
      http.end();
      return update::Answer::kRefused;
    }
  }
  http.end();
  if (status <= 0) {
    Serial.printf("MISE-A-JOUR %s injoignable : %s\r\n", kSkaronHost,
                  HTTPClient::errorToString(status).c_str());
  } else if (status != HTTP_CODE_OK && status != HTTP_CODE_NO_CONTENT) {
    Serial.printf("MISE-A-JOUR %s repond %d\r\n", kSkaronHost, status);
  }
  return update::readAnswer(status, body.as<JsonVariantConst>(), offer_, refusal);
}

void Updater::install(const update::SemVer& requested) {
  Refusal refusal = Refusal::kUnreachable;
  const update::Answer answer = ask(refusal);
  if (answer == update::Answer::kUpToDate ||
      (answer == update::Answer::kOffer && update::compare(offer_.version, requested) != 0)) {
    refusal = Refusal::kNotOffered;
  }
  if (answer != update::Answer::kOffer || refusal == Refusal::kNotOffered) {
    refuse(refusal);
    return;
  }
  char version[update::kVersionTextSize];
  versionText(requested, version);
  Serial.printf("MISE-A-JOUR %s acceptee : telechargement dans la partition inactive\r\n", version);
  char event[supervision::kEventSize];
  if (update::formatVersionEvent(update::VersionStep::kAccepted, requested, event)) send(event);
  printHeap("avant telechargement");

  Failure failure = Failure::kStalled;
  if (download(requested, failure)) {
    restart(requested);
    return;
  }
  Serial.printf("MISE-A-JOUR %s abandonnee : %s, le firmware en place continue\r\n", version,
                update::describe(failure));
  if (update::formatFailureEvent(requested, failure, event)) send(event);
}

bool Updater::download(const update::SemVer& requested, Failure& failure) {
  // Before any byte: the signature is the owner's, and names this release.
  update::Signature signature{};
  if (!keyValid_ ||
      update::parseSignature(offer_.signature, signature) != update::SignatureError::kNone ||
      update::checkTrustedComment(signature, key_) != update::SignatureError::kNone) {
    failure = Failure::kBadSignature;
    return false;
  }
  switch (update::checkRelease(signature.trustedComment, current_, requested)) {
    case update::ReleaseCheck::kOk:
      break;
    case update::ReleaseCheck::kOtherPhase:
      failure = Failure::kOtherPhase;
      return false;
    case update::ReleaseCheck::kNotNewer:
      failure = Failure::kNotNewer;
      return false;
    case update::ReleaseCheck::kNotRequested:
      failure = Failure::kBadSignature;
      return false;
  }
  const esp_partition_t* partition = esp_ota_get_next_update_partition(nullptr);
  if (partition == nullptr) {
    failure = Failure::kBadImage;
    return false;
  }

  WiFiClientSecure tls;
  tls.setCACert(mqtt::kBrokerRootCertificates);
  tls.setHandshakeTimeout(kHandshakeTimeoutS);
  HTTPClient http;
  http.setReuse(false);
  http.setConnectTimeout(kConnectTimeoutMs);
  http.setTimeout(kReadTimeoutMs);
  failure = Failure::kStalled;
  if (!http.begin(tls, offer_.url)) return false;
  const int status = http.GET();
  if (status != HTTP_CODE_OK) {
    Serial.printf("MISE-A-JOUR image : reponse %d\r\n", status);
    http.end();
    return false;
  }
  update::DownloadProgress progress(kStallMs);
  if (!progress.start(http.getSize(), partition->size, millis(), failure)) {
    http.end();
    return false;
  }
  esp_ota_handle_t handle = 0;
  // Sequential writes erase the partition as it fills, not all at once.
  if (esp_ota_begin(partition, OTA_WITH_SEQUENTIAL_WRITES, &handle) != ESP_OK) {
    failure = Failure::kBadImage;
    http.end();
    return false;
  }
  printHeap("deux connexions TLS ouvertes");
  update::ImageHash hash;
  hash.begin();
  WiFiClient* stream = http.getStreamPtr();
  bool written = true;
  while (!progress.complete()) {
    const uint32_t nowMs = millis();
    if (fault_) {
      failure = Failure::kFault;
      written = false;
      break;
    }
    if (progress.stalled(nowMs)) {
      failure = Failure::kStalled;
      written = false;
      break;
    }
    const int available = stream->available();
    if (available <= 0) {
      if (!stream->connected()) {
        failure = Failure::kStalled;
        written = false;
        break;
      }
      vTaskDelay(pdMS_TO_TICKS(kIdleWaitMs));
      continue;
    }
    size_t wanted = static_cast<size_t>(available);
    if (wanted > sizeof(buffer_)) wanted = sizeof(buffer_);
    if (wanted > progress.remaining()) wanted = progress.remaining();
    const int got = stream->read(buffer_, wanted);
    if (got <= 0) {
      vTaskDelay(pdMS_TO_TICKS(kIdleWaitMs));
      continue;
    }
    hash.update(buffer_, static_cast<size_t>(got));
    if (esp_ota_write(handle, buffer_, static_cast<size_t>(got)) != ESP_OK) {
      failure = Failure::kBadImage;
      written = false;
      break;
    }
    if (const uint8_t step = progress.advance(static_cast<size_t>(got), nowMs)) {
      char event[supervision::kEventSize];
      if (update::formatProgressEvent(requested, step, event)) send(event);
      char what[24];
      std::snprintf(what, sizeof(what), "telechargement %u %%", step);
      printHeap(what);
    }
    vTaskDelay(1);  // Lets core 0's idle task run: the task watchdog checks it.
  }
  http.end();
  if (!written) {
    esp_ota_abort(handle);
    return false;
  }
  if (hash.verify(signature, key_) != update::SignatureError::kNone) {
    esp_ota_abort(handle);
    failure = Failure::kBadSignature;
    return false;
  }
  // Checks the ESP-IDF image format and its own SHA-256.
  if (esp_ota_end(handle) != ESP_OK) {
    failure = Failure::kBadImage;
    return false;
  }
  if (fault_) {
    failure = Failure::kFault;
    return false;
  }
  char version[update::kVersionTextSize];
  versionText(requested, version);
  if (!store_.saveUpdateAttempt(version)) {
    Serial.println("ERREUR version tentee non notee : un retour arriere ne publiera pas annulee");
  }
  if (esp_ota_set_boot_partition(partition) != ESP_OK) {
    store_.clearUpdateAttempt();
    failure = Failure::kBadImage;
    return false;
  }
  return true;
}

void Updater::restart(const update::SemVer& version) {
  char text[update::kVersionTextSize];
  versionText(version, text);
  Serial.printf("MISE-A-JOUR %s verifiee : redemarrage, sorties coupees le temps du demarrage\r\n",
                text);
  char event[supervision::kEventSize];
  if (update::formatVersionEvent(update::VersionStep::kRestart, version, event)) send(event);
  const uint32_t startMs = millis();
  while (supervisor_.eventsPending() > 0 && millis() - startMs < kRestartDrainMs) {
    vTaskDelay(pdMS_TO_TICKS(kIdleWaitMs));
  }
  vTaskDelay(pdMS_TO_TICKS(kRestartGraceMs));
  Serial.flush();
  esp_restart();
}

}  // namespace terrarium::network
