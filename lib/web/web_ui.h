// Local web page (cadrage F7): dashboard, settings form, event log, OTA.
#pragma once

#include <ESPAsyncWebServer.h>

#include <cstdint>

namespace terrarium::web {

class WebUi {
 public:
  explicit WebUi(uint16_t port = 80) : server_(port) {}

  void begin();

 private:
  AsyncWebServer server_;
};

}  // namespace terrarium::web
