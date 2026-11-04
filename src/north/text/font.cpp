#include "north/text/font.hpp"

#include <atomic>

#include <hb-ft.h>

namespace north::text {

uint32_t Font::next_font_id() noexcept {
    static std::atomic<uint32_t> id_counter{1};
    return id_counter.fetch_add(1, std::memory_order_relaxed);
}

Font::Font(FT_Face face, hb_font_t* hb_font, std::vector<uint8_t> data)
    : face_(face), hb_font_(hb_font), data_(std::move(data)), font_id_(next_font_id()) {}

Font::~Font() {
    if (hb_font_) {
        hb_font_destroy(hb_font_);
        hb_font_ = nullptr;
    }
    if (face_) {
        FT_Done_Face(face_);
        face_ = nullptr;
    }
}

Font::Font(Font&& other) noexcept
    : face_(other.face_), hb_font_(other.hb_font_), data_(std::move(other.data_)),
      font_id_(other.font_id_), current_size_(other.current_size_) {
    other.face_ = nullptr;
    other.hb_font_ = nullptr;
    other.font_id_ = 0;
}

Font& Font::operator=(Font&& other) noexcept {
    if (this != &other) {
        if (hb_font_) {
            hb_font_destroy(hb_font_);
        }
        if (face_) {
            FT_Done_Face(face_);
        }

        face_ = other.face_;
        hb_font_ = other.hb_font_;
        data_ = std::move(other.data_);
        font_id_ = other.font_id_;
        current_size_ = other.current_size_;

        other.face_ = nullptr;
        other.hb_font_ = nullptr;
        other.font_id_ = 0;
    }
    return *this;
}

void Font::set_pixel_size(float size) {
    if (face_ && size != current_size_) {
        FT_Set_Pixel_Sizes(face_, 0, static_cast<FT_UInt>(size > 1.0f ? size : 1.0f));
        current_size_ = size;
        if (hb_font_) {
            hb_ft_font_changed(hb_font_);
        }
    }
}

int Font::units_per_em() const noexcept {
    return face_ ? face_->units_per_EM : 1000;
}

float Font::ascender(float pixel_size) const noexcept {
    if (!face_ || face_->units_per_EM == 0)
        return pixel_size * 0.8f;
    return (static_cast<float>(face_->ascender) / static_cast<float>(face_->units_per_EM)) *
           pixel_size;
}

float Font::descender(float pixel_size) const noexcept {
    if (!face_ || face_->units_per_EM == 0)
        return pixel_size * -0.2f;
    return (static_cast<float>(face_->descender) / static_cast<float>(face_->units_per_EM)) *
           pixel_size;
}

float Font::line_height(float pixel_size) const noexcept {
    if (!face_ || face_->units_per_EM == 0)
        return pixel_size * 1.2f;
    return (static_cast<float>(face_->height) / static_cast<float>(face_->units_per_EM)) *
           pixel_size;
}

std::string Font::family_name() const {
    if (face_ && face_->family_name) {
        return face_->family_name;
    }
    return "Unknown";
}

std::string Font::style_name() const {
    if (face_ && face_->style_name) {
        return face_->style_name;
    }
    return "Regular";
}

} // namespace north::text
