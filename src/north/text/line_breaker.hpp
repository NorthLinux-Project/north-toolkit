#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

namespace north::text {

enum class BreakAction : uint8_t { MustBreak, AllowBreak, NoBreak, InsideChar };

class LineBreaker {
public:
    static std::vector<BreakAction> get_breaks(std::string_view utf8_text);
};

} // namespace north::text
