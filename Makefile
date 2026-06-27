CXX ?= c++
CC ?= cc
CXXFLAGS ?= -std=c++26 -Wall -Wextra -Wpedantic -Iinclude -DMEMKIT_MCU=1
CFLAGS ?= -std=c23 -Wall -Wextra -Wpedantic -Iinclude -DMEMKIT_MCU=1
LDFLAGS ?=

C_API_SRCS = $(wildcard src/c_api/*.cpp)
LIB_SRCS = src/arena.cpp src/mmap_backing.cpp $(C_API_SRCS)
LIB_OBJS = $(BUILD)/arena.o $(BUILD)/mmap_backing.o \
           $(patsubst src/c_api/%.cpp,$(BUILD)/c_api/%.o,$(C_API_SRCS))
BUILD = build

CPP_TESTS = test_ring_cpp test_vector_cpp test_hashmap_cpp test_list_cpp \
              test_dlist_cpp test_btree_cpp test_stack_queue_cpp test_pqueue_cpp \
              test_objpool_cpp test_bitset_cpp test_deque_cpp test_lrucache_cpp \
              test_small_string_cpp test_byte_ring_cpp test_handle_pool_cpp

LEGACY_C_TESTS = test_bitset test_btree test_deque test_dlist test_hashmap test_list \
                   test_lrucache test_objpool test_pqueue test_ring test_stack_queue test_vector

.PHONY: all test_cpp test_c_api_smoke test_c_api_extended test_c_api_legacy mcu mpu clean lib

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

test_c_api_legacy: $(addprefix $(BUILD)/legacy/,$(LEGACY_C_TESTS))
	@for t in $(LEGACY_C_TESTS); do echo "==> legacy $$t"; ./$(BUILD)/legacy/$$t || exit 1; done

$(BUILD)/legacy/%: tests/legacy/%.c $(MPU_OBJS) | $(BUILD)/legacy
	$(CC) $(MPU_CFLAGS) -c -o $(BUILD)/legacy/$*_o.o $<
	$(CXX) $(MPU_CXXFLAGS) -o $@ $(BUILD)/legacy/$*_o.o $(MPU_OBJS) $(LDFLAGS)

$(BUILD)/test_c_api_extended.o: tests/test_c_api_extended.c | $(BUILD)/mpu
	$(CC) $(MPU_CFLAGS) -c -o $@ $<

$(BUILD)/test_c_api_extended: $(BUILD)/test_c_api_extended.o $(MPU_OBJS) | $(BUILD)/mpu/c_api
	$(CXX) $(MPU_CXXFLAGS) -o $@ $(BUILD)/test_c_api_extended.o $(MPU_OBJS) $(LDFLAGS)

define CPP_TEST_RULE
$(BUILD)/$(1): tests/$(1).cpp $(LIB_OBJS) | $(BUILD)
	$(CXX) $(CXXFLAGS) tests/$(1).cpp $(LIB_OBJS) -o $$@ $(LDFLAGS)
endef

$(foreach t,$(CPP_TESTS),$(eval $(call CPP_TEST_RULE,$(t))))

$(BUILD)/test_c_api_smoke.o: tests/test_c_api_smoke.c | $(BUILD)
	$(CXX) $(CFLAGS) -x c -c -o $@ $<

$(BUILD)/test_c_api_smoke: $(BUILD)/test_c_api_smoke.o $(LIB_OBJS) | $(BUILD)
	$(CXX) -o $@ $(BUILD)/test_c_api_smoke.o $(LIB_OBJS) $(LDFLAGS)

mcu: $(BUILD)/example_mcu
	./$(BUILD)/example_mcu

mpu: $(BUILD)/mpu/c_api $(BUILD)/example_mpu $(BUILD)/example_mpu_c test_c_api_extended test_c_api_legacy
	./$(BUILD)/example_mpu
	./$(BUILD)/example_mpu_c

$(BUILD)/example_mcu: examples/example_mcu.cpp $(LIB_OBJS) | $(BUILD)
	$(CXX) $(CXXFLAGS) -o $@ examples/example_mcu.cpp $(LIB_OBJS) $(LDFLAGS)

MPU_CXXFLAGS = -std=c++26 -Wall -Wextra -Wpedantic -Iinclude -DMEMKIT_MPU=1 -DEMBEDDED_LINUX=1 \
               -DMEMKIT_ALLOW_HEAP=1 -DMEMKIT_ALLOW_MMAP=1
MPU_CFLAGS = -std=c23 -Wall -Wextra -Wpedantic -Iinclude -DMEMKIT_MPU=1 -DEMBEDDED_LINUX=1 \
             -DMEMKIT_ALLOW_HEAP=1 -DMEMKIT_ALLOW_MMAP=1

$(BUILD)/mpu/%.o: src/%.cpp | $(BUILD)/mpu
	$(CXX) $(MPU_CXXFLAGS) -c -o $@ $<

$(BUILD)/mpu/c_api/%.o: src/c_api/%.cpp | $(BUILD)/mpu/c_api
	$(CXX) $(MPU_CXXFLAGS) -c -o $@ $<

MPU_OBJS = $(BUILD)/mpu/arena.o $(BUILD)/mpu/mmap_backing.o \
           $(patsubst src/c_api/%.cpp,$(BUILD)/mpu/c_api/%.o,$(C_API_SRCS))

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

$(BUILD)/legacy:
	mkdir -p $(BUILD)/legacy

$(BUILD):
	mkdir -p $(BUILD)

clean:
	rm -rf $(BUILD)
