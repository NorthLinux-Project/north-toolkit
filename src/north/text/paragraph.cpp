#include "north/text/paragraph.hpp"

#include <algorithm>
#include <cmath>

namespace north::text {

Paragraph::Paragraph(std::vector<TextSpan> spans, TextAlign align)
    : spans_(std::move(spans)), align_(align) {}

struct TempGlyph {
    uint32_t glyph_id{0};
    float advance_x{0.0f};
    float offset_x{0.0f};
    float offset_y{0.0f};
    std::shared_ptr<Font> font;
    float font_size{16.0f};
    core::Color color;
    float ascender{0.0f};
    float descender{0.0f};
    float line_height{0.0f};
    bool allow_break{false};
    bool must_break{false};
};

void Paragraph::layout(const layout::BoxConstraints& constraints) {
    positioned_glyphs_.clear();
    lines_.clear();

    if (spans_.empty()) {
        size_ = constraints.constrain({0.0f, 0.0f});
        baseline_ = 0.0f;
        return;
    }

    std::vector<TempGlyph> unpositioned;

    for (const auto& span : spans_) {
        if (span.text.empty() || !span.style.font) {
            continue;
        }

        auto bidi_runs = Bidi::analyze(span.text);
        auto raw_breaks = LineBreaker::get_breaks(span.text);

        for (const auto& brun : bidi_runs) {
            std::string_view run_text(span.text.data() + brun.start, brun.length);
            auto shaped =
                Shaper::shape_text(run_text, span.style.font, span.style.font_size, brun.direction);

            float asc = span.style.font->ascender(span.style.font_size);
            float dsc = -span.style.font->descender(span.style.font_size);
            float lh = (asc + dsc) * span.style.line_height;

            for (size_t gi = 0; gi < shaped.glyphs.size(); ++gi) {
                const auto& sg = shaped.glyphs[gi];
                TempGlyph tg{};
                tg.glyph_id = sg.glyph_id;
                tg.advance_x = sg.advance_x + span.style.letter_spacing;
                tg.offset_x = sg.offset_x;
                tg.offset_y = sg.offset_y;
                tg.font = span.style.font;
                tg.font_size = span.style.font_size;
                tg.color = span.style.color;
                tg.ascender = asc;
                tg.descender = dsc;
                tg.line_height = lh;

                size_t text_idx = brun.start + sg.cluster;
                if (text_idx < raw_breaks.size()) {
                    tg.allow_break = (raw_breaks[text_idx] == BreakAction::AllowBreak ||
                                      raw_breaks[text_idx] == BreakAction::MustBreak);
                    tg.must_break = (raw_breaks[text_idx] == BreakAction::MustBreak);
                }

                unpositioned.push_back(tg);
            }
        }
    }

    if (unpositioned.empty()) {
        size_ = constraints.constrain({0.0f, 0.0f});
        baseline_ = 0.0f;
        return;
    }

    float max_allowed_w = constraints.max_width;
    float current_x = 0.0f;
    float current_y = 0.0f;
    size_t line_start = 0;
    float line_ascender = 0.0f;
    float line_descender = 0.0f;
    float line_height = 0.0f;
    size_t last_break_idx = SIZE_MAX;
    float last_break_x = 0.0f;
    float max_measured_w = 0.0f;

    auto finish_line = [&](size_t end_idx, float line_w) {
        if (end_idx <= line_start)
            return;

        ParagraphLine pline{};
        pline.start_glyph = line_start;
        pline.glyph_count = end_idx - line_start;
        pline.width = line_w;
        pline.height = line_height;
        pline.baseline = current_y + line_ascender;
        pline.y = current_y;
        lines_.push_back(pline);

        max_measured_w = (std::max)(max_measured_w, line_w);
        current_y += line_height;
        current_x = 0.0f;
        line_start = end_idx;
        line_ascender = 0.0f;
        line_descender = 0.0f;
        line_height = 0.0f;
        last_break_idx = SIZE_MAX;
    };

    for (size_t i = 0; i < unpositioned.size(); ++i) {
        const auto& g = unpositioned[i];
        line_ascender = (std::max)(line_ascender, g.ascender);
        line_descender = (std::max)(line_descender, g.descender);
        line_height = (std::max)(line_height, g.line_height);

        if (g.must_break) {
            current_x += g.advance_x;
            finish_line(i + 1, current_x);
            continue;
        }

        if (g.allow_break) {
            last_break_idx = i;
            last_break_x = current_x + g.advance_x;
        }

        if (current_x + g.advance_x > max_allowed_w && current_x > 0.0f) {
            if (last_break_idx != SIZE_MAX && last_break_idx > line_start) {
                finish_line(last_break_idx + 1, last_break_x);
                i = last_break_idx; // Will advance to last_break_idx + 1 on loop increment
                continue;
            } else {
                finish_line(i, current_x);
            }
        }

        current_x += g.advance_x;
    }

    if (line_start < unpositioned.size()) {
        finish_line(unpositioned.size(), current_x);
    }

    // Now resolve final horizontal alignment and positions for each line
    float resolved_width = constraints.constrain({max_measured_w, current_y}).width;
    positioned_glyphs_.reserve(unpositioned.size());

    for (auto& line : lines_) {
        float x_offset = 0.0f;
        if (align_ == TextAlign::Center) {
            x_offset = (std::max)(0.0f, (resolved_width - line.width) * 0.5f);
        } else if (align_ == TextAlign::Right) {
            x_offset = (std::max)(0.0f, resolved_width - line.width);
        }

        float cursor_x = x_offset;
        for (size_t gi = 0; gi < line.glyph_count; ++gi) {
            size_t idx = line.start_glyph + gi;
            const auto& tg = unpositioned[idx];

            GlyphPosition gp{};
            gp.glyph_id = tg.glyph_id;
            gp.pos = core::Point{cursor_x + tg.offset_x, line.baseline + tg.offset_y};
            gp.advance_x = tg.advance_x;
            gp.font = tg.font;
            gp.font_size = tg.font_size;
            gp.color = tg.color;

            positioned_glyphs_.push_back(gp);
            cursor_x += tg.advance_x;
        }
    }

    size_ = constraints.constrain({max_measured_w, current_y});
    baseline_ = lines_.empty() ? 0.0f : lines_[0].baseline;
}

void Paragraph::paint(render::DisplayList& dl, GlyphAtlas& atlas, core::Point origin,
                      std::optional<core::Color> color_override) {
    if (positioned_glyphs_.empty()) {
        return;
    }

    std::vector<gpu::GlyphInstance> batch_glyphs;
    gpu::TextureId active_texture = gpu::kInvalidTexture;

    auto flush_batch = [&]() {
        if (!batch_glyphs.empty() && active_texture != gpu::kInvalidTexture) {
            auto span = dl.alloc_glyphs(batch_glyphs.size());
            if (!span.empty()) {
                std::memcpy(span.data(), batch_glyphs.data(),
                            batch_glyphs.size() * sizeof(gpu::GlyphInstance));
                dl.push_glyph_run(active_texture, span);
            }
            batch_glyphs.clear();
        }
    };

    for (const auto& g : positioned_glyphs_) {
        float gx = origin.x + g.pos.x;
        auto cg = atlas.get_or_insert_glyph(g.font, g.glyph_id, g.font_size, gx);
        if (cg.width <= 0.0f || cg.height <= 0.0f || cg.page_texture == gpu::kInvalidTexture) {
            continue;
        }

        if (active_texture != gpu::kInvalidTexture && active_texture != cg.page_texture) {
            flush_batch();
        }

        active_texture = cg.page_texture;

        float baseline_y = std::floor(origin.y + g.pos.y + 0.5f);
        gpu::GlyphInstance inst{};
        inst.dst_rect =
            core::Rect{std::floor(gx) + cg.offset_x, baseline_y - cg.offset_y, cg.width, cg.height};
        inst.uv_rect = cg.uv_rect;
        inst.color = color_override.has_value() ? color_override->premultiply() : g.color.premultiply();
        inst.clip_rect = dl.current_clip();

        batch_glyphs.push_back(inst);
    }

    flush_batch();
}

ParagraphBuilder::ParagraphBuilder(TextAlign align) : align_(align) {}

void ParagraphBuilder::add_text(std::string_view text, const TextStyle& style) {
    spans_.push_back(TextSpan{std::string(text), style});
}

std::unique_ptr<Paragraph> ParagraphBuilder::build() {
    return std::make_unique<Paragraph>(std::move(spans_), align_);
}

} // namespace north::text
