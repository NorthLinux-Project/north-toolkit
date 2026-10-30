//! Custom Flexbox layout engine, style properties, dimension resolution, and node hierarchy.

/// Direction of the flex container main axis.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub enum FlexDirection {
    /// Items flow horizontally from left to right.
    #[default]
    Row,
    /// Items flow vertically from top to bottom.
    Column,
    /// Items flow horizontally in reverse order.
    RowReverse,
    /// Items flow vertically in reverse order.
    ColumnReverse,
}

impl FlexDirection {
    /// Returns `true` if the flex direction is horizontal.
    pub fn is_row(self) -> bool {
        matches!(self, FlexDirection::Row | FlexDirection::RowReverse)
    }

    /// Returns `true` if items flow in reverse order.
    pub fn is_reverse(self) -> bool {
        matches!(self, FlexDirection::RowReverse | FlexDirection::ColumnReverse)
    }
}

/// Wrapping behavior of flex items along the cross axis.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub enum FlexWrap {
    /// Single-line container; children may overflow or shrink.
    #[default]
    NoWrap,
    /// Multi-line container; children wrap onto new lines.
    Wrap,
}

/// Main-axis distribution of free space between items.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub enum JustifyContent {
    /// Items packed flush with start of the main axis.
    #[default]
    FlexStart,
    /// Items centered along the main axis.
    Center,
    /// Items packed flush with end of the main axis.
    FlexEnd,
    /// Items evenly distributed; first at start, last at end.
    SpaceBetween,
    /// Items evenly distributed with equal space around each item.
    SpaceAround,
    /// Items distributed with equal spacing between each item and edges.
    SpaceEvenly,
}

/// Cross-axis alignment of items along a single flex line.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub enum AlignItems {
    /// Stretch items to fill the cross axis dimension of the line.
    #[default]
    Stretch,
    /// Align items flush with start of the cross axis.
    FlexStart,
    /// Center items along the cross axis.
    Center,
    /// Align items flush with end of the cross axis.
    FlexEnd,
}

/// Per-item override for cross-axis alignment.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub enum AlignSelf {
    /// Inherit `AlignItems` from parent container.
    #[default]
    Auto,
    /// Align item flush with start of the cross axis.
    FlexStart,
    /// Center item along the cross axis.
    Center,
    /// Align item flush with end of the cross axis.
    FlexEnd,
    /// Stretch item across the cross axis.
    Stretch,
}

/// Sizing dimension specification for width, height, and basis.
#[derive(Debug, Clone, Copy, PartialEq, Default)]
pub enum Dimension {
    /// Automatic sizing based on intrinsic content or flex distribution.
    #[default]
    Auto,
    /// Fixed size in logical points/pixels.
    Points(f32),
    /// Percentage relative to parent container size.
    Percent(f32),
}

impl Dimension {
    /// Resolves dimension against an optional parent size.
    pub fn resolve(self, parent_size: Option<f32>) -> Option<f32> {
        match self {
            Dimension::Auto => None,
            Dimension::Points(pts) => Some(pts),
            Dimension::Percent(pct) => parent_size.map(|p| p * (pct / 100.0)),
        }
    }
}

/// Insets applied to borders, padding, or margins.
#[derive(Debug, Clone, Copy, PartialEq, Default)]
pub struct EdgeInsets {
    /// Top inset.
    pub top: f32,
    /// Right inset.
    pub right: f32,
    /// Bottom inset.
    pub bottom: f32,
    /// Left inset.
    pub left: f32,
}

impl EdgeInsets {
    /// Zero insets on all four edges.
    pub const ZERO: Self = Self {
        top: 0.0,
        right: 0.0,
        bottom: 0.0,
        left: 0.0,
    };

    /// Uniform insets on all four edges.
    pub fn all(val: f32) -> Self {
        Self {
            top: val,
            right: val,
            bottom: val,
            left: val,
        }
    }

