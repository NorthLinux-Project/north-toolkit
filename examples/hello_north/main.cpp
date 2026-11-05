#include "north/core/tree.hpp"
#include "north/event/input_dispatcher.hpp"
#include "north/gpu/d3d11/d3d11_device.hpp"
#include "north/overlay/overlay_manager.hpp"
#include "north/platform/win32/win32_app.hpp"
#include "north/platform/win32/win32_window.hpp"
#include "north/render/renderer.hpp"
#include "north/render/svg_path.hpp"
#include "north/style/theme.hpp"
#include "north/text/font_manager.hpp"
#include "north/widget/box.hpp"
#include "north/widget/button.hpp"
#include "north/widget/combo_box.hpp"
#include "north/widget/command_palette.hpp"
#include "north/widget/data_table.hpp"
#include "north/widget/icon.hpp"
#include "north/widget/select.hpp"
#include "north/widget/split_view.hpp"
#include "north/widget/text.hpp"
#include "north/widget/tree_view.hpp"

#include <memory>
#include <string>
#include <vector>

using namespace north;

namespace {

// Phosphor-style vector icon paths (clean 24x24 / 256x256 geometric outline aesthetic)
constexpr std::string_view kNorthLogoSvg =
    "M128 24L160 96L232 128L160 160L128 232L96 160L24 128L96 96Z";

constexpr std::string_view kFolderIconSvg =
    "M216 72H130.7L104 45.3A16 16 0 0 0 92.7 40H40A16 16 0 0 0 24 56V200A16 16 0 0 0 40 216H216A16 16 0 0 0 232 200V88A16 16 0 0 0 216 72ZM216 200H40V56H92.7L119.3 82.7A16 16 0 0 0 130.7 88H216V200Z";

constexpr std::string_view kFileIconSvg =
    "M213.7 82.3L157.7 26.3A16 16 0 0 0 146.3 21.7H56A16 16 0 0 0 40 37.7V218.3A16 16 0 0 0 56 234.3H200A16 16 0 0 0 216 218.3V93.7A16 16 0 0 0 213.7 82.3ZM152 46.6L191.4 86H152ZM200 218.3H56V37.7H136V94A8 8 0 0 0 144 102H200Z";

constexpr std::string_view kCodeIconSvg =
    "M69.7 165.7L24 120L69.7 74.3A8 8 0 0 1 81 85.7L46.6 120L81 154.3A8 8 0 0 1 69.7 165.7ZM186.3 74.3A8 8 0 0 0 175 85.7L209.4 120L175 154.3A8 8 0 0 0 186.3 165.7L232 120ZM147.4 52.8A8 8 0 0 0 137.2 58.4L97.2 178.4A8 8 0 0 0 102.8 188.6A8 8 0 0 0 108.6 189A8 8 0 0 0 114.8 183.6L154.8 63.6A8 8 0 0 0 147.4 52.8Z";

constexpr std::string_view kGitBranchIconSvg =
    "M208 144A32 32 0 0 0 178.5 168H144A20 20 0 0 1 124 148V108A32 32 0 1 0 96 108V148A48 48 0 0 0 144 196H178.5A32 32 0 1 0 208 144Z";

struct AppState {
    core::NodeTree tree;
    overlay::OverlayManager overlay_manager;
    platform::win32::Win32Window* window{nullptr};
    gpu::Device* device{nullptr};
    std::unique_ptr<event::InputDispatcher> dispatcher;

    std::shared_ptr<text::Font> ui_font;
    style::Theme active_theme = style::Theme::dark();
    uint32_t active_theme_id{1}; // 1: Dark, 2: Light, 3: Midnight

    core::Handle<core::Node> root_node;
    core::Handle<core::Node> top_bar_node;
    core::Handle<core::Node> search_combo_node;
    core::Handle<core::Node> theme_select_node;
    core::Handle<core::Node> tree_node;
    core::Handle<core::Node> table_node;
    core::Handle<core::Node> split_node;
    core::Handle<core::Node> status_bar_node;

    core::Handle<core::Node> active_palette_node;
    overlay::OverlayId active_palette_overlay_id{overlay::kInvalidOverlayId};

    uint32_t selected_file_id{101};
    std::string selected_file_name{"split_view.hpp"};
    std::string status_info{"Ready"};
    float split_position{270.0f};

