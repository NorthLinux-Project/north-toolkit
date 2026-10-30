//! SVG path specification, parser (M, L, H, V, C, S, Q, T, A, Z), and tiny-skia path builder.

use tiny_skia::{Path, PathBuilder};

/// Fundamental drawing operations composing a 2D vector path.
#[derive(Debug, Clone, PartialEq)]
pub enum PathVerb {
    /// Moves current point without drawing.
    MoveTo(f32, f32),
    /// Draws a straight line to `(x, y)`.
    LineTo(f32, f32),
    /// Draws a quadratic bezier curve with control point `(x1, y1)` to `(x, y)`.
    QuadTo(f32, f32, f32, f32),
    /// Draws a cubic bezier curve with control points `(x1, y1)` and `(x2, y2)` to `(x, y)`.
    CubicTo(f32, f32, f32, f32, f32, f32),
    /// Closes the current contour with a straight line back to start.
    Close,
}

/// A 2D resolution-independent vector path with optional SVG viewBox.
#[derive(Debug, Clone, Default, PartialEq)]
pub struct VectorPath {
    /// Sequence of drawing verbs.
    pub verbs: Vec<PathVerb>,
    /// Optional viewBox `[min_x, min_y, width, height]`.
    pub view_box: Option<[f32; 4]>,
}

impl VectorPath {
    /// Creates an empty vector path.
    pub fn new() -> Self {
        Self {
            verbs: Vec::new(),
            view_box: None,
        }
    }

    /// Sets the viewBox rectangle `[min_x, min_y, width, height]`.
    pub fn with_view_box(mut self, min_x: f32, min_y: f32, width: f32, height: f32) -> Self {
        self.view_box = Some([min_x, min_y, width, height]);
        self
    }

    /// Appends a `MoveTo` verb.
    pub fn move_to(&mut self, x: f32, y: f32) {
        self.verbs.push(PathVerb::MoveTo(x, y));
    }

    /// Appends a `LineTo` verb.
    pub fn line_to(&mut self, x: f32, y: f32) {
        self.verbs.push(PathVerb::LineTo(x, y));
    }

    /// Appends a `QuadTo` quadratic bezier curve verb.
    pub fn quad_to(&mut self, x1: f32, y1: f32, x: f32, y: f32) {
        self.verbs.push(PathVerb::QuadTo(x1, y1, x, y));
    }

    /// Appends a `CubicTo` cubic bezier curve verb.
    pub fn cubic_to(&mut self, x1: f32, y1: f32, x2: f32, y2: f32, x: f32, y: f32) {
        self.verbs.push(PathVerb::CubicTo(x1, y1, x2, y2, x, y));
    }

    /// Closes the active subpath.
    pub fn close(&mut self) {
        self.verbs.push(PathVerb::Close);
    }

