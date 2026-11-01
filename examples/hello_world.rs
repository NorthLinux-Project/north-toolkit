use std::sync::atomic::{AtomicUsize, Ordering};
use std::sync::Arc;

use anyhow::Result;
use north_toolkit::layout::{
    AlignItems, Dimension, EdgeInsets, FlexDirection, FlexStyle, JustifyContent,
};
use north_toolkit::widget::{
    Button, Checkbox, Container, Dialog, Dropdown, Icon, IconName, Label, ListItem,
    ProgressBar, RadioButton, RadioGroup, RadioGroupOrientation, Slider, Spinner,
    SplitOrientation, SplitView, Switch, TableColumn, TableView, TabBarOrientation, Tabs,
    TextArea, TextInput, Titlebar, Toast, VirtualList, WidgetTree,
};
use north_toolkit::{Application, WindowDecorations};

fn main() -> Result<()> {
    if std::env::var("RUST_LOG").is_err() {
        unsafe {
            std::env::set_var("RUST_LOG", "info");
        }
    }
    tracing_subscriber::fmt::init();

    let mut app = Application::new("North Toolkit Showcase", 1080, 760)?
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
        padding: EdgeInsets::symmetric(16.0, 24.0),
        gap: 12.0,
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

    let mut notes_area = TextArea::new(
        "Multiline notes: type paragraphs, press Enter for newlines, hold keys for repeat, or compose via IME...",
    )
    .with_text(
        "Multiline TextArea with soft word wrapping, sticky-column Up/Down navigation, key repeat, and Wayland zwp_text_input_v3 IME preedit.\nPress Enter to insert new lines or hold any key to test Wayland repeat_info synthesis.",
    )
    .with_height(88.0);
    notes_area.style.width = Dimension::Percent(100.0);
    tree.add_widget(&mut app.layout_engine, Box::new(notes_area), Some(content));

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

    let toast_handle = tree.toast_handle();
    let toast_btn = Button::secondary("Show Toast")
        .with_icon(IconName::Check)
        .with_tooltip("Trigger an animated toast notification card")
        .on_click(move || {
            toast_handle.push(
                Toast::success("Pipeline Synchronized")
                    .with_body("Wayland surface presentation frame callback dispatched.")
                    .with_duration(4000.0)
                    .with_action("Dismiss", || {
                        println!("Toast action clicked: Dismiss");
                    }),
            );
        });
    tree.add_widget(&mut app.layout_engine, Box::new(toast_btn), Some(controls_row_id));

    let mut tabs = Tabs::new(TabBarOrientation::Horizontal);
    tabs.style.height = Dimension::Points(340.0);
    tabs.style.flex_grow = 1.0;
    let tabs_id = tree.add_widget(&mut app.layout_engine, Box::new(tabs), Some(content));

    // Page 1: Panes (SplitView with VirtualList and TableView)
    let page1_container = Container::new(FlexStyle {
        direction: FlexDirection::Column,
        width: Dimension::Percent(100.0),
        height: Dimension::Percent(100.0),
        flex_grow: 1.0,
        ..Default::default()
    });
    let page1_id = tree.add_widget(&mut app.layout_engine, Box::new(page1_container), Some(tabs_id));

    let split_view = SplitView::new(SplitOrientation::Horizontal)
        .with_split_offset(480.0)
        .with_min_sizes(240.0, 240.0)
        .with_divider_thickness(6.0);
    let split_id = tree.add_widget(&mut app.layout_engine, Box::new(split_view), Some(page1_id));

    let list_column = Container::new(FlexStyle {
        direction: FlexDirection::Column,
        gap: 8.0,
        width: Dimension::Points(480.0),
        height: Dimension::Percent(100.0),
        ..Default::default()
    });
    let list_column_id = tree.add_widget(&mut app.layout_engine, Box::new(list_column), Some(split_id));

    let list_header = Label::new("Virtual List (10,000 items - O(1) Windowing)")
        .with_font_size(13.0)
        .with_color([0.80, 0.80, 0.80, 1.0]);
    tree.add_widget(&mut app.layout_engine, Box::new(list_header), Some(list_column_id));

    let list_card = Container::new(FlexStyle {
        direction: FlexDirection::Column,
        padding: EdgeInsets::all(6.0),
        width: Dimension::Percent(100.0),
        height: Dimension::Points(280.0),
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
        height: Dimension::Percent(100.0),
        ..Default::default()
    });
    let table_column_id = tree.add_widget(&mut app.layout_engine, Box::new(table_column), Some(split_id));

    let table_header = Label::new("Table View (Multi-Column with Sorting & Resizing)")
        .with_font_size(13.0)
        .with_color([0.80, 0.80, 0.80, 1.0]);
    tree.add_widget(&mut app.layout_engine, Box::new(table_header), Some(table_column_id));

    let table_card = Container::new(FlexStyle {
        direction: FlexDirection::Column,
        padding: EdgeInsets::all(6.0),
        width: Dimension::Percent(100.0),
        height: Dimension::Points(280.0),
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

    // Page 2: Controls & Progress Indicators
    let page2_container = Container::new(FlexStyle {
        direction: FlexDirection::Row,
        gap: 32.0,
        align_items: AlignItems::FlexStart,
        padding: EdgeInsets::symmetric(16.0, 20.0),
        width: Dimension::Percent(100.0),
        height: Dimension::Percent(100.0),
        flex_grow: 1.0,
        ..Default::default()
    })
    .with_background([0.08, 0.08, 0.08, 1.0])
    .with_border([0.18, 0.18, 0.18, 1.0], 1.0)
    .with_corner_radius(8.0);
    let page2_id = tree.add_widget(&mut app.layout_engine, Box::new(page2_container), Some(tabs_id));

    // Column A: Radio group & RadioButton
    let radio_col = Container::new(FlexStyle {
        direction: FlexDirection::Column,
        gap: 12.0,
        width: Dimension::Points(340.0),
        ..Default::default()
    });
    let radio_col_id = tree.add_widget(&mut app.layout_engine, Box::new(radio_col), Some(page2_id));

    let radio_header = Label::new("Graphics Backend Mode (RadioGroup)")
        .with_font_size(13.0)
        .with_color([0.80, 0.80, 0.80, 1.0]);
    tree.add_widget(&mut app.layout_engine, Box::new(radio_header), Some(radio_col_id));

    let radio_group = RadioGroup::new(RadioGroupOrientation::Vertical)
        .with_option("Vulkan 1.3 Hardware Acceleration")
        .with_option("Wayland DRM Direct Lease")
        .with_option("Software Tiny-Skia Rasterizer")
        .with_selected(0)
        .on_select(|idx| {
            println!("RadioGroup selection: Option #{}", idx);
        });
    tree.add_widget(&mut app.layout_engine, Box::new(radio_group), Some(radio_col_id));

    let single_radio = RadioButton::new("Enable Extended Frame Timing Telemetry")
        .with_selected(true)
        .on_select(|| {
            println!("Single RadioButton selected");
        });
    tree.add_widget(&mut app.layout_engine, Box::new(single_radio), Some(radio_col_id));

    // Column B: Progress Bars & Spinner
    let progress_col = Container::new(FlexStyle {
        direction: FlexDirection::Column,
        gap: 14.0,
        width: Dimension::Points(360.0),
        ..Default::default()
    });
    let progress_col_id = tree.add_widget(&mut app.layout_engine, Box::new(progress_col), Some(page2_id));

    let progress_header = Label::new("Progress Indicators & Spinner")
        .with_font_size(13.0)
        .with_color([0.80, 0.80, 0.80, 1.0]);
    tree.add_widget(&mut app.layout_engine, Box::new(progress_header), Some(progress_col_id));

    let det_bar = ProgressBar::new(0.68)
        .with_show_label(true)
        .with_height(14.0);
    tree.add_widget(&mut app.layout_engine, Box::new(det_bar), Some(progress_col_id));

    let indet_bar = ProgressBar::indeterminate()
        .with_height(8.0);
    tree.add_widget(&mut app.layout_engine, Box::new(indet_bar), Some(progress_col_id));

    let spinner_row = Container::new(FlexStyle {
        direction: FlexDirection::Row,
        gap: 12.0,
        align_items: AlignItems::Center,
        ..Default::default()
    });
    let spinner_row_id = tree.add_widget(&mut app.layout_engine, Box::new(spinner_row), Some(progress_col_id));

    let spinner = Spinner::new().with_diameter(28.0).with_thickness(3.5);
    tree.add_widget(&mut app.layout_engine, Box::new(spinner), Some(spinner_row_id));

    let spinner_label = Label::new("Synchronizing texture atlas batches...")
        .with_font_size(12.0)
        .with_color([0.65, 0.65, 0.65, 1.0]);
    tree.add_widget(&mut app.layout_engine, Box::new(spinner_label), Some(spinner_row_id));

    // Register pages with Tabs
    tree.get_widget_mut::<Tabs>(tabs_id).unwrap()
        .add_tab("Panes (SplitView)", page1_id)
        .add_tab("Controls & Progress", page2_id);

    app.run(tree)
}
