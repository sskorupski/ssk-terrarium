// MQTT supervision (cadrage section 6): publishes state and events, receives
// commands. Supervision only: regulation never waits for it.
#pragma once

#include <PubSubClient.h>
#include <WiFiClient.h>

namespace terrarium::mqtt {

class MqttLink {
 public:
  MqttLink() : client_(wifi_) {}

  void begin();
  // Non-blocking: reconnects with backoff and returns immediately if offline.
  void loop();
  bool publishState();

 private:
  WiFiClient wifi_;
  PubSubClient client_;
};

}  // namespace terrarium::mqtt
