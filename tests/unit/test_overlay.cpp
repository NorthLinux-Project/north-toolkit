#include "north/core/tree.hpp"
#include "north/event/input_dispatcher.hpp"
#include "north/overlay/overlay_manager.hpp"
#include "north/overlay/positioning.hpp"
#include "north/widget/box.hpp"
#include "north/widget/button.hpp"
#include "north/widget/dialog.hpp"
#include "north/widget/menu.hpp"
#include "north/widget/popover.hpp"
#include "north/widget/tooltip.hpp"

#include <gtest/gtest.h>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace north::overlay::testing {

TEST(OverlayPositioningTest, ComputesBottomCenterPosition) {
    core::Rect anchor{100.0f, 100.0f, 80.0f, 40.0f};
    core::Size overlay_sz{120.0f, 60.0f};
    core::Size viewport{800.0f, 600.0f};

    core::Point pos = solve_overlay_position(anchor, overlay_sz, viewport, Placement::Bottom, 8.0f);

    // x = 100 + (80 - 120) * 0.5 = 80.0
    EXPECT_FLOAT_EQ(pos.x, 80.0f);
    // y = 100 + 40 + 8 = 148.0
    EXPECT_FLOAT_EQ(pos.y, 148.0f);
}

TEST(OverlayPositioningTest, ComputesTopCenterPosition) {
    core::Rect anchor{100.0f, 200.0f, 80.0f, 40.0f};
    core::Size overlay_sz{120.0f, 60.0f};
    core::Size viewport{800.0f, 600.0f};

    core::Point pos = solve_overlay_position(anchor, overlay_sz, viewport, Placement::Top, 8.0f);

    EXPECT_FLOAT_EQ(pos.x, 80.0f);
    // y = 200 - 60 - 8 = 132.0
    EXPECT_FLOAT_EQ(pos.y, 132.0f);
}

TEST(OverlayPositioningTest, FlipsBottomToTopOnViewportCollision) {
    // Anchor near bottom of viewport (height 600)
    core::Rect anchor{100.0f, 550.0f, 80.0f, 40.0f};
    core::Size overlay_sz{120.0f, 60.0f};
    core::Size viewport{800.0f, 600.0f};

    // Bottom would be 550 + 40 + 8 = 598, overlay ends at 658 > 600 -> should flip to Top
    core::Point pos = solve_overlay_position(anchor, overlay_sz, viewport, Placement::Bottom, 8.0f, true);

    // Flipped to top: y = 550 - 60 - 8 = 482.0
    EXPECT_FLOAT_EQ(pos.y, 482.0f);
}

TEST(OverlayPositioningTest, FlipsRightToLeftOnViewportCollision) {
    // Anchor near right edge of viewport (width 800)
    core::Rect anchor{750.0f, 100.0f, 40.0f, 40.0f};
    core::Size overlay_sz{100.0f, 50.0f};
    core::Size viewport{800.0f, 600.0f};

    // Right would be 750 + 40 + 8 = 798, overlay ends at 898 > 800 -> should flip to Left
    core::Point pos = solve_overlay_position(anchor, overlay_sz, viewport, Placement::Right, 8.0f, true);

    // Flipped to left: x = 750 - 100 - 8 = 642.0
    EXPECT_FLOAT_EQ(pos.x, 642.0f);
}

TEST(OverlayPositioningTest, ClampsToViewportBounds) {
    core::Rect anchor{0.0f, 0.0f, 20.0f, 20.0f};
    core::Size overlay_sz{100.0f, 50.0f};
    core::Size viewport{800.0f, 600.0f};

    // Top would be negative Y: 0 - 50 - 8 = -58 -> flipped to bottom, but x = 0 + (20-100)*0.5 = -40
    core::Point pos = solve_overlay_position(anchor, overlay_sz, viewport, Placement::Bottom, 8.0f, false, true);

    // Should clamp x to >= 0
    EXPECT_GE(pos.x, 0.0f);
    EXPECT_GE(pos.y, 0.0f);
}

