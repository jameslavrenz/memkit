#pragma once

#include "ring_buffer_core.hpp"

namespace memkit::detail {

template<typename Policy>
using queue_core = ring_buffer_core<Policy>;

} // namespace memkit::detail
