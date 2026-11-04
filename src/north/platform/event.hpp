#pragma once

#include "north/core/types.hpp"

#include <cstdint>

namespace north::platform {

enum class PointerButton : uint8_t { None, Left, Right, Middle };

enum class PointerType : uint8_t { Mouse, Touch, Pen };

enum class PointerAction : uint8_t { Down, Up, Move, Cancel };

inline constexpr uint32_t kModShift = 1 << 0;
inline constexpr uint32_t kModControl = 1 << 1;
inline constexpr uint32_t kModAlt = 1 << 2;

struct PointerEvent {
    PointerAction action{PointerAction::Move};
    PointerButton button{PointerButton::None};
    core::Point position{0.0f, 0.0f};
    uint32_t modifiers{0};
    float pressure{1.0f};
    PointerType type{PointerType::Mouse};
};

struct KeyEvent {
    uint32_t key_code{0};
    char32_t character{0};
    bool is_down{false};
    uint32_t modifiers{0};
};

} // namespace north::platform