    std::vector<widget::TreeNodeItem> build_file_tree() {
        render::Path folder_path;
        (void)render::parse_svg_path(kFolderIconSvg, folder_path);
        render::Path file_path;
        (void)render::parse_svg_path(kFileIconSvg, file_path);
        render::Path code_path;
        (void)render::parse_svg_path(kCodeIconSvg, code_path);

        widget::TreeNodeItem src;
        src.id = 1;
        src.label = "src";
        src.expandable = true;
        src.expanded = true;
        src.icon_path = folder_path;

        widget::TreeNodeItem widget_dir;
        widget_dir.id = 10;
        widget_dir.label = "widget";
        widget_dir.expandable = true;
        widget_dir.expanded = true;
        widget_dir.icon_path = folder_path;

        widget_dir.children.push_back(widget::TreeNodeItem{101, "split_view.hpp", code_path, false, false, nullptr, {}});
        widget_dir.children.push_back(widget::TreeNodeItem{102, "split_view.cpp", code_path, false, false, nullptr, {}});
        widget_dir.children.push_back(widget::TreeNodeItem{103, "tree_view.hpp", code_path, false, false, nullptr, {}});
        widget_dir.children.push_back(widget::TreeNodeItem{104, "tree_view.cpp", code_path, false, false, nullptr, {}});
        widget_dir.children.push_back(widget::TreeNodeItem{105, "data_table.hpp", code_path, false, false, nullptr, {}});
        widget_dir.children.push_back(widget::TreeNodeItem{106, "data_table.cpp", code_path, false, false, nullptr, {}});
        widget_dir.children.push_back(widget::TreeNodeItem{107, "tabs.hpp", code_path, false, false, nullptr, {}});
        widget_dir.children.push_back(widget::TreeNodeItem{108, "text_area.hpp", code_path, false, false, nullptr, {}});
        widget_dir.children.push_back(widget::TreeNodeItem{109, "command_palette.hpp", code_path, false, false, nullptr, {}});
        widget_dir.children.push_back(widget::TreeNodeItem{110, "command_palette.cpp", code_path, false, false, nullptr, {}});
        widget_dir.children.push_back(widget::TreeNodeItem{111, "select.hpp", code_path, false, false, nullptr, {}});
        widget_dir.children.push_back(widget::TreeNodeItem{112, "combo_box.hpp", code_path, false, false, nullptr, {}});
        widget_dir.children.push_back(widget::TreeNodeItem{113, "fuzzy_matcher.hpp", code_path, false, false, nullptr, {}});

        widget::TreeNodeItem core_dir;
        core_dir.id = 20;
        core_dir.label = "core";
        core_dir.expandable = true;
        core_dir.expanded = false;
        core_dir.icon_path = folder_path;
        core_dir.children.push_back(widget::TreeNodeItem{201, "tree.hpp", code_path, false, false, nullptr, {}});
        core_dir.children.push_back(widget::TreeNodeItem{202, "tree.cpp", code_path, false, false, nullptr, {}});
        core_dir.children.push_back(widget::TreeNodeItem{203, "arena.hpp", code_path, false, false, nullptr, {}});
        core_dir.children.push_back(widget::TreeNodeItem{204, "slab_pool.hpp", code_path, false, false, nullptr, {}});

        src.children.push_back(std::move(widget_dir));
        src.children.push_back(std::move(core_dir));

        widget::TreeNodeItem tests;
        tests.id = 2;
        tests.label = "tests";
        tests.expandable = true;
        tests.expanded = true;
        tests.icon_path = folder_path;
        tests.children.push_back(widget::TreeNodeItem{301, "test_command_palette_and_pickers.cpp", code_path, false, false, nullptr, {}});
        tests.children.push_back(widget::TreeNodeItem{302, "test_split_tree_table.cpp", code_path, false, false, nullptr, {}});
        tests.children.push_back(widget::TreeNodeItem{303, "test_tabs_and_navigation.cpp", code_path, false, false, nullptr, {}});
        tests.children.push_back(widget::TreeNodeItem{304, "test_text_editor.cpp", code_path, false, false, nullptr, {}});

        widget::TreeNodeItem cmake;
        cmake.id = 401;
        cmake.label = "CMakeLists.txt";
        cmake.expandable = false;
        cmake.icon_path = file_path;

        widget::TreeNodeItem readme;
        readme.id = 402;
        readme.label = "README.md";
        readme.expandable = false;
        readme.icon_path = file_path;

        return {std::move(src), std::move(tests), std::move(cmake), std::move(readme)};
    }

