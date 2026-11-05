#include "north/core/tree.hpp"
#include "north/event/input_dispatcher.hpp"
#include "north/overlay/overlay_manager.hpp"
#include "north/platform/event.hpp"
#include "north/text/fuzzy_matcher.hpp"
#include "north/widget/combo_box.hpp"
#include "north/widget/command_palette.hpp"
#include "north/widget/select.hpp"

#include <gtest/gtest.h>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

using namespace north;

TEST(FuzzyMatcherTest, SubsequenceMatchingAndScoring) {
    auto res_empty = text::fuzzy_match("", "Hello World");
    EXPECT_TRUE(res_empty.matched);
    EXPECT_EQ(res_empty.score, 0);
    EXPECT_TRUE(res_empty.matched_indices.empty());

    auto res_nomatch = text::fuzzy_match("xyz", "Command Palette");
    EXPECT_FALSE(res_nomatch.matched);

    auto res_match = text::fuzzy_match("cp", "Command Palette");
    EXPECT_TRUE(res_match.matched);
    EXPECT_GT(res_match.score, 0);
    ASSERT_EQ(res_match.matched_indices.size(), 2u);
    EXPECT_EQ(res_match.matched_indices[0], 0u); // 'C'
    EXPECT_EQ(res_match.matched_indices[1], 8u); // 'P' (word boundary after space)
}

TEST(FuzzyMatcherTest, WordBoundaryAndCamelCaseBonuses) {
    auto res_word_bound = text::fuzzy_match("cp", "Command Palette");
    auto res_internal = text::fuzzy_match("oa", "Command Palette");

    EXPECT_TRUE(res_word_bound.matched);
    EXPECT_TRUE(res_internal.matched);
    EXPECT_GT(res_word_bound.score, res_internal.score);

    // CamelCase boundary: 'v' and 'm' in 'ViewMode'
    auto res_camel = text::fuzzy_match("vm", "ViewMode");
    EXPECT_TRUE(res_camel.matched);
    ASSERT_EQ(res_camel.matched_indices.size(), 2u);
    EXPECT_EQ(res_camel.matched_indices[0], 0u);
    EXPECT_EQ(res_camel.matched_indices[1], 4u);
}

TEST(CommandPaletteTest, FilteringAndSorting) {
    core::NodeTree tree;
    auto cp_handle = widget::CommandPaletteBuilder(tree)
                         .item(1, "Open File", "File", "Ctrl+O")
                         .item(2, "Format Document", "Edit", "Shift+Alt+F")
                         .item(3, "Toggle Terminal", "View", "Ctrl+`")
                         .item(4, "Git: Commit", "Source Control", "Ctrl+Enter")
                         .build();

    auto* node = tree.get(cp_handle);
    ASSERT_NE(node, nullptr);
    auto* cp_data = reinterpret_cast<widget::CommandPaletteData*>(node->user_data);
    ASSERT_NE(cp_data, nullptr);

    EXPECT_EQ(cp_data->filtered_matches.size(), 4u);

    // Filter by "git"
    cp_data->set_query("git");
    ASSERT_EQ(cp_data->filtered_matches.size(), 1u);
    EXPECT_EQ(cp_data->filtered_matches[0].item->id, 4u);

    // Clear filter
    cp_data->set_query("");
    EXPECT_EQ(cp_data->filtered_matches.size(), 4u);
}

TEST(CommandPaletteTest, ModePrefixSwitching) {
    core::NodeTree tree;
    auto cp_handle = widget::CommandPaletteBuilder(tree)
                         .mode_prefix('>')
                         .item(10, "Preferences: Open Settings", "Preferences")
                         .item(20, "File: Save", "File")
                         .build();

    auto* node = tree.get(cp_handle);
    auto* cp_data = reinterpret_cast<widget::CommandPaletteData*>(node->user_data);

    // With prefix '>' and query ">save", match should match "save"
    cp_data->set_query(">save");
    ASSERT_EQ(cp_data->filtered_matches.size(), 1u);
    EXPECT_EQ(cp_data->filtered_matches[0].item->id, 20u);
}

TEST(CommandPaletteTest, SelectionNavigationAndExecution) {
    core::NodeTree tree;
    uint32_t executed_id = 0;

    auto cp_handle = widget::CommandPaletteBuilder(tree)
                         .item(1, "First Action")
                         .item(2, "Second Action")
                         .item(3, "Third Action")
                         .on_action([](uint32_t id, void* ud) {
                             *static_cast<uint32_t*>(ud) = id;
                         }, &executed_id)
                         .build();

    auto* node = tree.get(cp_handle);
    auto* cp_data = reinterpret_cast<widget::CommandPaletteData*>(node->user_data);

    EXPECT_EQ(cp_data->selected_index, 0u);
    cp_data->select_next();
    EXPECT_EQ(cp_data->selected_index, 1u);
    cp_data->select_next();
    EXPECT_EQ(cp_data->selected_index, 2u);
    cp_data->select_next();
    EXPECT_EQ(cp_data->selected_index, 0u); // wraps around

    cp_data->select_prev();
    EXPECT_EQ(cp_data->selected_index, 2u);

    cp_data->execute_selected();
    EXPECT_EQ(executed_id, 3u);
}

