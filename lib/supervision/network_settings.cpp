#include "network_settings.h"

#include <cstring>

namespace terrarium::supervision {

namespace {

// Length of `text` if it is at most `max` bytes, max + 1 otherwise; never
// reads past max + 1 bytes.
size_t boundedLength(const char* text, size_t max) {
  size_t length = 0;
  while (length <= max && text[length] != '\0') ++length;
  return length;
}

bool lengthWithin(const char* text, size_t min, size_t max) {
  if (text == nullptr) return false;
  const size_t length = boundedLength(text, max);
  return length >= min && length <= max;
}

bool isPrintable(const char* text) {
  for (; *text != '\0'; ++text) {
    if (*text < 0x20 || *text > 0x7e) return false;
  }
  return true;
}

bool isHostName(const char* text) {
  for (; *text != '\0'; ++text) {
    const char c = *text;
    const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                    c == '.' || c == '-';
    if (!ok) return false;
  }
  return true;
}

bool parsePort(const char* text, uint16_t& out) {
  if (!lengthWithin(text, 1, 5)) return false;
  uint32_t value = 0;
  for (; *text != '\0'; ++text) {
    if (*text < '0' || *text > '9') return false;
    value = value * 10 + static_cast<uint32_t>(*text - '0');
  }
  if (value < 1 || value > 65535) return false;
  out = static_cast<uint16_t>(value);
  return true;
}

bool wifiPasswordOk(const char* text) {
  if (text == nullptr) return false;
  return text[0] == '\0' || (lengthWithin(text, 8, 63) && isPrintable(text));
}

SettingsError check(const char* wifiSsid, const char* wifiPassword, const char* brokerHost) {
  if (!lengthWithin(wifiSsid, 1, 32)) return SettingsError::kWifiSsid;
  if (!wifiPasswordOk(wifiPassword)) return SettingsError::kWifiPassword;
  if (!lengthWithin(brokerHost, 1, 63) || !isHostName(brokerHost)) {
    return SettingsError::kBrokerHost;
  }
  return SettingsError::kNone;
}

SettingsError checkAccount(const char* mqttUser, const char* mqttPassword) {
  if (!lengthWithin(mqttUser, 1, 32) || !isPrintable(mqttUser)) return SettingsError::kMqttUser;
  if (!lengthWithin(mqttPassword, 1, 64) || !isPrintable(mqttPassword)) {
    return SettingsError::kMqttPassword;
  }
  return SettingsError::kNone;
}

}  // namespace

SettingsError fillFromForm(const char* wifiSsid, const char* wifiPassword, const char* brokerHost,
                           const char* brokerPort, const char* mqttUser, const char* mqttPassword,
                           NetworkSettings& out) {
  SettingsError error = check(wifiSsid, wifiPassword, brokerHost);
  if (error != SettingsError::kNone) return error;
  uint16_t port = 0;
  if (!parsePort(brokerPort, port)) return SettingsError::kBrokerPort;
  error = checkAccount(mqttUser, mqttPassword);
  if (error != SettingsError::kNone) return error;

  std::strcpy(out.wifiSsid, wifiSsid);
  std::strcpy(out.wifiPassword, wifiPassword);
  std::strcpy(out.brokerHost, brokerHost);
  out.brokerPort = port;
  std::strcpy(out.mqttUser, mqttUser);
  std::strcpy(out.mqttPassword, mqttPassword);
  return SettingsError::kNone;
}

SettingsError validate(const NetworkSettings& settings) {
  // Fields come from NVS: make sure each one is terminated before reading it.
  if (std::memchr(settings.wifiSsid, '\0', sizeof(settings.wifiSsid)) == nullptr) {
    return SettingsError::kWifiSsid;
  }
  if (std::memchr(settings.wifiPassword, '\0', sizeof(settings.wifiPassword)) == nullptr) {
    return SettingsError::kWifiPassword;
  }
  if (std::memchr(settings.brokerHost, '\0', sizeof(settings.brokerHost)) == nullptr) {
    return SettingsError::kBrokerHost;
  }
  if (std::memchr(settings.mqttUser, '\0', sizeof(settings.mqttUser)) == nullptr) {
    return SettingsError::kMqttUser;
  }
  if (std::memchr(settings.mqttPassword, '\0', sizeof(settings.mqttPassword)) == nullptr) {
    return SettingsError::kMqttPassword;
  }
  const SettingsError error = check(settings.wifiSsid, settings.wifiPassword, settings.brokerHost);
  if (error != SettingsError::kNone) return error;
  if (settings.brokerPort == 0) return SettingsError::kBrokerPort;
  return checkAccount(settings.mqttUser, settings.mqttPassword);
}

const char* describe(SettingsError error) {
  switch (error) {
    case SettingsError::kNone:
      return "Reglages enregistres";
    case SettingsError::kWifiSsid:
      return "Nom du Wi-Fi : 1 a 32 caracteres";
    case SettingsError::kWifiPassword:
      return "Mot de passe du Wi-Fi : vide, ou 8 a 63 caracteres";
    case SettingsError::kBrokerHost:
      return "Adresse du courtier : un nom d'hote seul, sans protocole ni chemin";
    case SettingsError::kBrokerPort:
      return "Port du courtier : un nombre de 1 a 65535";
    case SettingsError::kMqttUser:
      return "Identifiant MQTT : 1 a 32 caracteres";
    case SettingsError::kMqttPassword:
      return "Mot de passe MQTT : 1 a 64 caracteres";
  }
  return "Reglage invalide";
}

}  // namespace terrarium::supervision
