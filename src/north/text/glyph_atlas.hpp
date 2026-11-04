#pragma once

#include "north/core/types.hpp"
#include "north/gpu/device.hpp"
#include "north/text/font.hpp"

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

namespace north::text {

struct GlyphKey {
    uint32_t font_id{0};
    uint32_t glyph_id{0};
    uint16_t size_pt{0};
    uint8_t subpixel_bin{0};

    bool operator==(const GlyphKey& other) const noexcept {
        return font_id == other.font_id && glyph_id == other.glyph_id && size_pt == other.size_pt &&
               subpixel_bin == other.subpixel_bin;
    }
};

struct GlyphKeyHash {
    size_t operator()(const GlyphKey& k) const noexcept {
        return (static_cast<size_t>(k.font_id) * 73856093) ^
               (static_cast<size_t>(k.glyph_id) * 19349663) ^
               (static_cast<size_t>(k.size_pt) * 83492791) ^ (static_cast<size_t>(k.subpixel_bin));
    }
};

struct CachedGlyph {
    gpu::TextureId page_texture{gpu::kInvalidTexture};
    core::Rect uv_rect{}; // u0, v0, u1, v1
    float offset_x{0.0f};
    float offset_y{0.0f};
    float width{0.0f};
    float height{0.0f};
};

struct SkylineNode {
    int x{0};
    int y{0};
    int width{0};
};

struct AtlasPage {
    gpu::TextureId texture{gpu::kInvalidTexture};
    std::vector<SkylineNode> skyline;
};

class GlyphAtlas {
public:
    explicit GlyphAtlas(gpu::Device& device);
    ~GlyphAtlas();

    GlyphAtlas(const GlyphAtlas&) = delete;
    GlyphAtlas& operator=(const GlyphAtlas&) = delete;

    CachedGlyph get_or_insert_glyph(const std::shared_ptr<Font>& font, uint32_t glyph_id,
                                    float font_size, float subpixel_x = 0.0f);

    void clear();

    [[nodiscard]] size_t page_count() const noexcept { return pages_.size(); }
    [[nodiscard]] size_t cached_glyph_count() const noexcept { return cache_.size(); }

private:
    bool allocate_in_page(size_t page_idx, int width, int height, int& out_x, int& out_y);
    size_t add_page();

    gpu::Device& device_;
    std::vector<AtlasPage> pages_;
    std::unordered_map<GlyphKey, CachedGlyph, GlyphKeyHash> cache_;

    static constexpr int kPageSize = 1024;
    static constexpr int kPadding = 1;
};

} // namespace north::text