    /// Parses an SVG path data string (`d="..."`) into a `VectorPath`.
    pub fn parse_svg(d: &str) -> Self {
        let mut path = Self::new();
        let bytes = d.as_bytes();
        let mut i = 0;
        let len = bytes.len();

        let mut curr_x = 0.0f32;
        let mut curr_y = 0.0f32;
        let mut start_x = 0.0f32;
        let mut start_y = 0.0f32;
        let mut last_ctrl_x = 0.0f32;
        let mut last_ctrl_y = 0.0f32;
        let mut last_verb_was_cubic = false;
        let mut last_verb_was_quad = false;

        let skip_whitespace_and_commas = |idx: &mut usize| {
            while *idx < len {
                let b = bytes[*idx];
                if b == b' ' || b == b'\t' || b == b'\n' || b == b'\r' || b == b',' {
                    *idx += 1;
                } else {
                    break;
                }
            }
        };

        let parse_number = |idx: &mut usize| -> Option<f32> {
            skip_whitespace_and_commas(idx);
            if *idx >= len {
                return None;
            }
            let start = *idx;
            if bytes[*idx] == b'+' || bytes[*idx] == b'-' {
                *idx += 1;
            }
            let mut has_digits = false;
            let mut has_dot = false;
            while *idx < len {
                let b = bytes[*idx];
                if b.is_ascii_digit() {
                    has_digits = true;
                    *idx += 1;
                } else if b == b'.' && !has_dot {
                    has_dot = true;
                    *idx += 1;
                } else {
                    break;
                }
            }
            if *idx < len && (bytes[*idx] == b'e' || bytes[*idx] == b'E') {
                *idx += 1;
                if *idx < len && (bytes[*idx] == b'+' || bytes[*idx] == b'-') {
                    *idx += 1;
                }
                while *idx < len && bytes[*idx].is_ascii_digit() {
                    *idx += 1;
                }
            }
            if !has_digits {
                return None;
            }
            std::str::from_utf8(&bytes[start..*idx])
                .ok()
                .and_then(|s| s.parse::<f32>().ok())
        };

        let parse_flag = |idx: &mut usize| -> Option<bool> {
            skip_whitespace_and_commas(idx);
            if *idx >= len {
                return None;
            }
            let b = bytes[*idx];
            if b == b'0' {
                *idx += 1;
                Some(false)
            } else if b == b'1' {
                *idx += 1;
                Some(true)
            } else {
                None
            }
        };

        let mut current_cmd = b' ';

        while i < len {
            skip_whitespace_and_commas(&mut i);
            if i >= len {
                break;
            }
            let b = bytes[i];
            if b.is_ascii_alphabetic() {
                current_cmd = b;
                i += 1;
            }

            match current_cmd {
                b'M' | b'm' => {
                    let is_rel = current_cmd == b'm';
                    if let (Some(x), Some(y)) = (parse_number(&mut i), parse_number(&mut i)) {
                        let nx = if is_rel { curr_x + x } else { x };
                        let ny = if is_rel { curr_y + y } else { y };
                        path.move_to(nx, ny);
                        curr_x = nx;
                        curr_y = ny;
                        start_x = nx;
                        start_y = ny;
                        last_verb_was_cubic = false;
                        last_verb_was_quad = false;

                        // Subsequent coordinates are treated as implicit LineTo
                        current_cmd = if is_rel { b'l' } else { b'L' };
                    }
                }
                b'L' | b'l' => {
                    let is_rel = current_cmd == b'l';
                    while let (Some(x), Some(y)) = (parse_number(&mut i), parse_number(&mut i)) {
                        let nx = if is_rel { curr_x + x } else { x };
                        let ny = if is_rel { curr_y + y } else { y };
                        path.line_to(nx, ny);
                        curr_x = nx;
                        curr_y = ny;
                        last_verb_was_cubic = false;
                        last_verb_was_quad = false;
                    }
                }
                b'H' | b'h' => {
                    let is_rel = current_cmd == b'h';
                    while let Some(x) = parse_number(&mut i) {
                        let nx = if is_rel { curr_x + x } else { x };
                        path.line_to(nx, curr_y);
                        curr_x = nx;
                        last_verb_was_cubic = false;
                        last_verb_was_quad = false;
                    }
                }
                b'V' | b'v' => {
                    let is_rel = current_cmd == b'v';
                    while let Some(y) = parse_number(&mut i) {
                        let ny = if is_rel { curr_y + y } else { y };
                        path.line_to(curr_x, ny);
                        curr_y = ny;
                        last_verb_was_cubic = false;
                        last_verb_was_quad = false;
                    }
                }
                b'C' | b'c' => {
                    let is_rel = current_cmd == b'c';
                    while let (Some(x1), Some(y1), Some(x2), Some(y2), Some(x), Some(y)) = (
                        parse_number(&mut i),
                        parse_number(&mut i),
                        parse_number(&mut i),
                        parse_number(&mut i),
                        parse_number(&mut i),
                        parse_number(&mut i),
                    ) {
                        let (cx1, cy1) = if is_rel { (curr_x + x1, curr_y + y1) } else { (x1, y1) };
                        let (cx2, cy2) = if is_rel { (curr_x + x2, curr_y + y2) } else { (x2, y2) };
                        let (nx, ny) = if is_rel { (curr_x + x, curr_y + y) } else { (x, y) };
                        path.cubic_to(cx1, cy1, cx2, cy2, nx, ny);
                        last_ctrl_x = cx2;
                        last_ctrl_y = cy2;
                        curr_x = nx;
                        curr_y = ny;
                        last_verb_was_cubic = true;
                        last_verb_was_quad = false;
                    }
                }
                b'S' | b's' => {
                    let is_rel = current_cmd == b's';
                    while let (Some(x2), Some(y2), Some(x), Some(y)) = (
                        parse_number(&mut i),
                        parse_number(&mut i),
                        parse_number(&mut i),
                        parse_number(&mut i),
                    ) {
                        let (cx1, cy1) = if last_verb_was_cubic {
                            (2.0 * curr_x - last_ctrl_x, 2.0 * curr_y - last_ctrl_y)
                        } else {
                            (curr_x, curr_y)
                        };
                        let (cx2, cy2) = if is_rel { (curr_x + x2, curr_y + y2) } else { (x2, y2) };
                        let (nx, ny) = if is_rel { (curr_x + x, curr_y + y) } else { (x, y) };
                        path.cubic_to(cx1, cy1, cx2, cy2, nx, ny);
                        last_ctrl_x = cx2;
                        last_ctrl_y = cy2;
                        curr_x = nx;
                        curr_y = ny;
                        last_verb_was_cubic = true;
                        last_verb_was_quad = false;
                    }
                }
                b'Q' | b'q' => {
                    let is_rel = current_cmd == b'q';
                    while let (Some(x1), Some(y1), Some(x), Some(y)) = (
                        parse_number(&mut i),
                        parse_number(&mut i),
                        parse_number(&mut i),
                        parse_number(&mut i),
                    ) {
                        let (cx1, cy1) = if is_rel { (curr_x + x1, curr_y + y1) } else { (x1, y1) };
                        let (nx, ny) = if is_rel { (curr_x + x, curr_y + y) } else { (x, y) };
                        path.quad_to(cx1, cy1, nx, ny);
                        last_ctrl_x = cx1;
                        last_ctrl_y = cy1;
                        curr_x = nx;
                        curr_y = ny;
                        last_verb_was_quad = true;
                        last_verb_was_cubic = false;
                    }
                }
                b'T' | b't' => {
                    let is_rel = current_cmd == b't';
                    while let (Some(x), Some(y)) = (parse_number(&mut i), parse_number(&mut i)) {
                        let (cx1, cy1) = if last_verb_was_quad {
                            (2.0 * curr_x - last_ctrl_x, 2.0 * curr_y - last_ctrl_y)
                        } else {
                            (curr_x, curr_y)
                        };
                        let (nx, ny) = if is_rel { (curr_x + x, curr_y + y) } else { (x, y) };
                        path.quad_to(cx1, cy1, nx, ny);
                        last_ctrl_x = cx1;
                        last_ctrl_y = cy1;
                        curr_x = nx;
                        curr_y = ny;
                        last_verb_was_quad = true;
                        last_verb_was_cubic = false;
                    }
                }
                b'A' | b'a' => {
                    let is_rel = current_cmd == b'a';
                    while let Some(rx) = parse_number(&mut i) {
                        let ry = match parse_number(&mut i) {
                            Some(v) => v,
                            None => break,
                        };
                        let x_axis_rot = match parse_number(&mut i) {
                            Some(v) => v,
                            None => break,
                        };
                        let large_arc = match parse_flag(&mut i) {
                            Some(v) => v,
                            None => break,
                        };
                        let sweep = match parse_flag(&mut i) {
                            Some(v) => v,
                            None => break,
                        };
                        let (target_x, target_y) = match (parse_number(&mut i), parse_number(&mut i)) {
                            (Some(x), Some(y)) => (x, y),
                            _ => break,
                        };

                        let nx = if is_rel { curr_x + target_x } else { target_x };
                        let ny = if is_rel { curr_y + target_y } else { target_y };

                        arc_to_cubics(
                            curr_x,
                            curr_y,
                            rx,
                            ry,
                            x_axis_rot,
                            large_arc,
                            sweep,
                            nx,
                            ny,
                            &mut path,
                        );

                        curr_x = nx;
                        curr_y = ny;
                        last_verb_was_cubic = true;
                        last_verb_was_quad = false;
                        last_ctrl_x = nx;
                        last_ctrl_y = ny;
                    }
                }
                b'Z' | b'z' => {
                    path.close();
                    curr_x = start_x;
                    curr_y = start_y;
                    last_verb_was_cubic = false;
                    last_verb_was_quad = false;
                }
                _ => {
                    i += 1;
                }
            }
        }

        path
    }

