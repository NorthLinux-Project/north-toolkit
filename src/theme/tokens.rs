#[derive(Debug, Clone, Copy, PartialEq)]
pub struct ColorTokens {
    pub background: [f32; 4],
    pub surface: [f32; 4],
    pub surface_hover: [f32; 4],
    pub surface_active: [f32; 4],
    pub primary: [f32; 4],
    pub primary_hover: [f32; 4],
    pub on_primary: [f32; 4],
    pub text: [f32; 4],
    pub text_secondary: [f32; 4],
    pub border: [f32; 4],
    pub border_focused: [f32; 4],
    pub cursor: [f32; 4],
    pub selection: [f32; 4],
}

#[derive(Debug, Clone, Copy, PartialEq)]
pub struct SpacingTokens {
    pub xs: f32,
    pub sm: f32,
    pub md: f32,
    pub lg: f32,
    pub xl: f32,
}

impl Default for SpacingTokens {
    fn default() -> Self {
        Self {
            xs: 4.0,
            sm: 8.0,
            md: 16.0,
            lg: 24.0,
            xl: 32.0,
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq)]
pub struct TypographyTokens {
    pub font_size_caption: f32,
    pub font_size_body: f32,
    pub font_size_heading: f32,
    pub font_size_title: f32,
    pub line_height: f32,
}

impl Default for TypographyTokens {
    fn default() -> Self {
        Self {
            font_size_caption: 11.0,
            font_size_body: 14.0,
            font_size_heading: 18.0,
            font_size_title: 24.0,
            line_height: 1.4,
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq)]
pub struct Theme {
    pub colors: ColorTokens,
    pub spacing: SpacingTokens,
    pub typography: TypographyTokens,
    pub border_radius_sm: f32,
    pub border_radius_md: f32,
    pub border_radius_lg: f32,
}

impl Theme {
    pub fn dark() -> Self {
        Self {
            colors: ColorTokens {
                background: [0.09, 0.10, 0.13, 1.0],
                surface: [0.14, 0.16, 0.20, 1.0],
                surface_hover: [0.18, 0.21, 0.27, 1.0],
                surface_active: [0.22, 0.26, 0.33, 1.0],
                primary: [0.35, 0.55, 0.95, 1.0],
                primary_hover: [0.45, 0.65, 1.0, 1.0],
                on_primary: [1.0, 1.0, 1.0, 1.0],
                text: [0.94, 0.95, 0.96, 1.0],
                text_secondary: [0.65, 0.68, 0.74, 1.0],
                border: [0.22, 0.25, 0.31, 1.0],
                border_focused: [0.40, 0.60, 1.0, 1.0],
                cursor: [0.95, 0.95, 1.0, 1.0],
                selection: [0.20, 0.40, 0.80, 0.4],
            },
            spacing: SpacingTokens::default(),
            typography: TypographyTokens::default(),
            border_radius_sm: 4.0,
            border_radius_md: 8.0,
            border_radius_lg: 14.0,
        }
    }

    pub fn light() -> Self {
        Self {
            colors: ColorTokens {
                background: [0.98, 0.98, 0.99, 1.0],
                surface: [0.92, 0.93, 0.95, 1.0],
                surface_hover: [0.86, 0.88, 0.91, 1.0],
                surface_active: [0.80, 0.82, 0.86, 1.0],
                primary: [0.20, 0.45, 0.85, 1.0],
                primary_hover: [0.25, 0.52, 0.95, 1.0],
                on_primary: [1.0, 1.0, 1.0, 1.0],
                text: [0.12, 0.13, 0.15, 1.0],
                text_secondary: [0.45, 0.48, 0.53, 1.0],
                border: [0.80, 0.82, 0.85, 1.0],
                border_focused: [0.25, 0.52, 0.95, 1.0],
                cursor: [0.15, 0.15, 0.15, 1.0],
                selection: [0.30, 0.55, 0.95, 0.3],
            },
            spacing: SpacingTokens::default(),
            typography: TypographyTokens::default(),
            border_radius_sm: 4.0,
            border_radius_md: 8.0,
            border_radius_lg: 14.0,
        }
    }
}

impl Default for Theme {
    fn default() -> Self {
        Self::dark()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_theme_defaults() {
        let dark = Theme::dark();
        assert_eq!(dark.spacing.sm, 8.0);
        assert_eq!(dark.border_radius_md, 8.0);
        assert_eq!(dark.colors.background[3], 1.0);

        let light = Theme::light();
        assert_ne!(dark.colors.background, light.colors.background);
    }
}
