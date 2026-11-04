#pragma once

#include "north/core/types.hpp"
#include "north/layout/constraints.hpp"
#include "north/render/display_list.hpp"
#include "north/text/bidi.hpp"
#include "north/text/font.hpp"
#include "north/text/glyph_atlas.hpp"
#include "north/text/line_breaker.hpp"
#include "north/text/shaper.hpp"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace north::text {

struct TextStyle {
    std::shared_ptr<Font> font;
    float font_size{16.0f};
    core::Color color{core::Color::from_rgba8(255, 255, 255, 255)};
    float line_height{1.2f};
    float letter_spacing{0.0f};
};

enum class TextAlign : uint8_t { Left, Center, Right };

struct TextSpan {
    std::string text;
    TextStyle style;
};

struct GlyphPosition {
    uint32_t glyph_id{0};
    core::Point pos{0.0f, 0.0f};
    float advance_x{0.0f};
    std::shared_ptr<Font> font;
    float font_size{16.0f};
    core::Color color;
};

struct ParagraphLine {
    size_t start_glyph{0};
    size_t glyph_count{0};
    float width{0.0f};
    float height{0.0f};
    float baseline{0.0f};
    float y{0.0f};
};

class Paragraph {
public:
    explicit Paragraph(std::vector<TextSpan> spans, TextAlign align = TextAlign::Left);

    void layout(const layout::BoxConstraints& constraints);
    void paint(render::DisplayList& dl, GlyphAtlas& atlas, core::Point origin);

    [[nodiscard]] core::Size size() const noexcept { return size_; }
    [[nodiscard]] float baseline() const noexcept { return baseline_; }
    [[nodiscard]] size_t line_count() const noexcept { return lines_.size(); }
    [[nodiscard]] const std::vector<ParagraphLine>& lines() const noexcept { return lines_; }
    [[nodiscard]] const std::vector<GlyphPosition>& glyphs() const noexcept {
        return positioned_glyphs_;
    }

private:
    std::vector<TextSpan> spans_;
    TextAlign align_{TextAlign::Left};
    core::Size size_{0.0f, 0.0f};
    float baseline_{0.0f};
    std::vector<GlyphPosition> positioned_glyphs_;
    std::vector<ParagraphLine> lines_;
};

class ParagraphBuilder {
public:
    explicit ParagraphBuilder(TextAlign align = TextAlign::Left);

    void add_text(std::string_view text, const TextStyle& style);
    [[nodiscard]] std::unique_ptr<Paragraph> build();

private:
    std::vector<TextSpan> spans_;
    TextAlign align_{TextAlign::Left};
};

} // namespace north::text
