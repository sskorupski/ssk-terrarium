// Commands typed on the USB serial link (RG-changement-de-mode-manuel). Pure
// logic: no hardware, no allocation.
#pragma once

#include "mode_machine.h"

namespace terrarium::control {

// Parses "MODE <wire name>", e.g. "MODE ACTIF", with an optional trailing CR.
// Returns false and leaves `out` untouched on anything else, including nullptr.
bool parseModeCommand(const char* line, Mode& out);

}  // namespace terrarium::control