TEST(OverlayManagerTest, ShowAndHideOverlayLifecycle) {
    core::NodeTree tree;
    OverlayManager mgr;

    auto content = tree.create_node();
    OverlayConfig cfg{.modal = true};

    OverlayId id = mgr.show_overlay(content, cfg);
    EXPECT_NE(id, kInvalidOverlayId);
    EXPECT_TRUE(mgr.has_active_overlays());
    EXPECT_TRUE(mgr.has_active_modal());

    const auto* entry = mgr.find_overlay(id);
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->id, id);

    mgr.hide_overlay(id);
    EXPECT_FALSE(mgr.has_active_overlays());
    EXPECT_FALSE(mgr.has_active_modal());
}

TEST(OverlayManagerTest, DismissCallbackExecutedOnHide) {
    core::NodeTree tree;
    OverlayManager mgr;

    auto content = tree.create_node();
    bool dismissed = false;
    OverlayConfig cfg{
        .modal = true,
        .barrier_dismissible = true,
        .on_dismiss = [](void* ud) { *static_cast<bool*>(ud) = true; },
        .dismiss_user_data = &dismissed,
    };

    mgr.show_overlay(content, cfg);
    EXPECT_FALSE(dismissed);

    mgr.dismiss_top_modal();
    EXPECT_TRUE(dismissed);
    EXPECT_FALSE(mgr.has_active_modal());
}

TEST(InputDispatcherOverlayTest, OutsideClickDismissesModalAndAbsorbsEvent) {
    core::NodeTree tree;
    auto root = tree.create_node();
    tree.set_root(root);

    // Root button
    bool root_clicked = false;
    auto root_btn = widget::ButtonBuilder(tree, "Root Button")
                        .on_click([](void* ud) { *static_cast<bool*>(ud) = true; }, &root_clicked)
                        .build();
    tree.append_child(root, root_btn);
    tree.layout(layout::BoxConstraints::tight(core::Size{800.0f, 600.0f}));

    OverlayManager mgr;
    event::InputDispatcher dispatcher(tree);
    dispatcher.set_overlay_manager(&mgr);

    // Modal overlay content (100x100 box at center)
    bool modal_dismissed = false;
    auto modal_box = widget::BoxBuilder(tree)
                         .size(core::Size{100.0f, 100.0f})
                         .background(core::Color{1.0f, 1.0f, 1.0f, 1.0f})
                         .build();

    OverlayConfig cfg{
        .modal = true,
        .barrier_dismissible = true,
        .on_dismiss = [](void* ud) { *static_cast<bool*>(ud) = true; },
        .dismiss_user_data = &modal_dismissed,
    };

    mgr.show_overlay_at_rect(modal_box, core::Rect{350.0f, 250.0f, 100.0f, 100.0f}, cfg);
    mgr.layout_overlays(tree, core::Size{800.0f, 600.0f});

    EXPECT_TRUE(mgr.has_active_modal());

    // Click outside modal (at 50, 50, over the root button)
    platform::PointerEvent down_evt{
        .action = platform::PointerAction::Down,
        .button = platform::PointerButton::Left,
        .position = core::Point{50.0f, 50.0f},
    };
    dispatcher.on_pointer(down_evt);

    // Modal should be dismissed, and root button should NOT have been clicked!
    EXPECT_TRUE(modal_dismissed);
    EXPECT_FALSE(root_clicked);
    EXPECT_FALSE(mgr.has_active_modal());
}