    /// Converts this vector path into a tiny-skia `Path` with scaling and translation applied.
    pub fn to_tiny_skia(&self, scale_x: f32, scale_y: f32, offset_x: f32, offset_y: f32) -> Option<Path> {
        let mut pb = PathBuilder::new();
        for verb in &self.verbs {
            match *verb {
                PathVerb::MoveTo(x, y) => {
                    pb.move_to(x * scale_x + offset_x, y * scale_y + offset_y);
                }
                PathVerb::LineTo(x, y) => {
                    pb.line_to(x * scale_x + offset_x, y * scale_y + offset_y);
                }
                PathVerb::QuadTo(x1, y1, x, y) => {
                    pb.quad_to(
                        x1 * scale_x + offset_x,
                        y1 * scale_y + offset_y,
                        x * scale_x + offset_x,
                        y * scale_y + offset_y,
                    );
                }
                PathVerb::CubicTo(x1, y1, x2, y2, x, y) => {
                    pb.cubic_to(
                        x1 * scale_x + offset_x,
                        y1 * scale_y + offset_y,
                        x2 * scale_x + offset_x,
                        y2 * scale_y + offset_y,
                        x * scale_x + offset_x,
                        y * scale_y + offset_y,
                    );
                }
                PathVerb::Close => {
                    pb.close();
                }
            }
        }
        pb.finish()
    }

