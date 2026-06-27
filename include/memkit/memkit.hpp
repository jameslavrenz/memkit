#pragma once

#include "config.hpp"
#include "containers/bitset.hpp"
#include "containers/bit_stream.hpp"
#include "containers/byte_ring.hpp"
#include "containers/btree.hpp"
#include "containers/deque.hpp"
#include "containers/dlist.hpp"
#include "containers/double_buffer.hpp"
#include "containers/enum_map.hpp"
#include "containers/fixed_iovec.hpp"
#include "containers/fixed_variant.hpp"
#include "containers/hashmap.hpp"
#include "containers/list.hpp"
#include "containers/lookup_table.hpp"
#include "containers/lrucache.hpp"
#include "containers/mpsc_queue.hpp"
#include "containers/objpool.hpp"
#include "containers/pqueue.hpp"
#include "containers/queue.hpp"
#include "containers/ring.hpp"
#include "containers/ring_log.hpp"
#include "containers/running_stats.hpp"
#include "containers/flat_map.hpp"
#include "containers/handle_pool.hpp"
#include "containers/intrusive_list.hpp"
#include "containers/small_string.hpp"
#include "containers/small_buffer.hpp"
#include "containers/spsc_queue.hpp"
#include "containers/sparse_set.hpp"
#include "containers/stack.hpp"
#include "containers/timer_wheel.hpp"
#include "containers/token_bucket.hpp"
#include "containers/vector.hpp"
#include "memory/memory.hpp"
#include "status.hpp"
#include "stl.hpp"

namespace memkit {

template<typename T, typename StorageBacking = memory::fixed_buffer>
using Arena = memory::arena<StorageBacking>;

} // namespace memkit
