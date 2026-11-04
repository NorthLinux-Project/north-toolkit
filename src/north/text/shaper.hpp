#pragma once

#include "north/text/font.hpp"

#include <memory>
#include <string_view>
#include <vector>

namespace north::text {

enum class TextDirection : uint8_t { LTR, RTL };

struct ShapedGlyph {
    uint32_t glyph_id{0};
    uint32_t cluster{0};
    float advance_x{0.0f};
    float advance_y{0.0f};
    float offset_x{0.0f};
    float offset_y{0.0f};
};

struct ShapedRun {
    std::vector<ShapedGlyph> glyphs;
    TextDirection direction{TextDirection::LTR};
    float total_advance{0.0f};
    float font_size{16.0f};
    std::shared_ptr<Font> font;
};

class Shaper {
public:
    static ShapedRun shape_text(std::string_view text, const std::shared_ptr<Font>& font,
                                float font_size, TextDirection direction = TextDirection::LTR);
};

} // namespace north::text