    std::vector<widget::TableRow> build_table_rows() {
        return {
            widget::TableRow{101, {"split_view.hpp", "Widget Header", "4.2 KB", "100%", "Passing"}},
            widget::TableRow{102, {"split_view.cpp", "Widget Source", "9.6 KB", "100%", "Passing"}},
            widget::TableRow{103, {"tree_view.hpp", "Widget Header", "3.8 KB", "100%", "Passing"}},
            widget::TableRow{104, {"tree_view.cpp", "Widget Source", "11.2 KB", "100%", "Passing"}},
            widget::TableRow{105, {"data_table.hpp", "Widget Header", "4.5 KB", "100%", "Passing"}},
            widget::TableRow{106, {"data_table.cpp", "Widget Source", "14.8 KB", "100%", "Passing"}},
            widget::TableRow{107, {"tabs.hpp", "Widget Header", "3.4 KB", "100%", "Passing"}},
            widget::TableRow{108, {"text_area.hpp", "Widget Header", "3.2 KB", "100%", "Passing"}},
            widget::TableRow{109, {"command_palette.hpp", "Widget Header", "4.8 KB", "100%", "Passing"}},
            widget::TableRow{110, {"command_palette.cpp", "Widget Source", "17.6 KB", "100%", "Passing"}},
            widget::TableRow{111, {"select.hpp", "Widget Header", "3.1 KB", "100%", "Passing"}},
            widget::TableRow{112, {"combo_box.hpp", "Widget Header", "3.3 KB", "100%", "Passing"}},
            widget::TableRow{113, {"fuzzy_matcher.hpp", "Text Header", "2.4 KB", "100%", "Passing"}},
            widget::TableRow{201, {"tree.hpp", "Core Header", "5.1 KB", "100%", "Passing"}},
            widget::TableRow{202, {"tree.cpp", "Core Source", "7.8 KB", "100%", "Passing"}},
            widget::TableRow{301, {"test_command_palette_and_pickers.cpp", "Unit Test", "9.1 KB", "100%", "Passing"}},
            widget::TableRow{302, {"test_split_tree_table.cpp", "Unit Test", "10.4 KB", "100%", "Passing"}},
        };
    }

    void switch_theme(uint32_t id) {
        if (active_theme_id == id) {
            return;
        }
        active_theme_id = id;
        if (id == 1) {
            active_theme = style::Theme::dark();
            status_info = "Theme: Dark Mode";
        } else if (id == 2) {
            active_theme = style::Theme::light();
            status_info = "Theme: Light Mode";
        } else if (id == 3) {
            active_theme = style::Theme::from_hct(225.0f, 28.0f, true);
            status_info = "Theme: Midnight Charcoal";
        }
        build_tree();
    }

    void select_file_by_id(uint32_t id) {
        selected_file_id = id;
        for (const auto& r : build_table_rows()) {
            if (r.id == id) {
                selected_file_name = r.cells[0];
                break;
            }
        }
        status_info = "Selected " + selected_file_name;
        build_tree();
    }