TEST(InputDispatcherOverlayTest, EscapeKeyDismissesModalOverlay) {
    core::NodeTree tree;
    OverlayManager mgr;
    event::InputDispatcher dispatcher(tree);
    dispatcher.set_overlay_manager(&mgr);

    bool dismissed = false;
    auto modal_node = tree.create_node();
    OverlayConfig cfg{
        .modal = true,
        .barrier_dismissible = true,
        .on_dismiss = [](void* ud) { *static_cast<bool*>(ud) = true; },
        .dismiss_user_data = &dismissed,
    };
    mgr.show_overlay(modal_node, cfg);
    EXPECT_TRUE(mgr.has_active_modal());

    platform::KeyEvent esc_evt{
        .key_code = VK_ESCAPE,
        .character = 0,
        .is_down = true,
        .modifiers = 0,
    };
    dispatcher.on_key(esc_evt);

    EXPECT_TRUE(dismissed);
    EXPECT_FALSE(mgr.has_active_modal());
}

TEST(DialogWidgetTest, ConstructsAndExecutesActionCallback) {
    core::NodeTree tree;
    OverlayManager mgr;

    bool confirmed = false;
    widget::DialogBuilder(tree, mgr)
        .title("Delete Item")
        .message("Are you sure you want to delete this file?")
        .action("Cancel")
        .action("Confirm", [](void* ud) { *static_cast<bool*>(ud) = true; }, &confirmed, true)
        .show();

    EXPECT_TRUE(mgr.has_active_modal());
    const auto* modal = mgr.top_modal();
    ASSERT_NE(modal, nullptr);

    mgr.layout_overlays(tree, core::Size{800.0f, 600.0f});
    EXPECT_GT(modal->computed_bounds.width, 0.0f);
    EXPECT_GT(modal->computed_bounds.height, 0.0f);
}

TEST(MenuWidgetTest, BuildsDropdownMenuWithDividersAndActions) {
    core::NodeTree tree;
    OverlayManager mgr;

    auto anchor = tree.create_node();
    bool item_clicked = false;

    widget::MenuBuilder(tree, mgr)
        .item("Open", [](void* ud) { *static_cast<bool*>(ud) = true; }, &item_clicked)
        .divider()
        .item("Quit")
        .show_at_anchor(anchor);

    EXPECT_TRUE(mgr.has_active_overlays());
    mgr.layout_overlays(tree, core::Size{800.0f, 600.0f});

    const auto* entry = mgr.top_modal();
    ASSERT_NE(entry, nullptr);
    EXPECT_GT(entry->computed_bounds.width, 0.0f);
}

TEST(TooltipWidgetTest, ConstructsTooltipWithOffset) {
    core::NodeTree tree;
    OverlayManager mgr;
    auto anchor = tree.create_node();

    overlay::OverlayId id = widget::show_tooltip(tree, mgr, anchor, "Help text");
    EXPECT_NE(id, kInvalidOverlayId);
    EXPECT_TRUE(mgr.has_active_overlays());
    // Tooltips are non-modal
    EXPECT_FALSE(mgr.has_active_modal());

    mgr.layout_overlays(tree, core::Size{800.0f, 600.0f});
    const auto* entry = mgr.find_overlay(id);
    ASSERT_NE(entry, nullptr);
    EXPECT_GT(entry->computed_bounds.width, 0.0f);
}

