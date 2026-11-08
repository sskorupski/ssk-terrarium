// What a Wi-Fi disconnection reason means for whoever reads the serial link.
// Pure logic: no hardware, no allocation.
#pragma once

#include <cstdint>

namespace terrarium::supervision {

// Text for a reason code of esp_wifi_types.h (wifi_err_reason_t), in French
// without accents. nullptr for the board's own disconnection, which is not a
// failure.
const char* describeWifiFailure(uint8_t reason);

}  // namespace terrarium::supervision
