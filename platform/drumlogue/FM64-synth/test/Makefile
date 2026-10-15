# FM64 host tests. Builds the unit for this machine (not the drumlogue) with a
# portable arm.h, so it can run under AddressSanitizer / UBSan and be timed.
#
#   make test        smoke + params + fuzz under ASan+UBSan
#   make fuzz SEEDS="1 2 3" BLOCKS=200000
#   make bench       optimized build, render time per block
#   make lint        cppcheck + extra compiler warnings on the device source

SRC_DIR  := ..
ROOT     := ../..
BUILD    := build
CXX      ?= c++
CC       ?= cc

INC  := -include host/attributes.h -Ihost -I$(SRC_DIR) -I$(ROOT)/inc -I$(ROOT)/common -I$(ROOT)/fastapprox/fastapprox/src
DEFS := -DPARAM_COUNT=24 -DOP6 -DOPSIX -DSY77 -DEGLUT12
WARN := -Wno-unknown-attributes -Wno-ignored-attributes -Wno-deprecated-register \
        -Wno-unused-parameter -Wno-unused-variable -Wno-unused-function
SAN  := -fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer
STD  := -std=gnu++17 -ffast-math -fsigned-char

SEEDS  ?= 1 2 3 4 5
BLOCKS ?= 100000

# Host copy of unit.cc. Each rewrite keeps behaviour and only swaps a 32-bit
# ARM / GCC-only idiom for one Clang on 64-bit accepts:
#  - table pointer cast to int32_t (truncates on 64-bit) -> intptr_t
#  - vector & scalar broadcast -> vdup_n_s32
#  - address of a vector lane (&v[2] of a 4-lane vector) -> float offset 6
#  - ++ on a uint32x2_t lvalue -> two scalar increments
$(BUILD)/unit_host.cc: $(SRC_DIR)/unit.cc | $(BUILD)
	sed -e 's/ldr_lsl((int32_t)eg_lut/ldr_lsl((intptr_t)eg_lut/' \
	    -e 's/vcvt_s32_f32(x \* w->size) & w->size_mask/vcvt_s32_f32(x * w->size) \& vdup_n_s32(w->size_mask)/' \
	    -e 's/((float\*)&w0.val\[1\]\[2\])/((float*)\&w0 + 6)/' \
	    -e 's/(\*(uint32x2_t\*)&s_sample_cnt\[synvoice_idx\])++;/s_sample_cnt[synvoice_idx]++; s_sample_cnt[synvoice_idx + 1]++;/' \
	    $< > $@
	@n=$$(grep -c 'intptr_t)eg_lut\|vdup_n_s32(w->size_mask)\|&w0 + 6\|s_sample_cnt\[synvoice_idx + 1\]++' $@); \
	  test $$n -ge 5 || { echo "host rewrites matched $$n of 5 lines; unit.cc changed"; rm -f $@; exit 1; }

$(BUILD):
	mkdir -p $@

$(BUILD)/fm64_san: $(BUILD)/unit_host.cc host/harness.cc host/arm.h $(SRC_DIR)/header.c $(SRC_DIR)/fm64.h
	$(CC) -c $(INC) $(DEFS) -g -O1 $(SAN) $(SRC_DIR)/header.c -o $(BUILD)/header_san.o
	$(CXX) $(STD) $(INC) $(DEFS) $(WARN) -g -O1 $(SAN) -fno-finite-math-only host/harness.cc $(BUILD)/header_san.o -o $@

$(BUILD)/fm64_fast: $(BUILD)/unit_host.cc host/harness.cc host/arm.h $(SRC_DIR)/header.c $(SRC_DIR)/fm64.h
	$(CC) -c $(INC) $(DEFS) -O2 $(SRC_DIR)/header.c -o $(BUILD)/header_fast.o
	$(CXX) $(STD) $(INC) $(DEFS) $(WARN) -O2 -fno-finite-math-only host/harness.cc $(BUILD)/header_fast.o -o $@

$(BUILD)/levels: host/levels.cc $(SRC_DIR)/fm64.h host/attributes.h | $(BUILD)
	$(CXX) -std=gnu++17 $(INC) $(WARN) -g -O1 $(SAN) $< -o $@

regression: $(BUILD)/levels
	$(BUILD)/levels

test: regression $(BUILD)/fm64_san
	FM64_FIXTURES=fixtures $(BUILD)/fm64_san all

fuzz: $(BUILD)/fm64_san
	@fail=0; for s in $(SEEDS); do FM64_FIXTURES=fixtures $(BUILD)/fm64_san fuzz $$s $(BLOCKS) || fail=1; done; exit $$fail

bench: $(BUILD)/fm64_fast
	FM64_FIXTURES=fixtures $(BUILD)/fm64_fast bench

lint:
	-cppcheck --enable=warning,portability,performance --inline-suppr --std=c++14 \
	  $(DEFS) $(INC) --suppress=missingIncludeSystem --quiet $(SRC_DIR)/unit.cc $(SRC_DIR)/header.c
	-$(CXX) $(STD) $(INC) $(DEFS) -fsyntax-only -Wall -Wextra -Wshadow -Wimplicit-fallthrough \
	  -Wno-unknown-attributes -Wno-ignored-attributes -Wno-deprecated-register $(BUILD)/unit_host.cc

clean:
	rm -rf $(BUILD)

.PHONY: test regression fuzz bench lint clean
