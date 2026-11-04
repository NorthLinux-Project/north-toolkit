#include "north/text/shaper.hpp"

namespace north::text {

ShapedRun Shaper::shape_text(std::string_view text, const std::shared_ptr<Font>& font,
                             float font_size, TextDirection direction) {
    ShapedRun run{};
    run.direction = direction;
    run.font_size = font_size;
    run.font = font;

    if (!font || text.empty()) {
        return run;
    }

    font->set_pixel_size(font_size);

    hb_buffer_t* buf = hb_buffer_create();
    hb_buffer_add_utf8(buf, text.data(), static_cast<int>(text.size()), 0,
                       static_cast<int>(text.size()));
    hb_buffer_set_direction(buf,
                            direction == TextDirection::LTR ? HB_DIRECTION_LTR : HB_DIRECTION_RTL);
    hb_buffer_guess_segment_properties(buf);

    hb_shape(font->hb_font(), buf, nullptr, 0);

    unsigned int glyph_count = 0;
    hb_glyph_info_t* infos = hb_buffer_get_glyph_infos(buf, &glyph_count);
    hb_glyph_position_t* positions = hb_buffer_get_glyph_positions(buf, &glyph_count);

    run.glyphs.reserve(glyph_count);
    constexpr float scale_inv = 1.0f / 64.0f;

    for (unsigned int i = 0; i < glyph_count; ++i) {
        ShapedGlyph g{};
        g.glyph_id = infos[i].codepoint;
        g.cluster = infos[i].cluster;
        g.advance_x = static_cast<float>(positions[i].x_advance) * scale_inv;
        g.advance_y = static_cast<float>(positions[i].y_advance) * scale_inv;
        g.offset_x = static_cast<float>(positions[i].x_offset) * scale_inv;
        g.offset_y = static_cast<float>(positions[i].y_offset) * scale_inv;
        run.total_advance += g.advance_x;
        run.glyphs.push_back(g);
    }

    hb_buffer_destroy(buf);
    return run;
}

} // namespace north::text
