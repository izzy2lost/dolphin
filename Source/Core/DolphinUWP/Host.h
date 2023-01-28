#pragma once

#include "Common/Flag.h"

namespace UWP {
extern Common::Flag g_shutdown_requested;
extern Common::Flag g_tried_graceful_shutdown;
}