    /// Symmetric vertical and horizontal insets.
    pub fn symmetric(vertical: f32, horizontal: f32) -> Self {
        Self {
            top: vertical,
            right: horizontal,
            bottom: vertical,
            left: horizontal,
        }
    }

    /// Sum of left and right insets.
    pub fn horizontal(&self) -> f32 {
        self.left + self.right
    }

    /// Sum of top and bottom insets.
    pub fn vertical(&self) -> f32 {
        self.top + self.bottom
    }
}

/// Computed 2D bounding rectangle in logical coordinates.
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct LayoutRect {
    /// Top-left X coordinate.
    pub x: f32,
    /// Top-left Y coordinate.
    pub y: f32,
    /// Width in logical points.
    pub width: f32,
    /// Height in logical points.
    pub height: f32,
}

impl LayoutRect {
    /// An empty rectangle positioned at the origin.
    pub const ZERO: Self = Self {
        x: 0.0,
        y: 0.0,
        width: 0.0,
        height: 0.0,
    };

    /// Creates a new layout rectangle.
    pub fn new(x: f32, y: f32, width: f32, height: f32) -> Self {
        Self {
            x,
            y,
            width,
            height,
        }
    }

    /// Converts to `[x, y, width, height]` array format.
    pub fn to_array(self) -> [f32; 4] {
        [self.x, self.y, self.width, self.height]
    }

    /// Returns `true` if logical coordinates `(px, py)` lie within this rectangle.
    pub fn contains(&self, px: f32, py: f32) -> bool {
        px >= self.x && px < (self.x + self.width) && py >= self.y && py < (self.y + self.height)
    }
}

/// Clipping behavior for content extending past node bounds.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub enum Overflow {
    /// Unclipped; children may overflow visible bounds.
    #[default]
    Visible,
    /// Clipped to node bounds.
    Hidden,
    /// Clipped and scrollable.
    Scroll,
}

/// Style attributes determining flexbox layout computation for a node.
#[derive(Debug, Clone, PartialEq)]
pub struct FlexStyle {
    /// Direction of the main axis.
    pub direction: FlexDirection,
    /// Wrapping behavior.
    pub wrap: FlexWrap,
    /// Main-axis alignment and space distribution.
    pub justify_content: JustifyContent,
    /// Cross-axis alignment.
    pub align_items: AlignItems,
    /// Overflow clipping mode.
    pub overflow: Overflow,
    /// Requested width.
    pub width: Dimension,
    /// Requested height.
    pub height: Dimension,
    /// Minimum width constraint.
    pub min_width: Option<f32>,
    /// Maximum width constraint.
    pub max_width: Option<f32>,
    /// Minimum height constraint.
    pub min_height: Option<f32>,
    /// Maximum height constraint.
    pub max_height: Option<f32>,
    /// Growth factor for distributing extra main-axis space.
    pub flex_grow: f32,
    /// Shrink factor when main axis is constrained.
    pub flex_shrink: f32,
    /// Initial main-axis size before growing or shrinking.
    pub flex_basis: Dimension,
    /// Internal padding insets.
    pub padding: EdgeInsets,
    /// External margin insets.
    pub margin: EdgeInsets,
    /// Space between consecutive children.
    pub gap: f32,
}

impl Default for FlexStyle {
    fn default() -> Self {
        Self {
            direction: FlexDirection::Row,
            wrap: FlexWrap::NoWrap,
            justify_content: JustifyContent::FlexStart,
            align_items: AlignItems::Stretch,
            overflow: Overflow::Visible,
            width: Dimension::Auto,
            height: Dimension::Auto,
            min_width: None,
            max_width: None,
            min_height: None,
            max_height: None,
            flex_grow: 0.0,
            flex_shrink: 1.0,
            flex_basis: Dimension::Auto,
            padding: EdgeInsets::ZERO,
            margin: EdgeInsets::ZERO,
            gap: 0.0,
        }
    }
}

