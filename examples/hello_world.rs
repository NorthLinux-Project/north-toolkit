use std::sync::atomic::{AtomicUsize, Ordering};
use std::sync::Arc;

use anyhow::Result;
use north_toolkit::layout::{
    AlignItems, Dimension, EdgeInsets, FlexDirection, FlexStyle, JustifyContent, Overflow,
};
use north_toolkit::widget::{
    Button, Checkbox, Container, Dropdown, Label, ScrollArea, Slider, Switch, TextInput,
    WidgetTree,
};
use north_toolkit::Application;

fn main() -> Result<()> {
    if std::env::var("RUST_LOG").is_err() {
        unsafe {
            std::env::set_var("RUST_LOG", "info");
        }
    }
    tracing_subscriber::fmt::init();

    let mut app = Application::new("North Toolkit Showcase", 720, 600)?;
    let mut tree = WidgetTree::new();

    let root_container = Container::new(FlexStyle {
        direction: FlexDirection::Column,
        justify_content: JustifyContent::FlexStart,
        align_items: AlignItems::Stretch,
        padding: EdgeInsets::all(24.0),
        gap: 16.0,
        width: Dimension::Percent(100.0),
        height: Dimension::Percent(100.0),
        ..Default::default()
    });
    let root = tree.add_widget(&mut app.layout_engine, Box::new(root_container), None);

    let title = Label::new("North Toolkit")
        .with_font_size(24.0)
        .with_color([0.93, 0.93, 0.93, 1.0]);
    tree.add_widget(&mut app.layout_engine, Box::new(title), Some(root));

    let subtitle = Label::new("Vulkan 1.3 + Wayland Native Retained GUI with Reactive Throttling & XKB")
        .with_font_size(13.0)
        .with_color([0.54, 0.54, 0.54, 1.0]);
    tree.add_widget(&mut app.layout_engine, Box::new(subtitle), Some(root));

    let input = TextInput::new("Type here (Tab to switch focus)...").with_width(360.0);
    tree.add_widget(&mut app.layout_engine, Box::new(input), Some(root));

    let button_row = Container::new(FlexStyle {
        direction: FlexDirection::Row,
        gap: 12.0,
        align_items: AlignItems::Center,
        ..Default::default()
    });
    let row_id = tree.add_widget(&mut app.layout_engine, Box::new(button_row), Some(root));

    let counter = Arc::new(AtomicUsize::new(0));
    let counter_clone = counter.clone();

    let button = Button::new("Click Counter").on_click(move || {
        let val = counter_clone.fetch_add(1, Ordering::SeqCst) + 1;
        println!("Counter clicked: {}", val);
    });
    tree.add_widget(&mut app.layout_engine, Box::new(button), Some(row_id));

    let secondary_btn = Button::secondary("Reset").on_click(move || {
        println!("Reset triggered");
    });
    tree.add_widget(&mut app.layout_engine, Box::new(secondary_btn), Some(row_id));

    let dropdown = Dropdown::new(vec![
        "Vulkan 1.3 (Wayland)".to_string(),
        "Software Renderer".to_string(),
        "OpenGL ES 3.2".to_string(),
        "WebGPU Core".to_string(),
    ])
    .with_width(200.0)
    .on_select(|idx, item| {
        println!("Selected graphics backend #{}: {}", idx, item);
    });
    tree.add_widget(&mut app.layout_engine, Box::new(dropdown), Some(row_id));

    let toggles_row = Container::new(FlexStyle {
        direction: FlexDirection::Row,
        gap: 24.0,
        align_items: AlignItems::Center,
        ..Default::default()
    });
    let toggles_id = tree.add_widget(&mut app.layout_engine, Box::new(toggles_row), Some(root));

    let checkbox = Checkbox::new("Hardware Acceleration").with_checked(true).on_toggle(|val| {
        println!("Checkbox toggled: {}", val);
    });
    tree.add_widget(&mut app.layout_engine, Box::new(checkbox), Some(toggles_id));

    let switch = Switch::new(true).on_toggle(|val| {
        println!("Switch state: {}", val);
    });
    tree.add_widget(&mut app.layout_engine, Box::new(switch), Some(toggles_id));

    let switch_label = Label::new("Dark Mode").with_font_size(14.0);
    tree.add_widget(&mut app.layout_engine, Box::new(switch_label), Some(toggles_id));

    let slider_row = Container::new(FlexStyle {
        direction: FlexDirection::Row,
        gap: 16.0,
        align_items: AlignItems::Center,
        ..Default::default()
    });
    let slider_row_id = tree.add_widget(&mut app.layout_engine, Box::new(slider_row), Some(root));

    let slider_lbl = Label::new("Volume: 75%").with_font_size(13.0);
    tree.add_widget(&mut app.layout_engine, Box::new(slider_lbl), Some(slider_row_id));

    let slider = Slider::new(0.0, 100.0, 75.0)
        .with_width(200.0)
        .with_step(5.0)
        .on_change(|v| {
            println!("Slider volume: {:.0}%", v);
        });
    tree.add_widget(&mut app.layout_engine, Box::new(slider), Some(slider_row_id));

    let scroll_container = Container::new(FlexStyle {
        direction: FlexDirection::Column,
        padding: EdgeInsets::all(12.0),
        width: Dimension::Points(420.0),
        height: Dimension::Points(180.0),
        ..Default::default()
    })
    .with_background([0.094, 0.094, 0.094, 1.0])
    .with_border([0.15, 0.15, 0.15, 1.0], 1.0)
    .with_corner_radius(8.0);
    let scroll_container_id = tree.add_widget(&mut app.layout_engine, Box::new(scroll_container), Some(root));

    let scroll_area = ScrollArea::new(FlexStyle {
        direction: FlexDirection::Column,
        gap: 8.0,
        width: Dimension::Points(396.0),
        height: Dimension::Points(156.0),
        overflow: Overflow::Scroll,
        ..Default::default()
    })
    .with_max_scroll(280.0);
    let scroll_id = tree.add_widget(&mut app.layout_engine, Box::new(scroll_area), Some(scroll_container_id));

    for i in 1..=10 {
        let card = Container::new(FlexStyle {
            direction: FlexDirection::Row,
            justify_content: JustifyContent::SpaceBetween,
            align_items: AlignItems::Center,
            padding: EdgeInsets::symmetric(8.0, 12.0),
            width: Dimension::Points(380.0),
            height: Dimension::Points(36.0),
            flex_shrink: 0.0,
            ..Default::default()
        })
        .with_background([0.13, 0.13, 0.13, 1.0])
        .with_border([0.18, 0.18, 0.18, 1.0], 1.0)
        .with_corner_radius(4.0);
        let card_id = tree.add_widget(&mut app.layout_engine, Box::new(card), Some(scroll_id));

        let card_label = Label::new(format!("Item #{} - Wayland Surface Feed", i))
            .with_font_size(12.0)
            .with_color([0.88, 0.88, 0.88, 1.0]);
        tree.add_widget(&mut app.layout_engine, Box::new(card_label), Some(card_id));
    }

    app.run(tree)
}
