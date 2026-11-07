#include "alerts.h"

namespace terrarium::alerts {

// TODO(phase 3): ntfy topic and credentials from settings.
void Notifier::begin() {}

// TODO(phase 3): fixed-size ring buffer, deduplication per alert kind.
bool Notifier::raise(Alert /*alert*/) { return false; }

// TODO(phase 3): send outside the regulation path, with a short timeout.
void Notifier::loop() {}

}  // namespace terrarium::alerts