/// Handle identifying a layout node within the `LayoutEngine`.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct LayoutNodeId(pub usize);

/// Measurement closure for computing intrinsic node dimensions.
pub type MeasureFunc = Box<dyn Fn(Option<f32>, Option<f32>) -> (f32, f32)>;

/// Internal representation of a layout node.
pub struct LayoutNode {
    /// Style configuration.
    pub style: FlexStyle,
    /// Child node identifiers in document order.
    pub children: Vec<LayoutNodeId>,
    /// Custom intrinsic measurement closure.
    pub measure: Option<MeasureFunc>,
    /// Computed absolute layout bounds.
    pub computed_rect: LayoutRect,
}

impl LayoutNode {
    /// Creates a new layout node with default children and uncomputed rect.
    pub fn new(style: FlexStyle) -> Self {
        Self {
            style,
            children: Vec::new(),
            measure: None,
            computed_rect: LayoutRect::ZERO,
        }
    }
}

/// Retained flexbox layout calculation engine.
pub struct LayoutEngine {
    nodes: Vec<Option<LayoutNode>>,
}

impl LayoutEngine {
    /// Creates an empty layout engine.
    pub fn new() -> Self {
        Self { nodes: Vec::new() }
    }

    /// Allocates a new layout node with the specified style.
    pub fn create_node(&mut self, style: FlexStyle) -> LayoutNodeId {
        let node = LayoutNode::new(style);
        for (idx, slot) in self.nodes.iter_mut().enumerate() {
            if slot.is_none() {
                *slot = Some(node);
                return LayoutNodeId(idx);
            }
        }
        let id = LayoutNodeId(self.nodes.len());
        self.nodes.push(Some(node));
        id
    }

    /// Sets an intrinsic measurement function on a node.
    pub fn set_measure(&mut self, node: LayoutNodeId, measure: MeasureFunc) {
        if let Some(Some(n)) = self.nodes.get_mut(node.0) {
            n.measure = Some(measure);
        }
    }

    /// Updates the style configuration for an existing node.
    pub fn set_style(&mut self, node: LayoutNodeId, style: FlexStyle) {
        if let Some(Some(n)) = self.nodes.get_mut(node.0) {
            n.style = style;
        }
    }

    /// Appends a child node to a parent node.
    pub fn add_child(&mut self, parent: LayoutNodeId, child: LayoutNodeId) {
        if let Some(Some(p)) = self.nodes.get_mut(parent.0) {
            if !p.children.contains(&child) {
                p.children.push(child);
            }
        }
    }

    /// Removes and frees a layout node.
    pub fn remove_node(&mut self, node: LayoutNodeId) {
        if let Some(slot) = self.nodes.get_mut(node.0) {
            *slot = None;
        }
    }

    /// Retrieves the computed bounding rectangle for a node.
    pub fn get_rect(&self, node: LayoutNodeId) -> LayoutRect {
        self.nodes
            .get(node.0)
            .and_then(|n| n.as_ref())
            .map(|n| n.computed_rect)
            .unwrap_or(LayoutRect::ZERO)
    }

    /// Computes layout positions and sizes for the subtree rooted at `root`.
    pub fn compute(&mut self, root: LayoutNodeId, available_width: f32, available_height: f32) {
        self.layout_node(
            root,
            0.0,
            0.0,
            Some(available_width),
            Some(available_height),
        );
    }

