#include "north/core/tree.hpp"
#include "north/event/input_dispatcher.hpp"
#include "north/gpu/d3d11/d3d11_device.hpp"
#include "north/image/image_cache.hpp"
#include "north/platform/win32/win32_app.hpp"
#include "north/platform/win32/win32_window.hpp"
#include "north/render/renderer.hpp"
#include "north/style/theme.hpp"
#include "north/widget/box.hpp"
#include "north/widget/image.hpp"

#include <memory>

using namespace north;

namespace {

struct AppState {
    core::NodeTree tree;
    platform::win32::Win32Window* window{nullptr};
    gpu::Device* device{nullptr};
    std::unique_ptr<event::InputDispatcher> dispatcher;

    void build_tree() {
        if (tree.root().is_valid()) {
            tree.destroy_node(tree.root());
        }

        auto img_node = widget::ImageBuilder(tree)
                            .load_file(*device, "assets/images/download.jpg")
                            .content_scale(image::ContentScale::Fit)
                            .alignment(image::ImageAlignment::Center)
                            .corner_radii(core::CornerRadii::uniform(16.0f))
                            .border(core::Color::from_rgba8(60, 60, 70, 160), 1.0f)
                            .build();

        auto root = widget::BoxBuilder(tree)
                        .flex(layout::FlexDirection::Column, layout::MainAxisAlignment::Center,
                              layout::CrossAxisAlignment::Center)
                        .background(core::Color::from_rgba8(16, 17, 20, 255))
                        .padding(core::Edges::uniform(24.0f))
                        .child(img_node)
                        .build();

        tree.set_root(root);
        if (dispatcher) {
            dispatcher->reset();
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

    platform::win32::WindowDesc desc{.title = "North Image Viewer",
                                     .width = 900,
                                     .height = 700,
                                     .resizable = true,
                                     .min_width = 300,
                                     .min_height = 300};

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
        ctx.renderer->render_frame(ctx.state->tree, sz, core::Color::from_rgba8(16, 17, 20, 255));
        ctx.device->present();
    });

    window.show(true);
    app.run();

    return 0;
}