    /// Computes the bounding box `[min_x, min_y, width, height]` of all control points.
    pub fn compute_bounds(&self) -> [f32; 4] {
        if let Some(vb) = self.view_box {
            return vb;
        }
        if self.verbs.is_empty() {
            return [0.0, 0.0, 0.0, 0.0];
        }
        let mut min_x = f32::INFINITY;
        let mut min_y = f32::INFINITY;
        let mut max_x = f32::NEG_INFINITY;
        let mut max_y = f32::NEG_INFINITY;

        let mut expand = |x: f32, y: f32| {
            if x < min_x { min_x = x; }
            if x > max_x { max_x = x; }
            if y < min_y { min_y = y; }
            if y > max_y { max_y = y; }
        };

        for verb in &self.verbs {
            match *verb {
                PathVerb::MoveTo(x, y) | PathVerb::LineTo(x, y) => expand(x, y),
                PathVerb::QuadTo(x1, y1, x, y) => {
                    expand(x1, y1);
                    expand(x, y);
                }
                PathVerb::CubicTo(x1, y1, x2, y2, x, y) => {
                    expand(x1, y1);
                    expand(x2, y2);
                    expand(x, y);
                }
                PathVerb::Close => {}
            }
        }

        if min_x.is_infinite() {
            [0.0, 0.0, 0.0, 0.0]
        } else {
            [min_x, min_y, (max_x - min_x).max(1.0), (max_y - min_y).max(1.0)]
        }
    }
}