    void toggle_command_palette(std::string_view initial_query = "") {
        if (active_palette_overlay_id != overlay::kInvalidOverlayId) {
            overlay_manager.hide_overlay(active_palette_overlay_id);
            active_palette_overlay_id = overlay::kInvalidOverlayId;
            return;
        }

        if (active_palette_node.is_valid()) {
            tree.destroy_node(active_palette_node);
            active_palette_node = {};
        }

        render::Path code_path;
        (void)render::parse_svg_path(kCodeIconSvg, code_path);

        auto cp_builder = widget::CommandPaletteBuilder(tree);
        cp_builder.overlay_manager(overlay_manager)
            .width(580.0f)
            .max_height(400.0f)
            .font(ui_font)
            .font_size(13.0f)
            .theme(active_theme)
            .on_dismiss([](void* ud) {
                auto* self = static_cast<AppState*>(ud);
                self->active_palette_overlay_id = overlay::kInvalidOverlayId;
            }, this)
            .on_action([](uint32_t id, void* ud) {
                auto* self = static_cast<AppState*>(ud);
                self->handle_palette_action(id);
            }, this);

        // Populate categorized palette items
        cp_builder
            .item(1, "Toggle Dark / Light Theme", "Theme", "Ctrl+T")
            .item(2, "Set Theme: Dark Mode", "Theme")
            .item(3, "Set Theme: Light Mode", "Theme")
            .item(4, "Set Theme: Midnight Charcoal", "Theme")
            .item(10, "Focus File Explorer", "View", "Ctrl+1")
            .item(11, "Focus Data Table", "View", "Ctrl+2")
            .item(12, "Reset Split View Divider", "View")
            .item(101, "split_view.hpp", "Files", "#", code_path)
            .item(102, "split_view.cpp", "Files", "#", code_path)
            .item(103, "tree_view.hpp", "Files", "#", code_path)
            .item(104, "tree_view.cpp", "Files", "#", code_path)
            .item(105, "data_table.hpp", "Files", "#", code_path)
            .item(106, "data_table.cpp", "Files", "#", code_path)
            .item(107, "tabs.hpp", "Files", "#", code_path)
            .item(108, "text_area.hpp", "Files", "#", code_path)
            .item(109, "command_palette.hpp", "Files", "#", code_path)
            .item(110, "command_palette.cpp", "Files", "#", code_path)
            .item(111, "select.hpp", "Files", "#", code_path)
            .item(112, "combo_box.hpp", "Files", "#", code_path)
            .item(113, "fuzzy_matcher.hpp", "Files", "#", code_path)
            .item(20, "Git: Checkout main", "Git")
            .item(21, "Git: Refresh Working Tree", "Git")
            .item(22, "Git: Stash Workspace", "Git");

        active_palette_node = cp_builder.build();

        if (!initial_query.empty()) {
            auto* node = tree.get(active_palette_node);
            if (node && node->user_data) {
                auto* cp_data = reinterpret_cast<widget::CommandPaletteData*>(node->user_data);
                cp_data->set_query(initial_query);
            }
        }

        overlay::OverlayConfig cfg;
        cfg.placement = overlay::Placement::Bottom;
        cfg.offset = 64.0f;
        cfg.modal = true;
        cfg.barrier_dismissible = true;
        cfg.barrier_color = core::Color{0.0f, 0.0f, 0.0f, 0.45f};
        cfg.on_dismiss = [](void* ud) {
            auto* self = static_cast<AppState*>(ud);
            self->active_palette_overlay_id = overlay::kInvalidOverlayId;
        };
        cfg.dismiss_user_data = this;

        core::Size vsz = window ? window->size() : core::Size{1100.0f, 720.0f};
        core::Rect top_center{vsz.width * 0.5f, 0.0f, 0.0f, 0.0f};
        active_palette_overlay_id = overlay_manager.show_overlay_at_rect(active_palette_node, top_center, cfg);

        auto* node = tree.get(active_palette_node);
        if (node && node->user_data) {
            auto* data = reinterpret_cast<widget::CommandPaletteData*>(node->user_data);
            data->active_overlay_id = active_palette_overlay_id;
        }

        if (dispatcher) {
            dispatcher->set_focus(active_palette_node);
        }
    }

    void handle_palette_action(uint32_t id) {
        if (id == 1) {
            switch_theme(active_theme_id == 1 ? 2 : 1);
        } else if (id == 2) {
            switch_theme(1);
        } else if (id == 3) {
            switch_theme(2);
        } else if (id == 4) {
            switch_theme(3);
        } else if (id == 10) {
            if (dispatcher && tree_node.is_valid()) {
                dispatcher->set_focus(tree_node);
                status_info = "Focused File Explorer";
            }
        } else if (id == 11) {
            if (dispatcher && table_node.is_valid()) {
                dispatcher->set_focus(table_node);
                status_info = "Focused Data Table";
            }
        } else if (id == 12) {
            split_position = 270.0f;
            status_info = "Reset Split View Divider";
            build_tree();
        } else if (id >= 101 && id <= 200) {
            select_file_by_id(id);
        } else if (id == 20) {
            status_info = "Git: On branch main (clean)";
        } else if (id == 21) {
            status_info = "Git: Working tree refreshed";
        } else if (id == 22) {
            status_info = "Git: Workspace stashed";
        }
    }

