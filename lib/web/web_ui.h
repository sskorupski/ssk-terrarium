// Configuration portal (RG-configuration-wifi-sur-place): a page served on the
// board's own access point only, where the Wi-Fi and broker settings are
// entered. It commands no output and changes neither mode nor settings. The
// local dashboard of cadrage F7 will join it on the same server.
#pragma once

#include <ESPAsyncWebServer.h>
#include <freertos/FreeRTOS.h>

#include <cstdint>

#include "network_settings.h"

namespace terrarium::web {

class WebUi {
 public:
  explicit WebUi(uint16_t port = 80) : server_(port) {}

  void begin();

  // Settings shown in the form (passwords excepted). Thread-safe.
  void showCurrent(const supervision::NetworkSettings& settings);

  // Copies the settings posted since the last call into `out`. Returns true
  // once per accepted post. Thread-safe.
  bool takePosted(supervision::NetworkSettings& out);

 private:
  void handleForm(AsyncWebServerRequest* request);
  void handleSave(AsyncWebServerRequest* request);
  void sendForm(AsyncWebServerRequest* request, int status, const char* message, bool isError);

  AsyncWebServer server_;
  portMUX_TYPE lock_ = portMUX_INITIALIZER_UNLOCKED;
  supervision::NetworkSettings current_{};
  supervision::NetworkSettings posted_{};
  bool hasPosted_ = false;
};

}  // namespace terrarium::web
