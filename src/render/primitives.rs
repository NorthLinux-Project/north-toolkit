#[derive(Debug, Clone, Copy, PartialEq)]
pub struct Border {
    pub color: [f32; 4],
    pub width: f32,
}

impl Border {
    pub fn new(color: [f32; 4], width: f32) -> Self {
        Self { color, width }
    }
}

#[derive(Debug, Clone, Copy, PartialEq)]
pub struct CornerRadii {
    pub top_left: f32,
    pub top_right: f32,
    pub bottom_right: f32,
    pub bottom_left: f32,
}

impl CornerRadii {
    pub const fn new(top_left: f32, top_right: f32, bottom_right: f32, bottom_left: f32) -> Self {
        Self {
            top_left,
            top_right,
            bottom_right,
            bottom_left,
        }
    }

    pub const fn all(radius: f32) -> Self {
        Self {
            top_left: radius,
            top_right: radius,
            bottom_right: radius,
            bottom_left: radius,
        }
    }

    pub fn scale(self, factor: f32) -> Self {
        Self {
            top_left: self.top_left * factor,
            top_right: self.top_right * factor,
            bottom_right: self.bottom_right * factor,
            bottom_left: self.bottom_left * factor,
        }
    }

    pub const fn to_array(self) -> [f32; 4] {
        [
            self.top_left,
            self.top_right,
            self.bottom_right,
            self.bottom_left,
        ]
    }
}

impl From<f32> for CornerRadii {
    fn from(radius: f32) -> Self {
        Self::all(radius)
    }
}

impl From<[f32; 4]> for CornerRadii {
    fn from(radii: [f32; 4]) -> Self {
        Self {
            top_left: radii[0],
            top_right: radii[1],
            bottom_right: radii[2],
            bottom_left: radii[3],
        }
    }
}

impl Default for CornerRadii {
    fn default() -> Self {
        Self::all(0.0)
    }
}

#[derive(Debug, Clone, PartialEq)]
pub enum RenderPrimitive {
    Rect {
        rect: [f32; 4],
        color: [f32; 4],
        radii: CornerRadii,
        border: Option<Border>,
        uv_rect: [f32; 4],
        use_texture: bool,
    },
    Glyph {
        page: u8,
        screen_rect: [f32; 4],
        uv_rect: [f32; 4],
        color: [f32; 4],
    },
    ClipPush {
        rect: [f32; 4],
    },
    ClipPop,
}

impl RenderPrimitive {
    pub fn solid_rect(rect: [f32; 4], color: [f32; 4], radii: impl Into<CornerRadii>) -> Self {
        Self::Rect {
            rect,
            color,
            radii: radii.into(),
            border: None,
            uv_rect: [0.0, 0.0, 1.0, 1.0],
            use_texture: false,
        }
    }

    pub fn bordered_rect(
        rect: [f32; 4],
        color: [f32; 4],
        radii: impl Into<CornerRadii>,
        border: Border,
    ) -> Self {
        Self::Rect {
            rect,
            color,
            radii: radii.into(),
            border: Some(border),
            uv_rect: [0.0, 0.0, 1.0, 1.0],
            use_texture: false,
        }
    }

    pub fn textured_rect(rect: [f32; 4], uv_rect: [f32; 4], alpha: f32) -> Self {
        Self::Rect {
            rect,
            color: [1.0, 1.0, 1.0, alpha],
            radii: CornerRadii::all(0.0),
            border: None,
            uv_rect,
            use_texture: true,
        }
    }

    pub fn glyph(page: u8, screen_rect: [f32; 4], uv_rect: [f32; 4], color: [f32; 4]) -> Self {
        Self::Glyph {
            page,
            screen_rect,
            uv_rect,
            color,
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_corner_radii_conversions() {
        let from_scalar: CornerRadii = 8.0f32.into();
        assert_eq!(from_scalar.top_left, 8.0);
        assert_eq!(from_scalar.top_right, 8.0);
        assert_eq!(from_scalar.bottom_right, 8.0);
        assert_eq!(from_scalar.bottom_left, 8.0);
        assert_eq!(from_scalar.to_array(), [8.0, 8.0, 8.0, 8.0]);

        let from_array: CornerRadii = [4.0, 6.0, 8.0, 10.0].into();
        assert_eq!(from_array.top_left, 4.0);
        assert_eq!(from_array.top_right, 6.0);
        assert_eq!(from_array.bottom_right, 8.0);
        assert_eq!(from_array.bottom_left, 10.0);
        assert_eq!(from_array.scale(2.0).to_array(), [8.0, 12.0, 16.0, 20.0]);
    }

    #[test]
    fn test_squircle_primitives_emission() {
        let prim = RenderPrimitive::bordered_rect(
            [0.0, 0.0, 100.0, 50.0],
            [1.0, 1.0, 1.0, 1.0],
            [2.0, 4.0, 6.0, 8.0],
            Border::new([0.0, 0.0, 0.0, 1.0], 1.0),
        );

        if let RenderPrimitive::Rect { radii, border, .. } = prim {
            assert_eq!(radii.top_left, 2.0);
            assert_eq!(radii.top_right, 4.0);
            assert_eq!(radii.bottom_right, 6.0);
            assert_eq!(radii.bottom_left, 8.0);
            assert_eq!(border.unwrap().width, 1.0);
        } else {
            panic!("Expected RenderPrimitive::Rect");
        }
    }
}
