#include "mqtt_link.h"

#include <ArduinoJson.h>

namespace terrarium::mqtt {

// TODO(phase 3): broker settings, prefix ssk-terrarium/<device-id>/, subscriptions to cmd/#.
void MqttLink::begin() {}

// TODO(phase 3): non-blocking reconnect with exponential backoff.
void MqttLink::loop() {}

// TODO(phase 3): serialise state into a StaticJsonDocument-sized buffer, retained.
bool MqttLink::publishState() { return false; }

}  // namespace terrarium::mqtt
