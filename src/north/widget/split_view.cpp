#include "north/widget/split_view.hpp"

#include "north/render/display_list.hpp"

#include <algorithm>

namespace north::widget {

namespace {

core::Size split_view_layout_fn(core::NodeTree& tree, core::Handle<core::Node> handle,
                                const layout::BoxConstraints& constraints, void* user_data) {
    auto* data = reinterpret_cast<SplitViewData*>(user_data);
    auto* node = tree.get(handle);
    if (!data || !node) {
        return constraints.constrain({0.0f, 0.0f});
    }

    float target_w = (data->width > 0.0f) ? data->width : constraints.max_width;
    float target_h = (data->height > 0.0f) ? data->height : constraints.max_height;
    if (target_w >= 100000.0f) {
        target_w = 400.0f;
    }
    if (target_h >= 100000.0f) {
        target_h = 300.0f;
    }

    core::Size resolved = constraints.constrain({target_w, target_h});
    bool is_horiz = (data->orientation == SplitOrientation::Horizontal);
    float total_dim = is_horiz ? resolved.width : resolved.height;
    float cross_dim = is_horiz ? resolved.height : resolved.width;
    float avail_dim = (std::max)(0.0f, total_dim - data->sash_thickness);

    float first_size = 0.0f;
    if (data->sizing_mode == SplitSizingMode::Ratio) {
        first_size = avail_dim * std::clamp(data->split_value, 0.0f, 1.0f);
    } else if (data->sizing_mode == SplitSizingMode::FixedFirst) {
        first_size = data->split_value;
    } else {
        first_size = avail_dim - data->split_value;
    }

    float clamped_first = std::clamp(first_size, data->min_first_size, data->max_first_size);
    float second_size = avail_dim - clamped_first;
    if (second_size < data->min_second_size) {
        second_size = data->min_second_size;
        clamped_first = (std::max)(0.0f, avail_dim - second_size);
    } else if (second_size > data->max_second_size) {
        second_size = data->max_second_size;
        clamped_first = (std::max)(0.0f, avail_dim - second_size);
    }

    first_size = clamped_first;
    second_size = (std::max)(0.0f, avail_dim - first_size);

    if (data->first_pane.is_valid()) {
        auto* first_node = tree.get(data->first_pane);
        if (first_node) {
            layout::BoxConstraints first_c =
                is_horiz ? layout::BoxConstraints::tight({first_size, cross_dim})
                         : layout::BoxConstraints::tight({cross_dim, first_size});
            tree.layout_subtree(data->first_pane, first_c);
            first_node->layout.offset = {0.0f, 0.0f};
        }
    }

    if (is_horiz) {
        data->sash_rect = core::Rect{first_size, 0.0f, data->sash_thickness, cross_dim};
    } else {
        data->sash_rect = core::Rect{0.0f, first_size, cross_dim, data->sash_thickness};
    }

    if (data->second_pane.is_valid()) {
        auto* second_node = tree.get(data->second_pane);
        if (second_node) {
            layout::BoxConstraints second_c =
                is_horiz ? layout::BoxConstraints::tight({second_size, cross_dim})
                         : layout::BoxConstraints::tight({cross_dim, second_size});
            tree.layout_subtree(data->second_pane, second_c);
            if (is_horiz) {
                second_node->layout.offset = {first_size + data->sash_thickness, 0.0f};
            } else {
                second_node->layout.offset = {0.0f, first_size + data->sash_thickness};
            }
        }
    }

    return resolved;
}

void split_view_paint_fn(core::NodeTree& tree, core::Handle<core::Node> handle, core::Point origin,
                         void* stream, void* user_data) {
    auto* dl = static_cast<render::DisplayList*>(stream);
    auto* data = reinterpret_cast<SplitViewData*>(user_data);
    auto* node = tree.get(handle);
    if (!dl || !data || !node) {
        return;
    }

    core::Rect sash_world{origin.x + data->sash_rect.x, origin.y + data->sash_rect.y,
                          data->sash_rect.width, data->sash_rect.height};

    if (data->first_pane.is_valid()) {
        auto* fn = tree.get(data->first_pane);
        if (fn) {
            core::Rect first_clip{origin.x + fn->layout.offset.x, origin.y + fn->layout.offset.y,
                                  fn->layout.size.width, fn->layout.size.height};
            dl->push_clip(first_clip);
            tree.paint_subtree(data->first_pane, origin, stream);
            dl->pop_clip();
        }
    }

    if (data->second_pane.is_valid()) {
        auto* sn = tree.get(data->second_pane);
        if (sn) {
            core::Rect second_clip{origin.x + sn->layout.offset.x, origin.y + sn->layout.offset.y,
                                   sn->layout.size.width, sn->layout.size.height};
            dl->push_clip(second_clip);
            tree.paint_subtree(data->second_pane, origin, stream);
            dl->pop_clip();
        }
    }

    core::Color current_sash_bg =
        data->sash_dragging ? data->sash_drag_bg
                            : (data->sash_hovered ? data->sash_hover_bg : data->sash_bg);

    dl->push_quad(sash_world, core::CornerRadii::uniform(0.0f), current_sash_bg,
                  data->border_color, core::Edges::uniform(0.0f));

    bool is_horiz = (data->orientation == SplitOrientation::Horizontal);
    if (is_horiz) {
        float grip_w = 2.0f;
        float grip_h = (std::min)(32.0f, sash_world.height * 0.4f);
        float gx = sash_world.x + (sash_world.width - grip_w) * 0.5f;
        float gy = sash_world.y + (sash_world.height - grip_h) * 0.5f;
        core::Rect grip_rect{gx, gy, grip_w, grip_h};
        dl->push_quad(grip_rect, core::CornerRadii::uniform(1.0f), data->sash_grip_color,
                      core::Color{0.0f, 0.0f, 0.0f, 0.0f}, core::Edges::uniform(0.0f));
    } else {
        float grip_w = (std::min)(32.0f, sash_world.width * 0.4f);
        float grip_h = 2.0f;
        float gx = sash_world.x + (sash_world.width - grip_w) * 0.5f;
        float gy = sash_world.y + (sash_world.height - grip_h) * 0.5f;
        core::Rect grip_rect{gx, gy, grip_w, grip_h};
        dl->push_quad(grip_rect, core::CornerRadii::uniform(1.0f), data->sash_grip_color,
                      core::Color{0.0f, 0.0f, 0.0f, 0.0f}, core::Edges::uniform(0.0f));
    }
}

void split_view_destroy_fn(void* user_data) {
    delete reinterpret_cast<SplitViewData*>(user_data);
}

} // namespace

void SplitViewData::set_split(float val) {
    split_value = val;
    if (tree && node_handle.is_valid()) {
        tree->mark_dirty(node_handle, core::kLayout | core::kPaint);
    }
    if (on_split_change) {
        on_split_change(split_value, split_user_data);
    }
}

void SplitViewData::reset_split() {
    set_split(initial_split_value);
}

void SplitViewData::apply_theme(const style::Theme& theme) {
    const auto& c = theme.colors();
    sash_bg = c.surface;
    sash_hover_bg = c.surface_variant;
    sash_drag_bg = c.primary;
    sash_grip_color = c.outline;
    border_color = c.outline_variant;

    if (on_theme) {
        on_theme(theme, *this);
    }
    if (tree && node_handle.is_valid()) {
        tree->mark_dirty(node_handle, core::kPaint);
    }
}

SplitViewBuilder::SplitViewBuilder(core::NodeTree& tree) : tree_(tree) {}

SplitViewBuilder& SplitViewBuilder::orientation(SplitOrientation orientation) {
    data_.orientation = orientation;
    return *this;
}

SplitViewBuilder& SplitViewBuilder::sizing_mode(SplitSizingMode mode) {
    data_.sizing_mode = mode;
    return *this;
}

SplitViewBuilder& SplitViewBuilder::split(float value) {
    data_.split_value = value;
    data_.initial_split_value = value;
    return *this;
}

SplitViewBuilder& SplitViewBuilder::min_first_size(float min_size) {
    data_.min_first_size = min_size;
    return *this;
}

SplitViewBuilder& SplitViewBuilder::max_first_size(float max_size) {
    data_.max_first_size = max_size;
    return *this;
}

SplitViewBuilder& SplitViewBuilder::min_second_size(float min_size) {
    data_.min_second_size = min_size;
    return *this;
}

SplitViewBuilder& SplitViewBuilder::max_second_size(float max_size) {
    data_.max_second_size = max_size;
    return *this;
}

SplitViewBuilder& SplitViewBuilder::sash_thickness(float thickness) {
    data_.sash_thickness = thickness;
    return *this;
}

SplitViewBuilder& SplitViewBuilder::first(core::Handle<core::Node> first_pane) {
    data_.first_pane = first_pane;
    return *this;
}

SplitViewBuilder& SplitViewBuilder::second(core::Handle<core::Node> second_pane) {
    data_.second_pane = second_pane;
    return *this;
}

SplitViewBuilder& SplitViewBuilder::width(float width) {
    data_.width = width;
    return *this;
}

SplitViewBuilder& SplitViewBuilder::height(float height) {
    data_.height = height;
    return *this;
}

SplitViewBuilder& SplitViewBuilder::theme(const style::Theme& theme) {
    data_.apply_theme(theme);
    return *this;
}

SplitViewBuilder& SplitViewBuilder::theme_signal(core::Signal<style::Theme>* sig, SplitViewThemeFn fn) {
    data_.theme_signal = sig;
    data_.on_theme = fn;
    if (sig) {
        data_.apply_theme(sig->get());
    }
    return *this;
}

SplitViewBuilder& SplitViewBuilder::on_change(SplitChangeCallbackFn fn, void* user_data) {
    data_.on_split_change = fn;
    data_.split_user_data = user_data;
    return *this;
}

core::Handle<core::Node> SplitViewBuilder::build() {
    auto handle = tree_.create_node();
    auto* node = tree_.get(handle);
    if (!node) {
        return {};
    }

    auto* heap_data = new SplitViewData(std::move(data_));
    heap_data->tree = &tree_;
    heap_data->node_handle = handle;

    if (heap_data->first_pane.is_valid()) {
        tree_.append_child(handle, heap_data->first_pane);
    }
    if (heap_data->second_pane.is_valid()) {
        tree_.append_child(handle, heap_data->second_pane);
    }

    if (heap_data->theme_signal) {
        heap_data->theme_signal->bind_dirty(
            [](void* ctx, uint32_t) {
                auto* sv = static_cast<SplitViewData*>(ctx);
                if (sv && sv->theme_signal) {
                    sv->apply_theme(sv->theme_signal->get());
                }
            },
            heap_data, core::kPaint);
    }

    node->user_data = heap_data;
    node->layout_fn = &split_view_layout_fn;
    node->paint_fn = &split_view_paint_fn;
    node->destroy_fn = &split_view_destroy_fn;
    node->custom_child_paint = true;

    return handle;
}

} // namespace north::widget
