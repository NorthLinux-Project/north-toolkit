#pragma once

#include "north/text/font.hpp"

#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>

namespace north::text {

class FontManager {
public:
    FontManager();
    ~FontManager();

    FontManager(const FontManager&) = delete;
    FontManager& operator=(const FontManager&) = delete;

    [[nodiscard]] std::shared_ptr<Font> load_from_memory(std::span<const uint8_t> data);
    [[nodiscard]] std::shared_ptr<Font> load_from_file(const std::string& path);
    [[nodiscard]] std::shared_ptr<Font> load_system_font(std::string_view family_name = "Segoe UI");

    [[nodiscard]] FT_Library ft_library() const noexcept { return ft_lib_; }

    static FontManager& instance();

private:
    FT_Library ft_lib_{nullptr};
    std::unordered_map<std::string, std::shared_ptr<Font>> font_cache_;
};

} // namespace north::text
