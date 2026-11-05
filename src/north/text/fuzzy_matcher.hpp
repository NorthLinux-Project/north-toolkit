#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

namespace north::text {

struct FuzzyMatchResult {
    bool matched{false};
    int32_t score{0};
    std::vector<uint32_t> matched_indices;
};

[[nodiscard]] FuzzyMatchResult fuzzy_match(std::string_view pattern, std::string_view target);

} // namespace north::text
