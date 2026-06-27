#pragma once

#include "ring_buffer_core.hpp"

namespace memkit::detail {

using ring_storage_kind = ring_buffer_storage_kind;
using ring_policy       = ring_buffer_policy;

template<typename Policy>
using ring_core = ring_buffer_core<Policy>;

} // namespace memkit::detail