    fn layout_node(
        &mut self,
        node_id: LayoutNodeId,
        origin_x: f32,
        origin_y: f32,
        parent_w: Option<f32>,
        parent_h: Option<f32>,
    ) -> (f32, f32) {
        let (style, children_ids, has_measure) = {
            let n = match self.nodes.get(node_id.0).and_then(|n| n.as_ref()) {
                Some(n) => n,
                None => return (0.0, 0.0),
            };
            (n.style.clone(), n.children.clone(), n.measure.is_some())
        };

        if has_measure {
            let measure_result = {
                let n = self.nodes[node_id.0].as_ref().unwrap();
                let measure_fn = n.measure.as_ref().unwrap();
                measure_fn(parent_w, parent_h)
            };
            let w = style
                .width
                .resolve(parent_w)
                .unwrap_or(measure_result.0)
                .max(style.min_width.unwrap_or(0.0));
            let h = style
                .height
                .resolve(parent_h)
                .unwrap_or(measure_result.1)
                .max(style.min_height.unwrap_or(0.0));

            let rect = LayoutRect::new(origin_x, origin_y, w, h);
            self.nodes[node_id.0].as_mut().unwrap().computed_rect = rect;
            return (w, h);
        }

        let explicit_w = style.width.resolve(parent_w);
        let explicit_h = style.height.resolve(parent_h);

        let content_origin_x = origin_x + style.padding.left;
        let content_origin_y = origin_y + style.padding.top;

        let is_row = style.direction.is_row();
        let gap = style.gap;

        let mut child_sizes = Vec::new();
        let mut total_grow = 0.0f32;
        let mut total_shrink = 0.0f32;
        let mut base_main_sum = 0.0f32;
        let mut max_cross = 0.0f32;

        let inner_avail_w = explicit_w.map(|w| (w - style.padding.horizontal()).max(0.0));
        let inner_avail_h = explicit_h.map(|h| (h - style.padding.vertical()).max(0.0));

        let is_scroll_container = style.overflow == Overflow::Scroll;
        let main_avail = if is_scroll_container {
            None
        } else if is_row {
            inner_avail_w
        } else {
            inner_avail_h
        };
        let _cross_avail = if is_row { inner_avail_h } else { inner_avail_w };

        for &child_id in &children_ids {
            let (child_style, child_measured) = match self.nodes.get(child_id.0).and_then(|n| n.as_ref()) {
                Some(n) => {
                    let measured = if let Some(m) = &n.measure {
                        Some(m(inner_avail_w, inner_avail_h))
                    } else if !n.children.is_empty() {
                        Some(self.measure_intrinsic(child_id, inner_avail_w, inner_avail_h))
                    } else {
                        None
                    };
                    (n.style.clone(), measured)
                }
                None => continue,
            };

            let child_w = child_style
                .width
                .resolve(inner_avail_w)
                .or(child_measured.map(|(w, _)| w));
            let child_h = child_style
                .height
                .resolve(inner_avail_h)
                .or(child_measured.map(|(_, h)| h));

            let (main_size, cross_size) = if is_row {
                let m = child_style
                    .flex_basis
                    .resolve(inner_avail_w)
                    .or(child_w)
                    .unwrap_or(0.0);
                let c = child_h.unwrap_or(0.0);
                (m, c)
            } else {
                let m = child_style
                    .flex_basis
                    .resolve(inner_avail_h)
                    .or(child_h)
                    .unwrap_or(0.0);
                let c = child_w.unwrap_or(0.0);
                (m, c)
            };

            total_grow += child_style.flex_grow;
            total_shrink += child_style.flex_shrink;
            base_main_sum += main_size;
            max_cross = max_cross.max(cross_size);

            child_sizes.push((main_size, cross_size, child_style));
        }

        if !children_ids.is_empty() {
            base_main_sum += gap * (children_ids.len() - 1) as f32;
        }

        let mut final_main_sizes = Vec::new();
        if let Some(target_main) = main_avail {
            let diff = target_main - base_main_sum;
            for (m, _, c_style) in &child_sizes {
                if diff > 0.0 && total_grow > 0.0 {
                    let grow_share = diff * (c_style.flex_grow / total_grow);
                    final_main_sizes.push(m + grow_share);
                } else if diff < 0.0 && total_shrink > 0.0 {
                    let shrink_share = diff.abs() * (c_style.flex_shrink / total_shrink);
                    final_main_sizes.push((m - shrink_share).max(0.0));
                } else {
                    final_main_sizes.push(*m);
                }
            }
        } else {
            for (m, _, _) in &child_sizes {
                final_main_sizes.push(*m);
            }
        }

        let computed_main_total: f32 = final_main_sizes.iter().sum::<f32>()
            + if !final_main_sizes.is_empty() {
                gap * (final_main_sizes.len() - 1) as f32
            } else {
                0.0
            };

        let resolved_content_w = if is_row {
            inner_avail_w.unwrap_or(computed_main_total)
        } else {
            inner_avail_w.unwrap_or(max_cross)
        };

        let resolved_content_h = if is_row {
            inner_avail_h.unwrap_or(max_cross)
        } else {
            inner_avail_h.unwrap_or(computed_main_total)
        };

        let content_main_size = if is_row {
            resolved_content_w
        } else {
            resolved_content_h
        };

        let content_cross_size = if is_row {
            resolved_content_h
        } else {
            resolved_content_w
        };

        let free_space = (content_main_size - computed_main_total).max(0.0);
        let num_children = children_ids.len();

        let (mut current_main_offset, space_between_items) = match style.justify_content {
            JustifyContent::FlexStart => (0.0, gap),
            JustifyContent::FlexEnd => (free_space, gap),
            JustifyContent::Center => (free_space / 2.0, gap),
            JustifyContent::SpaceBetween => {
                if num_children > 1 {
                    (0.0, gap + free_space / (num_children - 1) as f32)
                } else {
                    (0.0, gap)
                }
            }
            JustifyContent::SpaceAround => {
                if num_children > 0 {
                    let share = free_space / num_children as f32;
                    (share / 2.0, gap + share)
                } else {
                    (0.0, gap)
                }
            }
            JustifyContent::SpaceEvenly => {
                if num_children > 0 {
                    let share = free_space / (num_children + 1) as f32;
                    (share, gap + share)
                } else {
                    (0.0, gap)
                }
            }
        };

        for (i, &child_id) in children_ids.iter().enumerate() {
            let child_main = final_main_sizes[i];
            let (_, child_cross, _) = child_sizes[i];

            let resolved_child_cross = match style.align_items {
                AlignItems::Stretch => content_cross_size,
                _ => child_cross,
            };

            let cross_offset = match style.align_items {
                AlignItems::FlexStart | AlignItems::Stretch => 0.0,
                AlignItems::FlexEnd => (content_cross_size - resolved_child_cross).max(0.0),
                AlignItems::Center => ((content_cross_size - resolved_child_cross) / 2.0).max(0.0),
            };

            let (cx, cy, cw, ch) = if is_row {
                (
                    content_origin_x + current_main_offset,
                    content_origin_y + cross_offset,
                    child_main,
                    resolved_child_cross,
                )
            } else {
                (
                    content_origin_x + cross_offset,
                    content_origin_y + current_main_offset,
                    resolved_child_cross,
                    child_main,
                )
            };

            self.layout_node(child_id, cx, cy, Some(cw), Some(ch));

            current_main_offset += child_main + space_between_items;
        }

        let total_w = resolved_content_w + style.padding.horizontal();
        let total_h = resolved_content_h + style.padding.vertical();

        let final_w = explicit_w
            .or(parent_w)
            .unwrap_or(total_w)
            .max(style.min_width.unwrap_or(0.0));
        let final_h = explicit_h
            .or(parent_h)
            .unwrap_or(total_h)
            .max(style.min_height.unwrap_or(0.0));

        let rect = LayoutRect::new(origin_x, origin_y, final_w, final_h);
        self.nodes[node_id.0].as_mut().unwrap().computed_rect = rect;

        (final_w, final_h)
    }