fn arc_to_cubics(
    x1: f32,
    y1: f32,
    rx: f32,
    ry: f32,
    phi_deg: f32,
    large_arc: bool,
    sweep: bool,
    x2: f32,
    y2: f32,
    path: &mut VectorPath,
) {
    if (x1 - x2).abs() < 1e-6 && (y1 - y2).abs() < 1e-6 {
        return;
    }
    let rx = rx.abs();
    let ry = ry.abs();
    if rx < 1e-6 || ry < 1e-6 {
        path.line_to(x2, y2);
        return;
    }

    let phi = phi_deg.to_radians();
    let cos_phi = phi.cos();
    let sin_phi = phi.sin();

    let dx = (x1 - x2) * 0.5;
    let dy = (y1 - y2) * 0.5;
    let x1_prime = cos_phi * dx + sin_phi * dy;
    let y1_prime = -sin_phi * dx + cos_phi * dy;

    let mut rx = rx;
    let mut ry = ry;
    let mut rx_sq = rx * rx;
    let mut ry_sq = ry * ry;
    let x1_sq = x1_prime * x1_prime;
    let y1_sq = y1_prime * y1_prime;

    let radii_check = x1_sq / rx_sq + y1_sq / ry_sq;
    if radii_check > 1.0 {
        let scale = radii_check.sqrt();
        rx *= scale;
        ry *= scale;
        rx_sq = rx * rx;
        ry_sq = ry * ry;
    }

    let sign = if large_arc == sweep { -1.0 } else { 1.0 };
    let num = (rx_sq * ry_sq) - (rx_sq * y1_sq) - (ry_sq * x1_sq);
    let den = (rx_sq * y1_sq) + (ry_sq * x1_sq);
    let coef = sign * (num.max(0.0) / den.max(1e-6)).sqrt();
    let cx_prime = coef * ((rx * y1_prime) / ry);
    let cy_prime = coef * (-(ry * x1_prime) / rx);

    let cx = cos_phi * cx_prime - sin_phi * cy_prime + (x1 + x2) * 0.5;
    let cy = sin_phi * cx_prime + cos_phi * cy_prime + (y1 + y2) * 0.5;

    let ux = (x1_prime - cx_prime) / rx;
    let uy = (y1_prime - cy_prime) / ry;
    let vx = (-x1_prime - cx_prime) / rx;
    let vy = (-y1_prime - cy_prime) / ry;

    let angle = |u_x: f32, u_y: f32, v_x: f32, v_y: f32| -> f32 {
        let dot = (u_x * v_x + u_y * v_y).clamp(-1.0, 1.0);
        let s = if (u_x * v_y - u_y * v_x) < 0.0 { -1.0 } else { 1.0 };
        s * dot.acos()
    };

    let theta1 = angle(1.0, 0.0, ux, uy);
    let mut d_theta = angle(ux, uy, vx, vy);

    if !sweep && d_theta > 0.0 {
        d_theta -= std::f32::consts::TAU;
    } else if sweep && d_theta < 0.0 {
        d_theta += std::f32::consts::TAU;
    }

    let segments = (d_theta.abs() / (std::f32::consts::FRAC_PI_2)).ceil().max(1.0) as usize;
    let delta = d_theta / segments as f32;
    let t = (delta * 0.25).tan();
    let alpha = (delta.sin() * ((4.0 + 3.0 * t * t).sqrt() - 1.0)) / 3.0;

    let mut cur_theta = theta1;
    for _ in 0..segments {
        let next_theta = cur_theta + delta;

        let cos_cur = cur_theta.cos();
        let sin_cur = cur_theta.sin();
        let cos_next = next_theta.cos();
        let sin_next = next_theta.sin();

        let cp1_x = cos_cur - alpha * sin_cur;
        let cp1_y = sin_cur + alpha * cos_cur;
        let cp2_x = cos_next + alpha * sin_next;
        let cp2_y = sin_next - alpha * cos_next;

        let map_point = |px: f32, py: f32| -> (f32, f32) {
            let px_scaled = px * rx;
            let py_scaled = py * ry;
            (
                cos_phi * px_scaled - sin_phi * py_scaled + cx,
                sin_phi * px_scaled + cos_phi * py_scaled + cy,
            )
        };

        let (c1x, c1y) = map_point(cp1_x, cp1_y);
        let (c2x, c2y) = map_point(cp2_x, cp2_y);
        let (ex, ey) = map_point(cos_next, sin_next);

        path.cubic_to(c1x, c1y, c2x, c2y, ex, ey);
        cur_theta = next_theta;
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_svg_path_parser_basic() {
        let d = "M 10 20 L 30 40 H 50 V 60 Z";
        let path = VectorPath::parse_svg(d);
        assert_eq!(path.verbs.len(), 5);
        assert_eq!(path.verbs[0], PathVerb::MoveTo(10.0, 20.0));
        assert_eq!(path.verbs[1], PathVerb::LineTo(30.0, 40.0));
        assert_eq!(path.verbs[2], PathVerb::LineTo(50.0, 40.0));
        assert_eq!(path.verbs[3], PathVerb::LineTo(50.0, 60.0));
        assert_eq!(path.verbs[4], PathVerb::Close);
    }

    #[test]
    fn test_svg_path_parser_arc() {
        // Search icon circle part
        let d = "M11 19a8 8 0 100-16 8 8 0 000 16z";
        let path = VectorPath::parse_svg(d);
        assert!(!path.verbs.is_empty());
        assert_eq!(path.verbs[0], PathVerb::MoveTo(11.0, 19.0));
        let has_cubics = path.verbs.iter().any(|v| matches!(v, PathVerb::CubicTo(..)));
        assert!(has_cubics);
    }

    #[test]
    fn test_svg_path_parser_cubic_relative() {
        let d = "m 0 0 c 5 5 10 10 20 20 z";
        let path = VectorPath::parse_svg(d);
        assert_eq!(path.verbs.len(), 3);
        assert_eq!(path.verbs[0], PathVerb::MoveTo(0.0, 0.0));
        assert_eq!(path.verbs[1], PathVerb::CubicTo(5.0, 5.0, 10.0, 10.0, 20.0, 20.0));
        assert_eq!(path.verbs[2], PathVerb::Close);
    }
}
