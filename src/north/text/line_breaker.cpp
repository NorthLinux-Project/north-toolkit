#include "north/text/line_breaker.hpp"

#include <linebreak.h>

namespace north::text {

std::vector<BreakAction> LineBreaker::get_breaks(std::string_view utf8_text) {
    if (utf8_text.empty()) {
        return {};
    }

    std::vector<char> raw_breaks(utf8_text.size());
    set_linebreaks_utf8(reinterpret_cast<const utf8_t*>(utf8_text.data()), utf8_text.size(),
                        nullptr, raw_breaks.data());

    std::vector<BreakAction> actions(utf8_text.size());
    for (size_t i = 0; i < utf8_text.size(); ++i) {
        switch (raw_breaks[i]) {
        case LINEBREAK_MUSTBREAK:
            actions[i] = BreakAction::MustBreak;
            break;
        case LINEBREAK_ALLOWBREAK:
            actions[i] = BreakAction::AllowBreak;
            break;
        case LINEBREAK_INSIDEACHAR:
            actions[i] = BreakAction::InsideChar;
            break;
        case LINEBREAK_NOBREAK:
        default:
            actions[i] = BreakAction::NoBreak;
            break;
        }
    }

    return actions;
}

} // namespace north::text
