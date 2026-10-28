use std::sync::atomic::{AtomicUsize, Ordering};
use std::sync::Arc;

use anyhow::Result;
use north_toolkit::layout::{
    AlignItems, Dimension, EdgeInsets, FlexDirection, FlexStyle, JustifyContent,
};
use north_toolkit::widget::{Button, Container, Label, TextInput, WidgetTree};
use north_toolkit::Application;

fn main() -> Result<()> {
    if std::env::var("RUST_LOG").is_err() {
        unsafe {
            std::env::set_var("RUST_LOG", "info");
        }
    }
    tracing_subscriber::fmt::init();

    let mut app = Application::new("North Toolkit Demo", 640, 480)?;
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
        .with_color([0.35, 0.65, 1.0, 1.0]);
    tree.add_widget(&mut app.layout_engine, Box::new(title), Some(root));

    let subtitle = Label::new("Vulkan 1.3 + Wayland Native Retained UI in Rust")
        .with_font_size(14.0)
        .with_color([0.65, 0.70, 0.78, 1.0]);
    tree.add_widget(&mut app.layout_engine, Box::new(subtitle), Some(root));

    let input = TextInput::new("Type something here...").with_width(320.0);
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
        println!("Button clicked! Count: {}", val);
    });
    tree.add_widget(&mut app.layout_engine, Box::new(button), Some(row_id));

    let secondary_btn = Button::new("Action").on_click(|| {
        println!("Secondary action clicked");
    });
    tree.add_widget(&mut app.layout_engine, Box::new(secondary_btn), Some(row_id));

    app.run(tree)
}
