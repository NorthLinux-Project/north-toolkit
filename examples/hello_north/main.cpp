#include "north/core/tree.hpp"
#include "north/event/input_dispatcher.hpp"
#include "north/gpu/d3d11/d3d11_device.hpp"
#include "north/platform/win32/win32_app.hpp"
#include "north/platform/win32/win32_window.hpp"
#include "north/render/renderer.hpp"
#include "north/style/theme.hpp"
#include "north/widget/box.hpp"
#include "north/widget/button.hpp"
#include "north/widget/checkbox.hpp"
#include "north/widget/radio_button.hpp"
#include "north/widget/slider.hpp"
#include "north/widget/switch.hpp"
#include "north/widget/text.hpp"
#include "north/widget/text_input.hpp"
#include "north/widget/virtual_list.hpp"

#include <string>

using namespace north;

struct DemoAppState {
    core::NodeTree tree;
    core::Signal<int> counter{0};
    core::Signal<bool> is_dark{true};
    core::Signal<bool> notifications{true};
    core::Signal<float> slider_val{45.0f};
    core::Signal<int> radio_choice{0};
    std::string input_text{"North UI Toolkit"};
    platform::win32::Win32Window* window{nullptr};
    std::unique_ptr<event::InputDispatcher> dispatcher;

    void rebuild();
};

static DemoAppState* g_demo_state = nullptr;

static void on_decrement(void*) {
    if (g_demo_state) {
        g_demo_state->counter.set(g_demo_state->counter.get() - 1);
    }
}

static void on_increment(void*) {
    if (g_demo_state) {
        g_demo_state->counter.set(g_demo_state->counter.get() + 1);
    }
}

static void on_toggle_theme(void*) {
    if (g_demo_state) {
        g_demo_state->is_dark.set(!g_demo_state->is_dark.get());
    }
}

void DemoAppState::rebuild() {
    if (tree.root().is_valid()) {
        tree.destroy_node(tree.root());
    }

    const auto& theme = is_dark.get() ? style::Theme::dark() : style::Theme::light();
    const auto& c = theme.colors();

    std::string count_str = "Count: " + std::to_string(counter.get());
    auto count_text =
        widget::TextBuilder(tree, count_str).font_size(28.0f).color(c.on_surface).build();

    auto dec_btn = widget::ButtonBuilder(tree, "-")
                       .theme(theme)
                       .padding(core::Edges{8.0f, 18.0f, 8.0f, 18.0f})
                       .on_click(&on_decrement)
                       .build();

    auto inc_btn = widget::ButtonBuilder(tree, "+")
                       .theme(theme)
                       .padding(core::Edges{8.0f, 18.0f, 8.0f, 18.0f})
                       .on_click(&on_increment)
                       .build();

    auto theme_switch =
        widget::SwitchBuilder(tree).label("Dark Theme").theme(theme).bind(is_dark).build();

    auto button_row = widget::BoxBuilder(tree)
                          .flex(layout::FlexDirection::Row, layout::MainAxisAlignment::Center,
                                layout::CrossAxisAlignment::Center, 10.0f)
                          .child(dec_btn)
                          .child(inc_btn)
                          .child(theme_switch)
                          .build();

    auto checkbox = widget::CheckboxBuilder(tree)
                        .label("Notifications")
                        .theme(theme)
                        .bind(notifications)
                        .build();

    auto radio_a = widget::RadioButtonBuilder(tree)
                       .label("Mode A")
                       .value(0)
                       .theme(theme)
                       .group(radio_choice)
                       .build();

    auto radio_b = widget::RadioButtonBuilder(tree)
                       .label("Mode B")
                       .value(1)
                       .theme(theme)
                       .group(radio_choice)
                       .build();

    auto options_row = widget::BoxBuilder(tree)
                           .flex(layout::FlexDirection::Row, layout::MainAxisAlignment::Center,
                                 layout::CrossAxisAlignment::Center, 14.0f)
                           .child(checkbox)
                           .child(radio_a)
                           .child(radio_b)
                           .build();

    auto slider = widget::SliderBuilder(tree)
                      .theme(theme)
                      .range(0.0f, 100.0f)
                      .bind(slider_val)
                      .width(240.0f)
                      .build();

    auto text_input = widget::TextInputBuilder(tree)
                          .theme(theme)
                          .text(input_text)
                          .placeholder("Type here...")
                          .width(240.0f)
                          .height(32.0f)
                          .on_change(
                              [](std::string_view text, void* ud) {
                                  auto* s = static_cast<DemoAppState*>(ud);
                                  s->input_text = std::string(text);
                              },
                              this)
                          .build();

    auto vlist = widget::VirtualListBuilder(tree)
                     .theme(theme)
                     .smooth_scroll(true)
                     .width(240.0f)
                     .height(100.0f)
                     .item_count(10000)
                     .item_extent(26.0f)
                     .item_builder(
                         [](core::NodeTree& t, size_t idx, void* ud) -> core::Handle<core::Node> {
                             auto* s = static_cast<DemoAppState*>(ud);
                             const auto& th =
                                 s->is_dark.get() ? style::Theme::dark() : style::Theme::light();
                             std::string label = "Item #" + std::to_string(idx + 1);
                             return widget::BoxBuilder(t)
                                 .flex(layout::FlexDirection::Row, layout::MainAxisAlignment::Start,
                                       layout::CrossAxisAlignment::Center)
                                 .padding(core::Edges{3.0f, 8.0f, 3.0f, 8.0f})
                                 .child(widget::TextBuilder(t, label)
                                            .font_size(12.0f)
                                            .color(th.colors().on_surface_variant)
                                            .build())
                                 .build();
                         },
                         this)
                     .build();

    auto card =
        widget::BoxBuilder(tree)
            .flex(layout::FlexDirection::Column, layout::MainAxisAlignment::Center,
                  layout::CrossAxisAlignment::Center, 12.0f)
            .padding(core::Edges::uniform(24.0f))
            .background(c.surface)
            .border(c.outline_variant, core::Edges::uniform(1.0f))
            .corner_radii(core::CornerRadii::uniform(12.0f))
            .shadow(core::Color{0.0f, 0.0f, 0.0f, 0.2f}, 12.0f, 0.0f, core::Point{0.0f, 4.0f})
            .child(count_text)
            .child(button_row)
            .child(options_row)
            .child(slider)
            .child(text_input)
            .child(vlist)
            .build();

    auto root = widget::BoxBuilder(tree)
                    .flex(layout::FlexDirection::Column, layout::MainAxisAlignment::Center,
                          layout::CrossAxisAlignment::Center)
                    .background(c.background)
                    .child(card)
                    .build();

    tree.set_root(root);
    if (dispatcher) {
        dispatcher->reset();
    }
    if (window && window->size().width > 0.0f && window->size().height > 0.0f) {
        tree.layout(layout::BoxConstraints::tight(window->size()));
    }
}

