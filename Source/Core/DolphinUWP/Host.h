#pragma once

#include "Common/Flag.h"

namespace UWP {
static Common::Flag g_shutdown_requested{false};
static Common::Flag g_tried_graceful_shutdown{false};
}
