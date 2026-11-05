#include "north/widget/accordion.hpp"

#include "north/render/display_list.hpp"
#include "north/text/font_manager.hpp"
#include "north/text/paragraph.hpp"

#include <algorithm>
#include <cmath>

namespace north::widget {

namespace {

constexpr float kPi = 3.14159265358979323846f;

void paint_rotated_chevron(render::DisplayList& dl, core::Point center, float progress,
                           core::Color color) {
    float angle = progress * (kPi * 0.5f);
    float cos_a = std::cos(angle);
    float sin_a = std::sin(angle);

    auto rot = [&](float x, float y) -> core::Point {
        return core::Point{center.x + (x * cos_a - y * sin_a),
                           center.y + (x * sin_a + y * cos_a)};
    };

    render::Path chev;
    chev.move_to(rot(-2.0f, -4.5f));
    chev.line_to(rot(2.5f, 0.0f));
    chev.line_to(rot(-2.0f, 4.5f));

    render::StrokeStyle stroke{
        .width = 1.6f, .cap = render::LineCap::Round, .join = render::LineJoin::Round};
    dl.push_path_stroke(chev, color, stroke);
}

core::Size accordion_layout_fn(core::NodeTree& tree, core::Handle<core::Node> handle,
                               const layout::BoxConstraints& constraints, void* user_data) {
    auto* data = reinterpret_cast<AccordionData*>(user_data);
    auto* node = tree.get(handle);
    if (!data || !node) {
        return constraints.constrain({0.0f, 0.0f});
    }

    float desired_w = (data->width > 0.0f) ? data->width
                                           : ((constraints.max_width < 100000.0f)
                                                  ? constraints.max_width
                                                  : 320.0f);
    float resolved_w = std::clamp(desired_w, constraints.min_width, constraints.max_width);
    float inner_w = (std::max)(0.0f, resolved_w - data->content_padding.horizontal());

    float cur_y = 0.0f;
    for (size_t i = 0; i < data->items.size(); ++i) {
        auto& item = data->items[i];
        if (i < data->item_animations.size() && data->item_animations[i]) {
            item.expand_progress = data->item_animations[i]->get();
        }

        item.header_y = cur_y;
        cur_y += data->header_height;
        item.content_y = cur_y;

        if (item.expand_progress > 0.001f && item.content_node.is_valid()) {
            auto* ch_node = tree.get(item.content_node);
            if (ch_node) {
                layout::BoxConstraints ch_constraints{inner_w, inner_w, 0.0f, 100000.0f};
                core::Size ch_sz = tree.layout_subtree(item.content_node, ch_constraints);
                item.natural_content_height = ch_sz.height + data->content_padding.vertical();
                item.visible_content_height = item.natural_content_height * item.expand_progress;
                ch_node->layout.offset = {data->content_padding.left,
                                          item.content_y + data->content_padding.top};
                cur_y += item.visible_content_height;
            }
        } else {
            item.visible_content_height = 0.0f;
            if (item.content_node.is_valid()) {
                if (auto* ch_node = tree.get(item.content_node)) {
                    ch_node->layout.size = {0.0f, 0.0f};
                    ch_node->layout.offset = {data->content_padding.left, item.content_y};
                }
            }
        }
    }

    return constraints.constrain({resolved_w, cur_y});
}

void accordion_paint_fn(core::NodeTree& tree, core::Handle<core::Node> handle, core::Point origin,
                        void* stream, void* user_data) {
    auto* dl = static_cast<render::DisplayList*>(stream);
    auto* data = reinterpret_cast<AccordionData*>(user_data);
    auto* node = tree.get(handle);
    if (!dl || !data || !node || data->items.empty()) {
        return;
    }

    core::Rect bounds{origin.x, origin.y, node->layout.size.width, node->layout.size.height};
    dl->push_quad(bounds, data->corner_radii, data->background_color, data->border_color,
                  core::Edges::uniform(1.0f));

    dl->push_clip(bounds);

    for (size_t i = 0; i < data->items.size(); ++i) {
        const auto& item = data->items[i];
        core::Rect header_rect{origin.x + 1.0f, origin.y + item.header_y,
                               (std::max)(0.0f, bounds.width - 2.0f), data->header_height};

        core::Color h_bg = (i == data->hovered_header) ? data->header_hover_bg : data->header_bg;
        dl->push_quad(header_rect, core::CornerRadii::uniform(0.0f), h_bg,
                      core::Color{0.0f, 0.0f, 0.0f, 0.0f}, core::Edges::uniform(0.0f));

        core::Point chev_center{origin.x + data->header_padding.left + 6.0f,
                                origin.y + item.header_y + data->header_height * 0.5f};
        paint_rotated_chevron(*dl, chev_center, item.expand_progress, data->chevron_color);

        if (data->font && !item.title.empty()) {
            float line_h = data->font->line_height(data->font_size);
            float text_x = origin.x + data->header_padding.left + 20.0f;
            float text_y =
                origin.y + item.header_y + std::floor((data->header_height - line_h) * 0.5f);
            float max_w =
                (std::max)(10.0f, bounds.width - data->header_padding.horizontal() - 24.0f);

            text::ParagraphBuilder pb(text::TextAlign::Left);
            pb.add_text(item.title, text::TextStyle{.font = data->font,
                                                    .font_size = data->font_size,
                                                    .color = data->text_color});
            auto para = pb.build();
            para->layout(layout::BoxConstraints::loose({max_w, line_h}));
            if (dl->glyph_atlas()) {
                para->paint(*dl, *dl->glyph_atlas(), {text_x, text_y}, data->text_color);
            }
        }

        if (item.visible_content_height > 0.5f && item.content_node.is_valid()) {
            core::Rect div_rect{origin.x + 1.0f, origin.y + item.content_y,
                                (std::max)(0.0f, bounds.width - 2.0f), 1.0f};
            dl->push_quad(div_rect, core::CornerRadii::uniform(0.0f), data->border_color,
                          core::Color{0.0f, 0.0f, 0.0f, 0.0f}, core::Edges::uniform(0.0f));

            core::Rect content_clip{origin.x + 1.0f, origin.y + item.content_y,
                                    (std::max)(0.0f, bounds.width - 2.0f),
                                    item.visible_content_height};
            dl->push_clip(content_clip);
            tree.paint_subtree(item.content_node, origin, stream);
            dl->pop_clip();
        }

        if (i + 1 < data->items.size()) {
            float sep_y = origin.y + item.content_y + item.visible_content_height - 1.0f;
            core::Rect sep_rect{origin.x + 1.0f, sep_y, (std::max)(0.0f, bounds.width - 2.0f),
                                1.0f};
            dl->push_quad(sep_rect, core::CornerRadii::uniform(0.0f), data->border_color,
                          core::Color{0.0f, 0.0f, 0.0f, 0.0f}, core::Edges::uniform(0.0f));
        }
    }

    dl->pop_clip();
}

void accordion_destroy_fn(void* user_data) {
    delete reinterpret_cast<AccordionData*>(user_data);
}

} // namespace

void AccordionData::toggle_item(size_t index) {
    if (index >= items.size()) {
        return;
    }
    set_expanded(index, !items[index].expanded, true);
}

void AccordionData::set_expanded(size_t index, bool expand, bool animate) {
    if (index >= items.size()) {
        return;
    }

    if (expand && !allow_multiple) {
        for (size_t i = 0; i < items.size(); ++i) {
            if (i != index && items[i].expanded) {
                items[i].expanded = false;
                if (i < item_animations.size() && item_animations[i]) {
                    if (animate) {
                        item_animations[i]->tween_to(0.0f, 0.18f,
                                                     animation::CubicBezier::ease_out());
                    } else {
                        item_animations[i]->set_immediate(0.0f);
                        items[i].expand_progress = 0.0f;
                    }
                } else {
                    items[i].expand_progress = 0.0f;
                }
                if (on_toggle) {
                    on_toggle(i, false, toggle_user_data);
                }
            }
        }
    }

    bool changed = (items[index].expanded != expand);
    items[index].expanded = expand;
    focused_header = index;

    float target = expand ? 1.0f : 0.0f;
    if (index < item_animations.size() && item_animations[index]) {
        if (animate) {
            item_animations[index]->tween_to(target, 0.18f, animation::CubicBezier::ease_out());
            items[index].expand_progress = target;
        } else {
            item_animations[index]->set_immediate(target);
            items[index].expand_progress = target;
        }
    } else {
        items[index].expand_progress = target;
    }

    if (tree && node_handle.is_valid()) {
        tree->mark_dirty(node_handle, core::kLayout | core::kPaint);
    }

    if (changed && on_toggle) {
        on_toggle(index, expand, toggle_user_data);
    }
}

bool AccordionData::is_expanded(size_t index) const noexcept {
    return index < items.size() && items[index].expanded;
}

void AccordionData::apply_theme(const style::Theme& theme) {
    const auto& c = theme.colors();
    background_color = c.surface;
    header_bg = c.surface_variant;
    header_hover_bg = core::Color{c.on_surface.r, c.on_surface.g, c.on_surface.b, 0.10f};
    border_color = c.outline_variant;
    text_color = c.on_surface;
    chevron_color = c.on_surface_variant;

    if (on_theme) {
        on_theme(theme, *this);
    }
    if (tree && node_handle.is_valid()) {
        tree->mark_dirty(node_handle, core::kPaint);
    }
}

AccordionBuilder::AccordionBuilder(core::NodeTree& tree) : tree_(tree) {}

AccordionBuilder& AccordionBuilder::item(std::string_view title,
                                         core::Handle<core::Node> content_node, bool expanded) {
    if (expanded && !data_.allow_multiple) {
        for (auto& existing : data_.items) {
            existing.expanded = false;
            existing.expand_progress = 0.0f;
        }
    }

    AccordionItem entry;
    entry.title = std::string(title);
    entry.content_node = content_node;
    entry.expanded = expanded;
    entry.expand_progress = expanded ? 1.0f : 0.0f;
    data_.items.push_back(std::move(entry));
    return *this;
}

AccordionBuilder& AccordionBuilder::allow_multiple(bool enabled) {
    data_.allow_multiple = enabled;
    return *this;
}

AccordionBuilder& AccordionBuilder::font(std::shared_ptr<text::Font> font) {
    data_.font = std::move(font);
    return *this;
}

AccordionBuilder& AccordionBuilder::font_size(float size) {
    data_.font_size = size;
    return *this;
}

AccordionBuilder& AccordionBuilder::header_height(float height) {
    data_.header_height = height;
    return *this;
}

AccordionBuilder& AccordionBuilder::width(float width) {
    data_.width = width;
    return *this;
}

AccordionBuilder& AccordionBuilder::corner_radii(core::CornerRadii radii) {
    data_.corner_radii = radii;
    return *this;
}

AccordionBuilder& AccordionBuilder::theme(const style::Theme& theme) {
    data_.apply_theme(theme);
    return *this;
}

AccordionBuilder& AccordionBuilder::theme_signal(core::Signal<style::Theme>* sig,
                                                 AccordionThemeFn fn) {
    data_.theme_signal = sig;
    data_.on_theme = fn;
    if (sig) {
        data_.apply_theme(sig->get());
    }
    return *this;
}

AccordionBuilder& AccordionBuilder::on_toggle(AccordionToggleCallbackFn fn, void* user_data) {
    data_.on_toggle = fn;
    data_.toggle_user_data = user_data;
    return *this;
}

core::Handle<core::Node> AccordionBuilder::build() {
    auto handle = tree_.create_node();
    auto* node = tree_.get(handle);
    if (!node) {
        return {};
    }

    if (!data_.font) {
        data_.font = text::FontManager::instance().load_system_font();
    }

    auto* heap_data = new AccordionData(std::move(data_));
    heap_data->tree = &tree_;
    heap_data->node_handle = handle;

    heap_data->item_animations.reserve(heap_data->items.size());
    for (auto& entry : heap_data->items) {
        auto anim = std::make_unique<animation::AnimatedValue<float>>(entry.expanded ? 1.0f : 0.0f);
        anim->bind_dirty(
            [](void* ctx, uint32_t flags) {
                auto* acc = static_cast<AccordionData*>(ctx);
                if (acc->tree && acc->node_handle.is_valid()) {
                    acc->tree->mark_dirty(acc->node_handle, flags);
                }
            },
            heap_data, core::kLayout | core::kPaint);
        heap_data->item_animations.push_back(std::move(anim));

        if (entry.content_node.is_valid()) {
            tree_.append_child(handle, entry.content_node);
        }
    }

    if (heap_data->theme_signal) {
        heap_data->theme_signal->bind_dirty(
            [](void* ctx, uint32_t) {
                auto* acc = static_cast<AccordionData*>(ctx);
                if (acc->theme_signal) {
                    acc->apply_theme(acc->theme_signal->get());
                }
            },
            heap_data, core::kPaint);
    }

    node->user_data = heap_data;
    node->layout_fn = &accordion_layout_fn;
    node->paint_fn = &accordion_paint_fn;
    node->destroy_fn = &accordion_destroy_fn;
    node->custom_child_paint = true;

    return handle;
}

} // namespace north::widget