int main() {
    platform::win32::Win32Window window;
    gpu::D3D11Device device;

    platform::win32::WindowDesc desc{.title = "North",
                                     .width = 680,
                                     .height = 680,
                                     .resizable = true,
                                     .min_width = 380,
                                     .min_height = 380};

    if (!window.create(desc, &device)) {
        return 1;
    }

    render::Renderer renderer(device);
    if (!renderer.init()) {
        return 1;
    }

    DemoAppState state;
    state.window = &window;
    g_demo_state = &state;

    state.dispatcher = std::make_unique<event::InputDispatcher>(state.tree);
    state.dispatcher->set_window(&window);
    window.set_listener(state.dispatcher.get());

    state.counter.bind_dirty(
        [](void* ctx, uint32_t) {
            auto* s = static_cast<DemoAppState*>(ctx);
            s->rebuild();
        },
        &state);

    state.is_dark.bind_dirty(
        [](void* ctx, uint32_t) {
            auto* s = static_cast<DemoAppState*>(ctx);
            s->rebuild();
        },
        &state);

    state.rebuild();

    auto& app = platform::win32::Win32App::instance();
    app.register_window(&window);

    struct RunContext {
        render::Renderer* renderer;
        gpu::D3D11Device* device;
        DemoAppState* state;
    };
    static RunContext ctx{.renderer = &renderer, .device = &device, .state = &state};

    app.set_idle_callback([]() {
        if (!ctx.state || !ctx.state->window || !ctx.state->window->is_open()) {
            return;
        }

        core::Size sz = ctx.state->window->size();
        if (sz.width <= 0.0f || sz.height <= 0.0f) {
            return;
        }

        ctx.state->tree.layout(layout::BoxConstraints::tight(sz));

        const auto& theme = ctx.state->is_dark.get() ? style::Theme::dark() : style::Theme::light();

        ctx.renderer->render_frame(ctx.state->tree, sz, theme.colors().background);
        ctx.device->present();
    });

    window.show(true);
    app.run();

    return 0;
}
