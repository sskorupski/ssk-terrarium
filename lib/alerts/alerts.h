// Alerts (cadrage F9): out-of-range temperature or humidity, silent sensor,
// unexpected restart. Sent to ntfy over HTTPS; never delays regulation.
#pragma once

#include <cstdint>

namespace terrarium::alerts {

enum class Alert : uint8_t {
  kTemperatureOutOfRange,
  kHumidityOutOfRange,
  kSensorSilent,
  kHeatTooLong,
  kUnexpectedRestart,
};

class Notifier {
 public:
  void begin();
  // Queues an alert. Returns false if the queue is full; never blocks.
  bool raise(Alert alert);
  // Sends queued alerts when the network allows it.
  void loop();
};

}  // namespace terrarium::alerts