TEST(OverlayThemeTest, OverlaysFollowSeedThemeColors) {
    core::NodeTree tree;
    OverlayManager mgr;

    // Green seed theme
    auto seed_theme = style::Theme::from_hct(140.0f, 60.0f, true);
    auto anchor = tree.create_node();

    // 1. Dialog follows seed
    widget::DialogBuilder(tree, mgr)
        .theme(seed_theme)
        .title("Seed Dialog")
        .message("Following green seed")
        .action("OK")
        .show();

    const auto* dialog_entry = mgr.top_modal();
    ASSERT_NE(dialog_entry, nullptr);
    auto* dialog_node = tree.get(dialog_entry->content_node);
    ASSERT_NE(dialog_node, nullptr);
    auto* dialog_box = reinterpret_cast<widget::BoxData*>(dialog_node->user_data);
    ASSERT_NE(dialog_box, nullptr);
    EXPECT_FLOAT_EQ(dialog_box->style.background.r, seed_theme.surface_container_high().r);
    EXPECT_FLOAT_EQ(dialog_box->style.background.g, seed_theme.surface_container_high().g);
    EXPECT_FLOAT_EQ(dialog_box->style.background.b, seed_theme.surface_container_high().b);
    EXPECT_FLOAT_EQ(dialog_box->style.border_color.r, seed_theme.colors().outline_variant.r);
    mgr.hide_overlay(dialog_entry->id);

    // 2. Menu follows seed
    widget::MenuBuilder(tree, mgr)
        .theme(seed_theme)
        .item("Item 1")
        .divider()
        .show_at_anchor(anchor);

    const auto* menu_entry = mgr.top_modal();
    ASSERT_NE(menu_entry, nullptr);
    auto* menu_node = tree.get(menu_entry->content_node);
    ASSERT_NE(menu_node, nullptr);
    auto* menu_box = reinterpret_cast<widget::BoxData*>(menu_node->user_data);
    ASSERT_NE(menu_box, nullptr);
    EXPECT_FLOAT_EQ(menu_box->style.background.r, seed_theme.surface_container_high().r);
    EXPECT_FLOAT_EQ(menu_box->style.background.g, seed_theme.surface_container_high().g);
    EXPECT_FLOAT_EQ(menu_box->style.background.b, seed_theme.surface_container_high().b);
    EXPECT_FLOAT_EQ(menu_box->style.border_color.r, seed_theme.colors().outline_variant.r);
    mgr.hide_overlay(menu_entry->id);

    // 3. Popover follows seed
    auto pop_content = tree.create_node();
    widget::PopoverBuilder(tree, mgr, pop_content)
        .theme(seed_theme)
        .anchor(anchor)
        .show();

    const auto* popover_entry = mgr.top_modal();
    ASSERT_NE(popover_entry, nullptr);
    auto* popover_node = tree.get(popover_entry->content_node);
    ASSERT_NE(popover_node, nullptr);
    auto* popover_box = reinterpret_cast<widget::BoxData*>(popover_node->user_data);
    ASSERT_NE(popover_box, nullptr);
    EXPECT_FLOAT_EQ(popover_box->style.background.r, seed_theme.surface_container_high().r);
    EXPECT_FLOAT_EQ(popover_box->style.background.g, seed_theme.surface_container_high().g);
    EXPECT_FLOAT_EQ(popover_box->style.background.b, seed_theme.surface_container_high().b);
    EXPECT_FLOAT_EQ(popover_box->style.border_color.r, seed_theme.colors().outline_variant.r);
    mgr.hide_overlay(popover_entry->id);

    // 4. Tooltip follows seed
    OverlayId tooltip_id = widget::show_tooltip(tree, mgr, anchor, "Seed Tooltip",
                                                Placement::Top, seed_theme);
    const auto* tooltip_entry = mgr.find_overlay(tooltip_id);
    ASSERT_NE(tooltip_entry, nullptr);
    auto* tooltip_node = tree.get(tooltip_entry->content_node);
    ASSERT_NE(tooltip_node, nullptr);
    auto* tooltip_box = reinterpret_cast<widget::BoxData*>(tooltip_node->user_data);
    ASSERT_NE(tooltip_box, nullptr);
    EXPECT_FLOAT_EQ(tooltip_box->style.background.r, seed_theme.surface_container_highest().r);
    EXPECT_FLOAT_EQ(tooltip_box->style.background.g, seed_theme.surface_container_highest().g);
    EXPECT_FLOAT_EQ(tooltip_box->style.background.b, seed_theme.surface_container_highest().b);
    EXPECT_FLOAT_EQ(tooltip_box->style.border_color.r, seed_theme.colors().outline_variant.r);
    mgr.hide_overlay(tooltip_id);
}

} // namespace north::overlay::testing