    void register_accelerators() {
        if (!dispatcher) return;
        dispatcher->clear_accelerators();

        // Ctrl+Shift+P -> Command Palette
        dispatcher->register_accelerator(
            event::KeyShortcut{.key_code = 'P', .modifiers = platform::kModControl | platform::kModShift},
            [](void* ud) {
                static_cast<AppState*>(ud)->toggle_command_palette("");
            },
            this);

        // Ctrl+P -> File Quick Open (#)
        dispatcher->register_accelerator(
            event::KeyShortcut{.key_code = 'P', .modifiers = platform::kModControl},
            [](void* ud) {
                static_cast<AppState*>(ud)->toggle_command_palette("#");
            },
            this);

        // Ctrl+T -> Toggle Theme
        dispatcher->register_accelerator(
            event::KeyShortcut{.key_code = 'T', .modifiers = platform::kModControl},
            [](void* ud) {
                auto* self = static_cast<AppState*>(ud);
                self->switch_theme(self->active_theme_id == 1 ? 2 : 1);
            },
            this);

        // Ctrl+1 -> Focus Tree
        dispatcher->register_accelerator(
            event::KeyShortcut{.key_code = '1', .modifiers = platform::kModControl},
            [](void* ud) {
                auto* self = static_cast<AppState*>(ud);
                if (self->tree_node.is_valid()) {
                    self->dispatcher->set_focus(self->tree_node);
                    self->status_info = "Focused File Explorer";
                }
            },
            this);

        // Ctrl+2 -> Focus Table
        dispatcher->register_accelerator(
            event::KeyShortcut{.key_code = '2', .modifiers = platform::kModControl},
            [](void* ud) {
                auto* self = static_cast<AppState*>(ud);
                if (self->table_node.is_valid()) {
                    self->dispatcher->set_focus(self->table_node);
                    self->status_info = "Focused Data Table";
                }
            },
            this);
    }

    core::Handle<core::Node> build_top_bar() {
        const auto& c = active_theme.colors();

        auto logo = widget::IconBuilder(tree, kNorthLogoSvg)
                        .size(16.0f, 16.0f)
                        .color(c.primary)
                        .build();

        auto brand_text = widget::TextBuilder(tree, "North Workstation")
                              .font(ui_font)
                              .font_size(13.5f)
                              .color(c.on_surface)
                              .build();

        auto brand_box = widget::BoxBuilder(tree)
                             .flex(layout::FlexDirection::Row, layout::MainAxisAlignment::Start,
                                   layout::CrossAxisAlignment::Center, 8.0f)
                             .child(logo)
                             .child(brand_text)
                             .build();

        search_combo_node = widget::ComboBoxBuilder(tree)
                                .width(230.0f)
                                .height(30.0f)
                                .placeholder("Filter files or symbols...")
                                .font(ui_font)
                                .font_size(12.0f)
                                .theme(active_theme)
                                .overlay_manager(overlay_manager)
                                .item(101, "split_view.hpp", "Widget Header")
                                .item(102, "split_view.cpp", "Widget Source")
                                .item(103, "tree_view.hpp", "Widget Header")
                                .item(104, "tree_view.cpp", "Widget Source")
                                .item(105, "data_table.hpp", "Widget Header")
                                .item(106, "data_table.cpp", "Widget Source")
                                .item(107, "tabs.hpp", "Widget Header")
                                .item(108, "text_area.hpp", "Widget Header")
                                .item(109, "command_palette.hpp", "Widget Header")
                                .item(110, "command_palette.cpp", "Widget Source")
                                .item(111, "select.hpp", "Widget Header")
                                .item(112, "combo_box.hpp", "Widget Header")
                                .item(113, "fuzzy_matcher.hpp", "Text Header")
                                .on_select([](uint32_t id, std::string_view, void* ud) {
                                    auto* self = static_cast<AppState*>(ud);
                                    self->select_file_by_id(id);
                                }, this)
                                .build();

        auto cmd_btn = widget::ButtonBuilder(tree, "Commands (Ctrl+Shift+P)")
                           .font(ui_font)
                           .font_size(12.0f)
                           .padding(core::Edges{4.0f, 12.0f, 4.0f, 12.0f})
                           .corner_radii(core::CornerRadii::uniform(6.0f))
                           .theme(active_theme)
                           .on_click([](void* ud) {
                               auto* self = static_cast<AppState*>(ud);
                               self->toggle_command_palette("");
                           }, this)
                           .build();

        auto center_box = widget::BoxBuilder(tree)
                              .flex(layout::FlexDirection::Row, layout::MainAxisAlignment::Center,
                                    layout::CrossAxisAlignment::Center, 10.0f)
                              .child(search_combo_node)
                              .child(cmd_btn)
                              .build();

        theme_select_node = widget::SelectBuilder(tree)
                                .width(135.0f)
                                .height(30.0f)
                                .item(1, "Dark Theme")
                                .item(2, "Light Theme")
                                .item(3, "Midnight")
                                .selected_id(active_theme_id)
                                .font(ui_font)
                                .font_size(12.0f)
                                .theme(active_theme)
                                .overlay_manager(overlay_manager)
                                .on_change([](uint32_t id, size_t, void* ud) {
                                    auto* self = static_cast<AppState*>(ud);
                                    self->switch_theme(id);
                                }, this)
                                .build();

        auto right_box = widget::BoxBuilder(tree)
                             .flex(layout::FlexDirection::Row, layout::MainAxisAlignment::End,
                                   layout::CrossAxisAlignment::Center, 10.0f)
                             .child(theme_select_node)
                             .build();

        return widget::BoxBuilder(tree)
            .flex(layout::FlexDirection::Row, layout::MainAxisAlignment::SpaceBetween,
                  layout::CrossAxisAlignment::Center)
            .height(44.0f)
            .background(c.surface)
            .border(c.outline_variant, core::Edges{0.0f, 0.0f, 1.0f, 0.0f})
            .padding(core::Edges{0.0f, 16.0f, 0.0f, 16.0f})
            .child(brand_box)
            .child(center_box)
            .child(right_box)
            .build();
    }