    fn measure_intrinsic(
        &self,
        node_id: LayoutNodeId,
        avail_w: Option<f32>,
        avail_h: Option<f32>,
    ) -> (f32, f32) {
        let n = match self.nodes.get(node_id.0).and_then(|n| n.as_ref()) {
            Some(n) => n,
            None => return (0.0, 0.0),
        };
        let is_row = n.style.direction.is_row();
        let gap = n.style.gap;
        let mut main_total = 0.0f32;
        let mut max_cross = 0.0f32;

        for &cid in &n.children {
            let (cw, ch) = if let Some(child_node) = self.nodes.get(cid.0).and_then(|cn| cn.as_ref()) {
                let measured = if let Some(m) = &child_node.measure {
                    Some(m(avail_w, avail_h))
                } else if !child_node.children.is_empty() {
                    Some(self.measure_intrinsic(cid, avail_w, avail_h))
                } else {
                    None
                };
                let w = child_node
                    .style
                    .width
                    .resolve(avail_w)
                    .or(measured.map(|(w, _)| w))
                    .unwrap_or(0.0);
                let h = child_node
                    .style
                    .height
                    .resolve(avail_h)
                    .or(measured.map(|(_, h)| h))
                    .unwrap_or(0.0);
                (w, h)
            } else {
                (0.0, 0.0)
            };

            let (m, c) = if is_row { (cw, ch) } else { (ch, cw) };
            main_total += m;
            max_cross = max_cross.max(c);
        }

        if !n.children.is_empty() {
            main_total += gap * (n.children.len() - 1) as f32;
        }

        let pad_h = n.style.padding.horizontal();
        let pad_v = n.style.padding.vertical();

        if is_row {
            (main_total + pad_h, max_cross + pad_v)
        } else {
            (max_cross + pad_h, main_total + pad_v)
        }
    }
}

