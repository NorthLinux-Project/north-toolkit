#pragma once

#include "north/core/types.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H
#include <memory>
#include <span>
#include <string>
#include <vector>

#include <hb.h>

namespace north::text {

class Font {
public:
    Font(FT_Face face, hb_font_t* hb_font, std::vector<uint8_t> data = {});
    ~Font();

    Font(const Font&) = delete;
    Font& operator=(const Font&) = delete;
    Font(Font&& other) noexcept;
    Font& operator=(Font&& other) noexcept;

    void set_pixel_size(float size);

    [[nodiscard]] FT_Face ft_face() const noexcept { return face_; }
    [[nodiscard]] hb_font_t* hb_font() const noexcept { return hb_font_; }
    [[nodiscard]] uint32_t font_id() const noexcept { return font_id_; }

    [[nodiscard]] int units_per_em() const noexcept;
    [[nodiscard]] float ascender(float pixel_size) const noexcept;
    [[nodiscard]] float descender(float pixel_size) const noexcept;
    [[nodiscard]] float line_height(float pixel_size) const noexcept;

    [[nodiscard]] std::string family_name() const;
    [[nodiscard]] std::string style_name() const;

private:
    FT_Face face_{nullptr};
    hb_font_t* hb_font_{nullptr};
    std::vector<uint8_t> data_{};
    uint32_t font_id_{0};
    float current_size_{0.0f};

    static uint32_t next_font_id() noexcept;
};

} // namespace north::text