TEST(SelectTest, ItemSelectionAndCallbacks) {
    core::NodeTree tree;
    uint32_t selected_id = 0;
    size_t selected_idx = 0;

    auto sel_handle = widget::SelectBuilder(tree)
                          .item(100, "Light Theme")
                          .item(200, "Dark Theme")
                          .item(300, "High Contrast")
                          .on_change([](uint32_t id, size_t idx, void* ud) {
                              auto* p = static_cast<std::pair<uint32_t*, size_t*>*>(ud);
                              *p->first = id;
                              *p->second = idx;
                          }, new std::pair<uint32_t*, size_t*>(&selected_id, &selected_idx))
                          .build();

    auto* node = tree.get(sel_handle);
    ASSERT_NE(node, nullptr);
    auto* sel_data = reinterpret_cast<widget::SelectData*>(node->user_data);
    ASSERT_NE(sel_data, nullptr);

    EXPECT_EQ(sel_data->selected_index, 0u);
    sel_data->select_by_index(1);
    EXPECT_EQ(selected_id, 200u);
    EXPECT_EQ(selected_idx, 1u);

    sel_data->select_by_id(300);
    EXPECT_EQ(selected_id, 300u);
    EXPECT_EQ(selected_idx, 2u);

    delete static_cast<std::pair<uint32_t*, size_t*>*>(sel_data->change_user_data);
}

TEST(ComboBoxTest, RealTimeTextFilteringAndSelection) {
    core::NodeTree tree;
    uint32_t committed_id = 0;
    std::string changed_text;

    auto cb_handle = widget::ComboBoxBuilder(tree)
                         .item(1, "TypeScript")
                         .item(2, "C++20")
                         .item(3, "Python")
                         .item(4, "Rust")
                         .on_select([](uint32_t id, std::string_view, void* ud) {
                             *static_cast<uint32_t*>(ud) = id;
                         }, &committed_id)
                         .on_text_change([](std::string_view txt, void* ud) {
                             *static_cast<std::string*>(ud) = std::string(txt);
                         }, &changed_text)
                         .build();

    auto* node = tree.get(cb_handle);
    ASSERT_NE(node, nullptr);
    auto* cb_data = reinterpret_cast<widget::ComboBoxData*>(node->user_data);
    ASSERT_NE(cb_data, nullptr);

    EXPECT_EQ(cb_data->filtered_indices.size(), 4u);

    // Type "c++"
    cb_data->set_text("c++");
    EXPECT_EQ(changed_text, "c++");
    ASSERT_EQ(cb_data->filtered_indices.size(), 1u);
    EXPECT_EQ(cb_data->filtered_indices[0], 1u);

    // Select the filtered item
    cb_data->select_by_index(0);
    EXPECT_EQ(committed_id, 2u);
    EXPECT_EQ(cb_data->text, "C++20");
}

TEST(InputDispatcherTest, AcceleratorRegistrationAndFiring) {
    core::NodeTree tree;
    event::InputDispatcher dispatcher(tree);

    bool accelerator_fired = false;
    event::KeyShortcut shortcut{.key_code = 'P',
                                .modifiers = platform::kModControl | platform::kModShift};

    dispatcher.register_accelerator(shortcut, [](void* ud) {
        *static_cast<bool*>(ud) = true;
    }, &accelerator_fired);

    // Send non-matching key (Ctrl+P without Shift)
    platform::KeyEvent e1{.key_code = 'P', .is_down = true, .modifiers = platform::kModControl};
    dispatcher.on_key(e1);
    EXPECT_FALSE(accelerator_fired);

    // Send matching key (Ctrl+Shift+P)
    platform::KeyEvent e2{.key_code = 'P',
                          .is_down = true,
                          .modifiers = platform::kModControl | platform::kModShift};
    dispatcher.on_key(e2);
    EXPECT_TRUE(accelerator_fired);

    // Unregister accelerator
    accelerator_fired = false;
    dispatcher.unregister_accelerator(shortcut);
    dispatcher.on_key(e2);
    EXPECT_FALSE(accelerator_fired);
}