impl Default for LayoutEngine {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_flex_row_layout_with_gap() {
        let mut engine = LayoutEngine::new();

        let root_style = FlexStyle {
            direction: FlexDirection::Row,
            width: Dimension::Points(300.0),
            height: Dimension::Points(100.0),
            gap: 10.0,
            padding: EdgeInsets::all(10.0),
            ..Default::default()
        };
        let root = engine.create_node(root_style);

        let c1_style = FlexStyle {
            width: Dimension::Points(50.0),
            height: Dimension::Points(40.0),
            ..Default::default()
        };
        let c1 = engine.create_node(c1_style);

        let c2_style = FlexStyle {
            width: Dimension::Points(50.0),
            height: Dimension::Points(40.0),
            ..Default::default()
        };
        let c2 = engine.create_node(c2_style);

        engine.add_child(root, c1);
        engine.add_child(root, c2);

        engine.compute(root, 300.0, 100.0);

        let r1 = engine.get_rect(c1);
        let r2 = engine.get_rect(c2);

        assert_eq!(r1.x, 10.0);
        assert_eq!(r1.y, 10.0);
        assert_eq!(r1.width, 50.0);

        assert_eq!(r2.x, 70.0);
        assert_eq!(r2.y, 10.0);
        assert_eq!(r2.width, 50.0);
    }

    #[test]
    fn test_flex_grow_distribution() {
        let mut engine = LayoutEngine::new();

        let root_style = FlexStyle {
            direction: FlexDirection::Row,
            width: Dimension::Points(200.0),
            height: Dimension::Points(50.0),
            ..Default::default()
        };
        let root = engine.create_node(root_style);

        let c1 = engine.create_node(FlexStyle {
            flex_grow: 1.0,
            ..Default::default()
        });
        let c2 = engine.create_node(FlexStyle {
            flex_grow: 3.0,
            ..Default::default()
        });

        engine.add_child(root, c1);
        engine.add_child(root, c2);

        engine.compute(root, 200.0, 50.0);

        let r1 = engine.get_rect(c1);
        let r2 = engine.get_rect(c2);

        assert_eq!(r1.width, 50.0);
        assert_eq!(r2.width, 150.0);
        assert_eq!(r2.x, 50.0);
    }
}
