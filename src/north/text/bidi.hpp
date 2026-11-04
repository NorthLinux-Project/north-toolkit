#pragma once

#include "north/text/shaper.hpp"

#include <cstdint>
#include <string_view>
#include <vector>

namespace north::text {

struct BidiRun {
    size_t start{0};
    size_t length{0};
    uint8_t level{0};
    TextDirection direction{TextDirection::LTR};
};

class Bidi {
public:
    static std::vector<BidiRun> analyze(std::string_view utf8_text);
};

} // namespace north::text
