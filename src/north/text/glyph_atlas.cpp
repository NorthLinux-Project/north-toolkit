#include "north/text/glyph_atlas.hpp"

#include <algorithm>
#include <cmath>

namespace north::text {

GlyphAtlas::GlyphAtlas(gpu::Device& device) : device_(device) {}

GlyphAtlas::~GlyphAtlas() {
    clear();
}

void GlyphAtlas::clear() {
    for (auto& page : pages_) {
        if (page.texture != gpu::kInvalidTexture) {
            device_.destroy_texture(page.texture);
        }
    }
    pages_.clear();
    cache_.clear();
}

size_t GlyphAtlas::add_page() {
    gpu::TextureDesc desc{.width = static_cast<uint32_t>(kPageSize),
                          .height = static_cast<uint32_t>(kPageSize),
                          .format = gpu::TextureFormat::R8,
                          .render_target = false};
    auto tex = device_.create_texture(desc);
    if (!tex) {
        return SIZE_MAX;
    }
    AtlasPage page;
    page.texture = *tex;
    page.skyline.push_back(SkylineNode{0, 0, kPageSize});
    pages_.push_back(page);
    return pages_.size() - 1;
}

bool GlyphAtlas::allocate_in_page(size_t page_idx, int width, int height, int& out_x, int& out_y) {
    auto& skyline = pages_[page_idx].skyline;
    int best_height = kPageSize;
    int best_width = kPageSize;
    int best_index = -1;
    out_x = 0;
    out_y = 0;

    for (size_t i = 0; i < skyline.size(); ++i) {
        int y = 0;
        if (skyline[i].x + width > kPageSize) {
            continue;
        }
        int w = width;
        size_t j = i;
        while (w > 0 && j < skyline.size()) {
            y = (std::max)(y, skyline[j].y);
            w -= skyline[j].width;
            ++j;
        }
        if (w <= 0 && y + height <= kPageSize) {
            if (y < best_height || (y == best_height && skyline[i].width < best_width)) {
                best_height = y;
                best_width = skyline[i].width;
                best_index = static_cast<int>(i);
                out_x = skyline[i].x;
                out_y = y;
            }
        }
    }

    if (best_index == -1) {
        return false;
    }

    SkylineNode new_node{out_x, out_y + height, width};
    skyline.insert(skyline.begin() + best_index, new_node);
    for (size_t i = best_index + 1; i < skyline.size();) {
        auto& prev = skyline[i - 1];
        auto& curr = skyline[i];
        if (curr.x < prev.x + prev.width) {
            int shrink = (prev.x + prev.width) - curr.x;
            curr.x += shrink;
            curr.width -= shrink;
            if (curr.width <= 0) {
                skyline.erase(skyline.begin() + i);
                continue;
            }
        }
        break;
    }

    for (size_t i = 0; i + 1 < skyline.size();) {
        if (skyline[i].y == skyline[i + 1].y) {
            skyline[i].width += skyline[i + 1].width;
            skyline.erase(skyline.begin() + i + 1);
        } else {
            ++i;
        }
    }

    return true;
}

CachedGlyph GlyphAtlas::get_or_insert_glyph(const std::shared_ptr<Font>& font, uint32_t glyph_id,
                                            float font_size, float subpixel_x) {
    if (!font) {
        return {};
    }

    float frac = subpixel_x - std::floor(subpixel_x);
    if (frac < 0.0f)
        frac += 1.0f;
    uint8_t bin = static_cast<uint8_t>(std::clamp(static_cast<int>(frac * 4.0f), 0, 3));
    uint16_t size_pt = static_cast<uint16_t>(font_size);

    GlyphKey key{
        .font_id = font->font_id(), .glyph_id = glyph_id, .size_pt = size_pt, .subpixel_bin = bin};

    auto it = cache_.find(key);
    if (it != cache_.end()) {
        return it->second;
    }

    font->set_pixel_size(font_size);
    FT_Face face = font->ft_face();
    if (!face) {
        return {};
    }

    FT_Vector shift;
    shift.x = static_cast<FT_Pos>((bin * 0.25f) * 64.0f);
    shift.y = 0;
    FT_Set_Transform(face, nullptr, &shift);

    if (FT_Load_Glyph(face, glyph_id, FT_LOAD_TARGET_LIGHT) != 0) {
        FT_Set_Transform(face, nullptr, nullptr);
        return {};
    }

    FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);
    FT_Set_Transform(face, nullptr, nullptr);

    FT_Bitmap& bmp = face->glyph->bitmap;
    int bw = static_cast<int>(bmp.width);
    int bh = static_cast<int>(bmp.rows);

    CachedGlyph cached{};
    cached.offset_x = static_cast<float>(face->glyph->bitmap_left);
    cached.offset_y = static_cast<float>(face->glyph->bitmap_top);
    cached.width = static_cast<float>(bw);
    cached.height = static_cast<float>(bh);

    if (bw <= 0 || bh <= 0 || !bmp.buffer) {
        cache_[key] = cached;
        return cached;
    }

    int alloc_w = bw + kPadding * 2;
    int alloc_h = bh + kPadding * 2;

    int pack_x = 0;
    int pack_y = 0;
    size_t target_page = SIZE_MAX;

    for (size_t p = 0; p < pages_.size(); ++p) {
        if (allocate_in_page(p, alloc_w, alloc_h, pack_x, pack_y)) {
            target_page = p;
            break;
        }
    }

    if (target_page == SIZE_MAX) {
        target_page = add_page();
        if (target_page == SIZE_MAX ||
            !allocate_in_page(target_page, alloc_w, alloc_h, pack_x, pack_y)) {
            return cached;
        }
    }

    cached.page_texture = pages_[target_page].texture;

    int glyph_x = pack_x + kPadding;
    int glyph_y = pack_y + kPadding;
    constexpr float inv_page = 1.0f / static_cast<float>(kPageSize);

    cached.uv_rect =
        core::Rect{static_cast<float>(glyph_x) * inv_page, static_cast<float>(glyph_y) * inv_page,
                   static_cast<float>(bw) * inv_page, static_cast<float>(bh) * inv_page};

    std::vector<uint8_t> padded(static_cast<size_t>(alloc_w * alloc_h), 0);
    for (int r = 0; r < bh; ++r) {
        std::memcpy(padded.data() + (r + kPadding) * alloc_w + kPadding, bmp.buffer + r * bmp.pitch,
                    static_cast<size_t>(bw));
    }

    core::Rect update_region{static_cast<float>(pack_x), static_cast<float>(pack_y),
                             static_cast<float>(alloc_w), static_cast<float>(alloc_h)};
    device_.update_texture(cached.page_texture, update_region, padded);

    cache_[key] = cached;
    return cached;
}

} // namespace north::text
