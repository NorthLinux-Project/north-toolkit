#pragma once

#include "north/render/path.hpp"

#include <string_view>

namespace north::render {

[[nodiscard]] bool parse_svg_path(std::string_view d, Path& out_path) noexcept;

} // namespace north::render