    core::Handle<core::Node> build_workspace_split() {
        tree_node = widget::TreeViewBuilder(tree)
                        .roots(build_file_tree())
                        .selected_id(selected_file_id)
                        .font(ui_font)
                        .font_size(13.0f)
                        .row_height(28.0f)
                        .indent_width(18.0f)
                        .indent_guides(true)
                        .theme(active_theme)
                        .on_select([](uint32_t id, void* ud) {
                            auto* self = static_cast<AppState*>(ud);
                            self->select_file_by_id(id);
                        }, this)
                        .build();

        table_node = widget::DataTableBuilder(tree)
                         .column("Name", 180.0f, text::TextAlign::Left, true, true)
                         .column("Type", 140.0f, text::TextAlign::Left, true, true)
                         .column("Size", 90.0f, text::TextAlign::Right, true, true)
                         .column("Coverage", 100.0f, text::TextAlign::Right, true, true)
                         .flex_column("Test Status", 1.0f, 110.0f, text::TextAlign::Left, true)
                         .rows(build_table_rows())
                         .header_height(34.0f)
                         .row_height(28.0f)
                         .alternating_rows(true)
                         .font(ui_font)
                         .font_size(13.0f)
                         .theme(active_theme)
                         .build();

        auto split = widget::SplitViewBuilder(tree)
                         .orientation(widget::SplitOrientation::Horizontal)
                         .sizing_mode(widget::SplitSizingMode::FixedFirst)
                         .split(split_position)
                         .min_first_size(160.0f)
                         .max_first_size(500.0f)
                         .min_second_size(300.0f)
                         .sash_thickness(6.0f)
                         .first(tree_node)
                         .second(table_node)
                         .theme(active_theme)
                         .on_change([](float split_val, void* ud) {
                             auto* self = static_cast<AppState*>(ud);
                             self->split_position = split_val;
                         }, this)
                         .build();

        auto* sn = tree.get(split);
        if (sn) {
            sn->flex_item.grow = 1.0f;
            sn->flex_item.shrink = 1.0f;
        }
        return split;
    }