TEST(OverlayDismissalTest, ReentrantHideOverlayDuringDismissDoesNotCrash) {
    core::NodeTree tree;
    overlay::OverlayManager mgr;

    struct DismissContext {
        overlay::OverlayManager* mgr{nullptr};
        overlay::OverlayId id{overlay::kInvalidOverlayId};
        bool called{false};
    };

    DismissContext ctx;
    ctx.mgr = &mgr;

    auto dummy_node = tree.create_node();
    overlay::OverlayConfig cfg;
    cfg.modal = true;
    cfg.barrier_dismissible = true;
    cfg.on_dismiss = [](void* ud) {
        auto* c = static_cast<DismissContext*>(ud);
        c->called = true;
        // Re-entrant hide_overlay call inside on_dismiss
        c->mgr->hide_overlay(c->id);
    };
    cfg.dismiss_user_data = &ctx;

    ctx.id = mgr.show_overlay_at_point(dummy_node, {100.0f, 100.0f}, cfg);
    EXPECT_TRUE(mgr.has_active_modal());

    // Dismissing top modal must not crash or trigger "vector erase iterator outside range"
    EXPECT_NO_FATAL_FAILURE(mgr.dismiss_top_modal());
    EXPECT_TRUE(ctx.called);
    EXPECT_FALSE(mgr.has_active_modal());
}

TEST(SelectTest, OpenDropdownAndClickItem) {
    core::NodeTree tree;
    overlay::OverlayManager mgr;
    event::InputDispatcher dispatcher(tree);
    dispatcher.set_overlay_manager(&mgr);

    uint32_t selected_id = 0;
    auto sel_handle = widget::SelectBuilder(tree)
                          .item(1, "Option 1")
                          .item(2, "Option 2")
                          .overlay_manager(mgr)
                          .on_change([](uint32_t id, size_t, void* ud) {
                              *static_cast<uint32_t*>(ud) = id;
                          }, &selected_id)
                          .build();

    tree.set_root(sel_handle);
    tree.layout(layout::BoxConstraints::tight({200.0f, 40.0f}));
    mgr.layout_overlays(tree, {800.0f, 600.0f});

    // 1. Click select button to open
    platform::PointerEvent down1{.action = platform::PointerAction::Down,
                                 .button = platform::PointerButton::Left,
                                 .position = {20.0f, 20.0f}};
    dispatcher.on_pointer(down1);

    EXPECT_TRUE(mgr.has_active_overlays());
    mgr.layout_overlays(tree, {800.0f, 600.0f});

    // 2. Click item 2 in dropdown popup
    platform::PointerEvent down2{.action = platform::PointerAction::Down,
                                 .button = platform::PointerButton::Left,
                                 .position = {20.0f, 80.0f}};
    dispatcher.on_pointer(down2);
    EXPECT_EQ(selected_id, 2u);
    EXPECT_FALSE(mgr.has_active_overlays());
}

TEST(SelectTest, ReentrantTreeRebuildOnSelectDoesNotCrash) {
    core::NodeTree tree;
    overlay::OverlayManager mgr;
    event::InputDispatcher dispatcher(tree);
    dispatcher.set_overlay_manager(&mgr);

    struct RebuildContext {
        core::NodeTree* tree_ptr{nullptr};
        overlay::OverlayManager* mgr_ptr{nullptr};
        core::Handle<core::Node> sel_node{};
        bool rebuilt{false};

        void execute_rebuild() {
            if (tree_ptr->root().is_valid()) {
                tree_ptr->destroy_node(tree_ptr->root());
            }
            sel_node = widget::SelectBuilder(*tree_ptr)
                           .item(1, "Option 1")
                           .item(2, "Option 2")
                           .overlay_manager(*mgr_ptr)
                           .build();
            tree_ptr->set_root(sel_node);
            rebuilt = true;
        }
    };

    RebuildContext ctx;
    ctx.tree_ptr = &tree;
    ctx.mgr_ptr = &mgr;
    ctx.execute_rebuild();

    tree.layout(layout::BoxConstraints::tight({200.0f, 40.0f}));
    mgr.layout_overlays(tree, {800.0f, 600.0f});

    auto* n = tree.get(ctx.sel_node);
    auto* data = reinterpret_cast<widget::SelectData*>(n->user_data);
    data->on_change = [](uint32_t, size_t, void* ud) {
        auto* c = static_cast<RebuildContext*>(ud);
        c->execute_rebuild();
    };
    data->change_user_data = &ctx;

    // Open dropdown
    platform::PointerEvent down1{.action = platform::PointerAction::Down,
                                 .button = platform::PointerButton::Left,
                                 .position = {20.0f, 20.0f}};
    dispatcher.on_pointer(down1);
    EXPECT_TRUE(mgr.has_active_overlays());
    mgr.layout_overlays(tree, {800.0f, 600.0f});

    // Click item 2 - this invokes on_change which destroys the old SelectData and reconstructs tree
    ctx.rebuilt = false;
    platform::PointerEvent down2{.action = platform::PointerAction::Down,
                                 .button = platform::PointerButton::Left,
                                 .position = {20.0f, 80.0f}};
    EXPECT_NO_FATAL_FAILURE(dispatcher.on_pointer(down2));
    EXPECT_TRUE(ctx.rebuilt);
    EXPECT_FALSE(mgr.has_active_overlays());
}



