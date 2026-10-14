#include <cassert>
#include <cstdint>
#include "fm64.h"

int main() {
  for (uint32_t value = 0; value < 256; value++) {
    const uint8_t input = value;
    const uint8_t level = scale_level(input);
    assert(level <= 127);
    if (value < sizeof(level_lut)) assert(level == level_lut[value]);
    else if (value <= 99) assert(level == value + 28);
    else assert(level == 127);
    const int32_t fixed_level = level * 0x01000000;
    assert(fixed_level >= 0);
  }
}
