#pragma once

#include <cstdint>

// One accepted connection; unlike a SOCKET value, never reused while the process runs.
using ConnectionId = std::uint64_t;
