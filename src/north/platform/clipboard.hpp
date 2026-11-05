#pragma once

#include <string>
#include <string_view>

namespace north::platform {

class Clipboard {
public:
    static bool set_text(std::string_view text);
    static std::string get_text();
    static bool has_text();
};

} // namespace north::platform
