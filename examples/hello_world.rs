use std::sync::atomic::{AtomicUsize, Ordering};
use std::sync::Arc;

use anyhow::Result;
use north_toolkit::layout::{
    AlignItems, Dimension, EdgeInsets, FlexDirection, FlexStyle, JustifyContent,
};
use north_toolkit::platform::WindowDecorations;
use north_toolkit::widget::{
    Button, Checkbox, Container, Dialog, Dropdown, Icon, IconName, Label, ListItem,
    Slider, Switch, TableColumn, TableView, TextInput, Titlebar, VirtualList, WidgetTree,
};
use north_toolkit::Application;

fn main() -> Result<()> {
    if std::env::var("RUST_LOG").is_err() {
        unsafe {
            std::env::set_var("RUST_LOG", "info");
        }
    }
    tracing_subscriber::fmt::init();

    let mut app = Application::new("North Toolkit Showcase", 1080, 720)?
        .with_decorations(WindowDecorations::ClientSide);
    let mut tree = WidgetTree::new();

    let root_container = Container::new(FlexStyle {
        direction: FlexDirection::Column,
        justify_content: JustifyContent::FlexStart,
        align_items: AlignItems::Stretch,
        width: Dimension::Percent(100.0),
        height: Dimension::Percent(100.0),
        ..Default::default()
    });
    let root = tree.add_widget(&mut app.layout_engine, Box::new(root_container), None);

    let titlebar = Titlebar::new("North Toolkit Showcase")
        .with_icon(IconName::Settings);
    tree.add_widget(&mut app.layout_engine, Box::new(titlebar), Some(root));

    let content_container = Container::new(FlexStyle {
        direction: FlexDirection::Column,
        justify_content: JustifyContent::FlexStart,
        align_items: AlignItems::Stretch,
        padding: EdgeInsets::symmetric(18.0, 24.0),
        gap: 14.0,
        flex_grow: 1.0,
        width: Dimension::Percent(100.0),
        ..Default::default()
    });
    let content = tree.add_widget(&mut app.layout_engine, Box::new(content_container), Some(root));

    let header_row = Container::new(FlexStyle {
        direction: FlexDirection::Row,
        justify_content: JustifyContent::SpaceBetween,
        align_items: AlignItems::Center,
        ..Default::default()
    });
    let header_id = tree.add_widget(&mut app.layout_engine, Box::new(header_row), Some(content));

    let title_col = Container::new(FlexStyle {
        direction: FlexDirection::Column,
        gap: 4.0,
        ..Default::default()
    });
    let title_col_id = tree.add_widget(&mut app.layout_engine, Box::new(title_col), Some(header_id));

    let title = Label::new("North Toolkit")
        .with_font_size(22.0)
        .with_color([0.95, 0.95, 0.95, 1.0]);
    tree.add_widget(&mut app.layout_engine, Box::new(title), Some(title_col_id));

    let subtitle = Label::new("Vulkan 1.3 + Wayland Native Retained GUI Engine in Rust")
        .with_font_size(12.0)
        .with_color([0.55, 0.55, 0.55, 1.0]);
    tree.add_widget(&mut app.layout_engine, Box::new(subtitle), Some(title_col_id));

    let input_row = Container::new(FlexStyle {
        direction: FlexDirection::Row,
        gap: 12.0,
        align_items: AlignItems::Center,
        ..Default::default()
    });
    let input_row_id = tree.add_widget(&mut app.layout_engine, Box::new(input_row), Some(content));

    let input1 = TextInput::new("Source: Type, drag to select, Ctrl+C / Ctrl+X...")
        .with_width(340.0)
        .with_leading_icon(IconName::Search);
    tree.add_widget(&mut app.layout_engine, Box::new(input1), Some(input_row_id));

    let input2 = TextInput::new("Destination: Ctrl+V or middle-click to paste...")
        .with_width(340.0)
        .with_leading_icon(IconName::File);
    tree.add_widget(&mut app.layout_engine, Box::new(input2), Some(input_row_id));

    let controls_row = Container::new(FlexStyle {
        direction: FlexDirection::Row,
        gap: 14.0,
        align_items: AlignItems::Center,
        ..Default::default()
    });
    let controls_row_id = tree.add_widget(&mut app.layout_engine, Box::new(controls_row), Some(content));

    let counter = Arc::new(AtomicUsize::new(0));
    let counter_clone = counter.clone();

    let button = Button::new("Click Counter")
        .with_icon(IconName::Plus)
        .with_tooltip("Click to increment the counter")
        .on_click(move || {
            let val = counter_clone.fetch_add(1, Ordering::SeqCst) + 1;
            println!("Counter clicked: {}", val);
        });
    tree.add_widget(&mut app.layout_engine, Box::new(button), Some(controls_row_id));

    let secondary_btn = Button::secondary("Reset")
        .with_icon(IconName::Refresh)
        .with_tooltip("Reset state to initial defaults")
        .on_click(move || {
            println!("Reset triggered");
        });
    tree.add_widget(&mut app.layout_engine, Box::new(secondary_btn), Some(controls_row_id));

    let dropdown = Dropdown::new(vec![
        "Vulkan 1.3 (Wayland)".to_string(),
        "Software Renderer".to_string(),
        "OpenGL ES 3.2".to_string(),
        "WebGPU Core".to_string(),
    ])
    .with_width(180.0)
    .on_select(|idx, item| {
        println!("Selected graphics backend #{}: {}", idx, item);
    });
    tree.add_widget(&mut app.layout_engine, Box::new(dropdown), Some(controls_row_id));

    let modal_handle = tree.modal_handle();
    let dialog_btn = Button::secondary("Open Dialog")
        .with_icon(IconName::Settings)
        .with_tooltip("Open modal confirmation dialog")
        .on_click(move || {
            modal_handle.open(
                Dialog::confirm(
                    "Restart Pipeline?",
                    "Are you sure you want to restart the Wayland graphics pipeline?",
                )
                .on_response(|resp| {
                    println!("Modal response: {:?}", resp);
                }),
            );
        });
    tree.add_widget(&mut app.layout_engine, Box::new(dialog_btn), Some(controls_row_id));

    let checkbox = Checkbox::new("Hardware Acceleration")
        .with_checked(true)
        .with_tooltip("Toggle Vulkan GPU rendering pipeline")
        .on_toggle(|val| {
            println!("Checkbox toggled: {}", val);
        });
    tree.add_widget(&mut app.layout_engine, Box::new(checkbox), Some(controls_row_id));

    let switch = Switch::new(true)
        .with_tooltip("Toggle dark / light color scheme")
        .on_toggle(|val| {
            println!("Switch state: {}", val);
        });
    tree.add_widget(&mut app.layout_engine, Box::new(switch), Some(controls_row_id));

    let switch_label = Label::new("Dark Mode").with_font_size(13.0);
    tree.add_widget(&mut app.layout_engine, Box::new(switch_label), Some(controls_row_id));

    let volume_icon = Icon::new(IconName::Volume)
        .with_size(16.0)
        .with_color([0.7, 0.7, 0.7, 1.0])
        .with_tooltip("Master audio output level");
    tree.add_widget(&mut app.layout_engine, Box::new(volume_icon), Some(controls_row_id));

    let slider = Slider::new(0.0, 100.0, 75.0)
        .with_width(120.0)
        .with_step(5.0)
        .with_tooltip("Adjust audio volume level")
        .on_change(|v| {
            println!("Slider volume: {:.0}%", v);
        });
    tree.add_widget(&mut app.layout_engine, Box::new(slider), Some(controls_row_id));

    let data_section = Container::new(FlexStyle {
        direction: FlexDirection::Row,
        gap: 16.0,
        align_items: AlignItems::Stretch,
        width: Dimension::Percent(100.0),
        height: Dimension::Points(380.0),
        ..Default::default()
    });
    let data_section_id = tree.add_widget(&mut app.layout_engine, Box::new(data_section), Some(content));

    let list_column = Container::new(FlexStyle {
        direction: FlexDirection::Column,
        gap: 8.0,
        width: Dimension::Points(460.0),
        height: Dimension::Percent(100.0),
        ..Default::default()
    });
    let list_column_id = tree.add_widget(&mut app.layout_engine, Box::new(list_column), Some(data_section_id));

    let list_header = Label::new("Virtual List (10,000 items - O(1) Windowing)")
        .with_font_size(13.0)
        .with_color([0.80, 0.80, 0.80, 1.0]);
    tree.add_widget(&mut app.layout_engine, Box::new(list_header), Some(list_column_id));

    let list_card = Container::new(FlexStyle {
        direction: FlexDirection::Column,
        padding: EdgeInsets::all(6.0),
        width: Dimension::Percent(100.0),
        height: Dimension::Points(340.0),
        ..Default::default()
    })
    .with_background([0.08, 0.08, 0.08, 1.0])
    .with_border([0.18, 0.18, 0.18, 1.0], 1.0)
    .with_corner_radius(8.0);
    let list_card_id = tree.add_widget(&mut app.layout_engine, Box::new(list_card), Some(list_column_id));

    let virtual_items: Vec<ListItem> = (1..=10_000)
        .map(|i| {
            ListItem::new(format!("Surface Feed Node #{}", i))
                .with_icon(if i % 2 == 0 { IconName::File } else { IconName::Folder })
                .with_trailing_text(format!("{} KB/s", (i * 37) % 850 + 50))
        })
        .collect();

    let virtual_list = VirtualList::with_items(virtual_items)
        .with_item_height(34.0)
        .on_select(|indices| {
            println!("Selected virtual list items: {:?}", indices);
        })
        .on_activate(|idx| {
            println!("Activated virtual list item #{}", idx + 1);
        });
    tree.add_widget(&mut app.layout_engine, Box::new(virtual_list), Some(list_card_id));

    let table_column = Container::new(FlexStyle {
        direction: FlexDirection::Column,
        gap: 8.0,
        flex_grow: 1.0,
        width: Dimension::Points(540.0),
        height: Dimension::Percent(100.0),
        ..Default::default()
    });
    let table_column_id = tree.add_widget(&mut app.layout_engine, Box::new(table_column), Some(data_section_id));

    let table_header = Label::new("Table View (Multi-Column with Sorting & Resizing)")
        .with_font_size(13.0)
        .with_color([0.80, 0.80, 0.80, 1.0]);
    tree.add_widget(&mut app.layout_engine, Box::new(table_header), Some(table_column_id));

    let table_card = Container::new(FlexStyle {
        direction: FlexDirection::Column,
        padding: EdgeInsets::all(6.0),
        width: Dimension::Percent(100.0),
        height: Dimension::Points(340.0),
        ..Default::default()
    })
    .with_background([0.08, 0.08, 0.08, 1.0])
    .with_border([0.18, 0.18, 0.18, 1.0], 1.0)
    .with_corner_radius(8.0);
    let table_card_id = tree.add_widget(&mut app.layout_engine, Box::new(table_card), Some(table_column_id));

    let table_columns = vec![
        TableColumn::new("id", "ID", 60.0).with_min_width(40.0),
        TableColumn::new("service", "Service / Node", 200.0).with_min_width(100.0),
        TableColumn::new("memory", "Memory", 110.0).with_min_width(80.0),
        TableColumn::new("status", "Status", 130.0).with_min_width(80.0),
    ];

    let table_rows: Vec<Vec<String>> = (1..=1_000)
        .map(|i| {
            let status = match i % 4 {
                0 => "Active",
                1 => "Idle",
                2 => "Standby",
                _ => "Syncing",
            };
            vec![
                format!("{:04}", i),
                format!("wayland-surface-{}", i),
                format!("{:.1} MB", (i as f32 * 1.7) % 64.0 + 8.0),
                status.to_string(),
            ]
        })
        .collect();

    let table_view = TableView::with_rows(table_columns, table_rows)
        .on_sort(|col_idx, dir| {
            println!("Table sorted by col #{}: {:?}", col_idx, dir);
        });
    tree.add_widget(&mut app.layout_engine, Box::new(table_view), Some(table_card_id));

    app.run(tree)
}