    core::Handle<core::Node> build_status_bar() {
        const auto& c = active_theme.colors();

        auto branch_icon = widget::IconBuilder(tree, kGitBranchIconSvg)
                               .size(13.0f, 13.0f)
                               .color(c.on_surface_variant)
                               .build();

        auto branch_text = widget::TextBuilder(tree, "main")
                               .font(ui_font)
                               .font_size(11.5f)
                               .color(c.on_surface_variant)
                               .build();

        std::string file_info = selected_file_name + "  •  " + status_info;
        auto file_text = widget::TextBuilder(tree, file_info)
                             .font(ui_font)
                             .font_size(11.5f)
                             .color(c.on_surface_variant)
                             .build();

        auto status_left = widget::BoxBuilder(tree)
                               .flex(layout::FlexDirection::Row, layout::MainAxisAlignment::Start,
                                     layout::CrossAxisAlignment::Center, 8.0f)
                               .child(branch_icon)
                               .child(branch_text)
                               .child(file_text)
                               .build();

        auto shortcut_hint = widget::TextBuilder(tree, "UTF-8   C++20   Ctrl+Shift+P for Palette")
                                 .font(ui_font)
                                 .font_size(11.5f)
                                 .color(c.on_surface_variant)
                                 .build();

        return widget::BoxBuilder(tree)
            .flex(layout::FlexDirection::Row, layout::MainAxisAlignment::SpaceBetween,
                  layout::CrossAxisAlignment::Center)
            .height(26.0f)
            .background(c.surface)
            .border(c.outline_variant, core::Edges{1.0f, 0.0f, 0.0f, 0.0f})
            .padding(core::Edges{0.0f, 12.0f, 0.0f, 12.0f})
            .child(status_left)
            .child(shortcut_hint)
            .build();
    }

    void build_tree() {
        if (active_palette_overlay_id != overlay::kInvalidOverlayId) {
            overlay_manager.hide_overlay(active_palette_overlay_id);
            active_palette_overlay_id = overlay::kInvalidOverlayId;
            active_palette_node = {};
        }

        if (tree.root().is_valid()) {
            tree.destroy_node(tree.root());
        }

        ui_font = text::FontManager::instance().load_system_font("Segoe UI");
        if (!ui_font) {
            ui_font = text::FontManager::instance().load_system_font();
        }

        const auto& c = active_theme.colors();

        top_bar_node = build_top_bar();
        split_node = build_workspace_split();
        status_bar_node = build_status_bar();

        root_node = widget::BoxBuilder(tree)
                        .flex(layout::FlexDirection::Column, layout::MainAxisAlignment::Start,
                              layout::CrossAxisAlignment::Stretch, 0.0f)
                        .background(c.background)
                        .padding(core::Edges::uniform(0.0f))
                        .margin(core::Edges::uniform(0.0f))
                        .child(top_bar_node)
                        .child(split_node)
                        .child(status_bar_node)
                        .build();

        tree.set_root(root_node);

        if (dispatcher) {
            dispatcher->reset();
            dispatcher->set_overlay_manager(&overlay_manager);
            register_accelerators();
            dispatcher->set_focus(tree_node);
        }

        if (window && window->size().width > 0.0f && window->size().height > 0.0f) {
            tree.layout(layout::BoxConstraints::tight(window->size()));
        }
    }
};

} // namespace

int main() {
    platform::win32::Win32Window window;
    gpu::D3D11Device device;

    platform::win32::WindowDesc desc{.title = "North Toolkit - Workstation IDE",
                                     .width = 1100,
                                     .height = 720,
                                     .resizable = true,
                                     .min_width = 700,
                                     .min_height = 450};

    if (!window.create(desc, &device)) {
        return 1;
    }

    render::Renderer renderer(device);
    if (!renderer.init()) {
        return 1;
    }

    AppState state;
    state.window = &window;
    state.device = &device;
    state.dispatcher = std::make_unique<event::InputDispatcher>(state.tree);
    state.dispatcher->set_window(&window);
    window.set_listener(state.dispatcher.get());

    state.build_tree();

    auto& app = platform::win32::Win32App::instance();
    app.register_window(&window);

    struct RunContext {
        render::Renderer* renderer;
        gpu::D3D11Device* device;
        AppState* state;
    };
    static RunContext ctx{.renderer = &renderer,
                          .device = &device,
                          .state = &state};

    app.set_idle_callback([]() {
        if (!ctx.state || !ctx.state->window || !ctx.state->window->is_open()) {
            return;
        }

        core::Size sz = ctx.state->window->size();
        if (sz.width <= 0.0f || sz.height <= 0.0f) {
            return;
        }

        ctx.state->tree.layout(layout::BoxConstraints::tight(sz));
        ctx.renderer->render_frame(ctx.state->tree, sz,
                                   ctx.state->active_theme.colors().background,
                                   &ctx.state->overlay_manager);
        ctx.device->present();
    });

    window.show(true);
    app.run();

    return 0;
}
