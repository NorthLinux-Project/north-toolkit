#include "north/overlay/positioning.hpp"

#include <algorithm>

namespace north::overlay {

core::Point solve_overlay_position(core::Rect anchor, core::Size overlay_size,
                                   core::Size viewport_size, Placement placement,
                                   float offset, bool flip_on_collision, bool clamp) noexcept {
    if (placement == Placement::Auto) {
        float space_below = viewport_size.height - (anchor.y + anchor.height);
        float space_above = anchor.y;
        placement = (space_below >= overlay_size.height || space_below >= space_above)
                        ? Placement::Bottom
                        : Placement::Top;
    }

    float x = 0.0f;
    float y = 0.0f;

    auto compute_pos = [&](Placement p) noexcept {
        switch (p) {
        case Placement::Top:
            x = anchor.x + (anchor.width - overlay_size.width) * 0.5f;
            y = anchor.y - overlay_size.height - offset;
            break;
        case Placement::TopStart:
            x = anchor.x;
            y = anchor.y - overlay_size.height - offset;
            break;
        case Placement::TopEnd:
            x = anchor.x + anchor.width - overlay_size.width;
            y = anchor.y - overlay_size.height - offset;
            break;
        case Placement::Bottom:
            x = anchor.x + (anchor.width - overlay_size.width) * 0.5f;
            y = anchor.y + anchor.height + offset;
            break;
        case Placement::BottomStart:
            x = anchor.x;
            y = anchor.y + anchor.height + offset;
            break;
        case Placement::BottomEnd:
            x = anchor.x + anchor.width - overlay_size.width;
            y = anchor.y + anchor.height + offset;
            break;
        case Placement::Left:
            x = anchor.x - overlay_size.width - offset;
            y = anchor.y + (anchor.height - overlay_size.height) * 0.5f;
            break;
        case Placement::LeftStart:
            x = anchor.x - overlay_size.width - offset;
            y = anchor.y;
            break;
        case Placement::LeftEnd:
            x = anchor.x - overlay_size.width - offset;
            y = anchor.y + anchor.height - overlay_size.height;
            break;
        case Placement::Right:
            x = anchor.x + anchor.width + offset;
            y = anchor.y + (anchor.height - overlay_size.height) * 0.5f;
            break;
        case Placement::RightStart:
            x = anchor.x + anchor.width + offset;
            y = anchor.y;
            break;
        case Placement::RightEnd:
            x = anchor.x + anchor.width + offset;
            y = anchor.y + anchor.height - overlay_size.height;
            break;
        case Placement::Auto:
            break;
        }
    };

    compute_pos(placement);

    if (flip_on_collision) {
        switch (placement) {
        case Placement::Top:
        case Placement::TopStart:
        case Placement::TopEnd:
            if (y < 0.0f) {
                float space_below = viewport_size.height - (anchor.y + anchor.height + offset);
                if (space_below >= overlay_size.height || space_below > anchor.y) {
                    Placement flipped = (placement == Placement::Top)        ? Placement::Bottom
                                        : (placement == Placement::TopStart) ? Placement::BottomStart
                                                                             : Placement::BottomEnd;
                    compute_pos(flipped);
                }
            }
            break;
        case Placement::Bottom:
        case Placement::BottomStart:
        case Placement::BottomEnd:
            if (y + overlay_size.height > viewport_size.height) {
                float space_above = anchor.y - offset;
                float space_below = viewport_size.height - (anchor.y + anchor.height + offset);
                if (space_above >= overlay_size.height || space_above > space_below) {
                    Placement flipped = (placement == Placement::Bottom)        ? Placement::Top
                                        : (placement == Placement::BottomStart) ? Placement::TopStart
                                                                                : Placement::TopEnd;
                    compute_pos(flipped);
                }
            }
            break;
        case Placement::Left:
        case Placement::LeftStart:
        case Placement::LeftEnd:
            if (x < 0.0f) {
                float space_right = viewport_size.width - (anchor.x + anchor.width + offset);
                if (space_right >= overlay_size.width || space_right > anchor.x) {
                    Placement flipped = (placement == Placement::Left)        ? Placement::Right
                                        : (placement == Placement::LeftStart) ? Placement::RightStart
                                                                              : Placement::RightEnd;
                    compute_pos(flipped);
                }
            }
            break;
        case Placement::Right:
        case Placement::RightStart:
        case Placement::RightEnd:
            if (x + overlay_size.width > viewport_size.width) {
                float space_left = anchor.x - offset;
                float space_right = viewport_size.width - (anchor.x + anchor.width + offset);
                if (space_left >= overlay_size.width || space_left > space_right) {
                    Placement flipped = (placement == Placement::Right)        ? Placement::Left
                                        : (placement == Placement::RightStart) ? Placement::LeftStart
                                                                               : Placement::LeftEnd;
                    compute_pos(flipped);
                }
            }
            break;
        case Placement::Auto:
            break;
        }
    }

    if (clamp) {
        float max_x = (std::max)(0.0f, viewport_size.width - overlay_size.width);
        float max_y = (std::max)(0.0f, viewport_size.height - overlay_size.height);
        x = std::clamp(x, 0.0f, max_x);
        y = std::clamp(y, 0.0f, max_y);
    }

    return core::Point{x, y};
}

} // namespace north::overlay
