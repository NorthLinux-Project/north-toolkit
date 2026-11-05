#pragma once

#include "north/core/types.hpp"
#include "north/gpu/types.hpp"
#include "north/render/path.hpp"

#include <cstdint>
#include <vector>

namespace north::render {

struct PathMesh {
    std::vector<gpu::PathVertex> vertices;
    std::vector<uint16_t> indices;

    void clear() noexcept {
        vertices.clear();
        indices.clear();
    }

    [[nodiscard]] bool empty() const noexcept { return indices.empty(); }
    [[nodiscard]] size_t vertex_count() const noexcept { return vertices.size(); }
    [[nodiscard]] size_t index_count() const noexcept { return indices.size(); }
};

class PathTessellator {
public:
    PathTessellator() = default;

    static void tessellate_fill(const Path& path, core::Color color, core::Rect clip_rect,
                                FillRule rule, PathMesh& out_mesh);

    static void tessellate_stroke(const Path& path, core::Color color, core::Rect clip_rect,
                                  const StrokeStyle& style, PathMesh& out_mesh);
};

} // namespace north::render
