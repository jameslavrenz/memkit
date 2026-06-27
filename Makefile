CXX ?= c++
CC ?= cc
CXXFLAGS ?= -std=c++26 -Wall -Wextra -Wpedantic -Iinclude -DMEMKIT_MCU=1
CFLAGS ?= -std=c23 -Wall -Wextra -Wpedantic -Iinclude -DMEMKIT_MCU=1
LDFLAGS ?=

LIB_OBJS = $(BUILD)/arena.o $(BUILD)/mmap_backing.o $(BUILD)/c_api/bindings.o
BUILD = build

CPP_TESTS = test_ring_cpp test_vector_cpp test_hashmap_cpp test_list_cpp \
              test_dlist_cpp test_btree_cpp test_stack_queue_cpp test_pqueue_cpp \
              test_objpool_cpp test_bitset_cpp test_deque_cpp test_lrucache_cpp \
              test_small_string_cpp test_byte_ring_cpp test_handle_pool_cpp \
              test_intrusive_list_cpp test_spsc_queue_cpp test_flat_map_cpp test_timer_wheel_cpp \
              test_double_buffer_cpp test_mpsc_queue_cpp test_enum_map_cpp test_ring_log_cpp \
              test_sparse_set_cpp test_small_buffer_cpp test_fixed_variant_cpp test_token_bucket_cpp \
              test_fixed_iovec_cpp test_lookup_table_cpp test_bit_stream_cpp test_running_stats_cpp

MCU_EXAMPLES = example_mcu example_embedded_patterns

.PHONY: all test_cpp test_c_api_smoke test_c_api_extended mcu mpu clean lib

all: lib test_cpp mcu

lib: $(BUILD)/c_api $(LIB_OBJS)

$(BUILD)/%.o: src/%.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILD)/c_api/%.o: src/c_api/%.cpp | $(BUILD)/c_api
	$(CXX) $(CXXFLAGS) -c -o $@ $<

test_cpp: $(addprefix $(BUILD)/,$(CPP_TESTS))
	@for t in $(CPP_TESTS); do echo "==> $$t"; ./$(BUILD)/$$t || exit 1; done

test_c_api_smoke: $(BUILD)/test_c_api_smoke
	./$(BUILD)/test_c_api_smoke

test_c_api_extended: $(BUILD)/test_c_api_extended
	./$(BUILD)/test_c_api_extended

define CPP_TEST_RULE
$(BUILD)/$(1): tests/$(1).cpp $(LIB_OBJS) | $(BUILD)
	$(CXX) $(CXXFLAGS) tests/$(1).cpp $(LIB_OBJS) -o $$@ $(LDFLAGS)
endef

$(foreach t,$(CPP_TESTS),$(eval $(call CPP_TEST_RULE,$(t))))

$(BUILD)/test_c_api_smoke.o: tests/test_c_api_smoke.c | $(BUILD)
	$(CXX) $(CFLAGS) -x c -c -o $@ $<

$(BUILD)/test_c_api_smoke: $(BUILD)/test_c_api_smoke.o $(LIB_OBJS) | $(BUILD)
	$(CXX) -o $@ $(BUILD)/test_c_api_smoke.o $(LIB_OBJS) $(LDFLAGS)

$(BUILD)/test_c_api_extended.o: tests/test_c_api_extended.c | $(BUILD)/mpu
	$(CC) $(MPU_CFLAGS) -c -o $@ $<

$(BUILD)/test_c_api_extended: $(BUILD)/test_c_api_extended.o $(MPU_OBJS) | $(BUILD)/mpu/c_api
	$(CXX) $(MPU_CXXFLAGS) -o $@ $(BUILD)/test_c_api_extended.o $(MPU_OBJS) $(LDFLAGS)

mcu: $(addprefix $(BUILD)/,$(MCU_EXAMPLES))
	@for e in $(MCU_EXAMPLES); do echo "==> $$e"; ./$(BUILD)/$$e || exit 1; done

mpu: $(BUILD)/mpu/c_api $(BUILD)/example_mpu $(BUILD)/example_mpu_c test_c_api_extended
	./$(BUILD)/example_mpu
	./$(BUILD)/example_mpu_c

$(BUILD)/example_mcu: examples/example_mcu.cpp $(LIB_OBJS) | $(BUILD)
	$(CXX) $(CXXFLAGS) -o $@ examples/example_mcu.cpp $(LIB_OBJS) $(LDFLAGS)

$(BUILD)/example_embedded_patterns: examples/example_embedded_patterns.cpp $(LIB_OBJS) | $(BUILD)
	$(CXX) $(CXXFLAGS) -o $@ examples/example_embedded_patterns.cpp $(LIB_OBJS) $(LDFLAGS)

MPU_CXXFLAGS = -std=c++26 -Wall -Wextra -Wpedantic -Iinclude -DMEMKIT_MPU=1 -DEMBEDDED_LINUX=1 \
               -DMEMKIT_ALLOW_HEAP=1 -DMEMKIT_ALLOW_MMAP=1
MPU_CFLAGS = -std=c23 -Wall -Wextra -Wpedantic -Iinclude -DMEMKIT_MPU=1 -DEMBEDDED_LINUX=1 \
             -DMEMKIT_ALLOW_HEAP=1 -DMEMKIT_ALLOW_MMAP=1

$(BUILD)/mpu/%.o: src/%.cpp | $(BUILD)/mpu
	$(CXX) $(MPU_CXXFLAGS) -c -o $@ $<

$(BUILD)/mpu/c_api/%.o: src/c_api/%.cpp | $(BUILD)/mpu/c_api
	$(CXX) $(MPU_CXXFLAGS) -c -o $@ $<

MPU_OBJS = $(BUILD)/mpu/arena.o $(BUILD)/mpu/mmap_backing.o $(BUILD)/mpu/c_api/bindings.o

$(BUILD)/example_mpu: examples/example_mpu.cpp $(MPU_OBJS) | $(BUILD)/mpu/c_api
	$(CXX) $(MPU_CXXFLAGS) -o $@ examples/example_mpu.cpp $(MPU_OBJS) $(LDFLAGS)

$(BUILD)/example_mpu_c.o: examples/example_mpu.c | $(BUILD)/mpu
	$(CC) $(MPU_CFLAGS) -c -o $@ $<

$(BUILD)/example_mpu_c: $(BUILD)/example_mpu_c.o $(MPU_OBJS) | $(BUILD)/mpu/c_api
	$(CXX) $(MPU_CXXFLAGS) -o $@ $(BUILD)/example_mpu_c.o $(MPU_OBJS) $(LDFLAGS)

$(BUILD)/mpu/c_api:
	mkdir -p $(BUILD)/mpu/c_api

$(BUILD)/c_api:
	mkdir -p $(BUILD)/c_api

$(BUILD)/mpu:
	mkdir -p $(BUILD)/mpu

$(BUILD):
	mkdir -p $(BUILD)

clean:
	rm -rf $(BUILD)
