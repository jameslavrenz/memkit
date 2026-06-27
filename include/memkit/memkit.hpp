#pragma once

#include "config.hpp"
#include "containers/bitset.hpp"
#include "containers/byte_ring.hpp"
#include "containers/btree.hpp"
#include "containers/deque.hpp"
#include "containers/dlist.hpp"
#include "containers/hashmap.hpp"
#include "containers/list.hpp"
#include "containers/lrucache.hpp"
#include "containers/objpool.hpp"
#include "containers/pqueue.hpp"
#include "containers/queue.hpp"
#include "containers/ring.hpp"
#include "containers/handle_pool.hpp"
#include "containers/small_string.hpp"
#include "containers/stack.hpp"
#include "containers/vector.hpp"
#include "concepts.hpp"
#include "memory/memory.hpp"
#include "status.hpp"
#include "stl.hpp"

namespace memkit {

template<typename T, typename StorageBacking = memory::fixed_buffer>
using Arena = memory::arena<StorageBacking>;

template<typename StorageBacking = memory::fixed_buffer>
using FixedPool = memory::fixed_pool<StorageBacking>;

} // namespace memkit
