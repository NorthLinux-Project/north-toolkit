//! Color palettes, component metrics, corner radii, and theme configurations.

/// Color token definitions in normalized RGBA format `[0.0, 1.0]`.
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct ColorTokens {
    /// Window and root container background color.
    pub background: [f32; 4],
    /// Default component surface color (cards, panels).
    pub surface: [f32; 4],
    /// Hovered state background color for interactive elements.
    pub surface_hover: [f32; 4],
    /// Pressed/active state background color.
    pub surface_active: [f32; 4],
    /// Primary brand accent color (buttons, active toggles).
    pub primary: [f32; 4],
    /// Hover state for primary accent elements.
    pub primary_hover: [f32; 4],
    /// Text and icon color rendered on top of primary accent backgrounds.
    pub on_primary: [f32; 4],
    /// Primary high-contrast text color.
    pub text: [f32; 4],
    /// Secondary medium-contrast text and placeholder color.
    pub text_secondary: [f32; 4],
    /// Default outline and separator border color.
    pub border: [f32; 4],
    /// Focused input and control outline border color.
    pub border_focused: [f32; 4],
    /// Text input caret cursor color.
    pub cursor: [f32; 4],
    /// Text selection highlight color.
    pub selection: [f32; 4],
}

/// Spacing and padding scale tokens in logical points.
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct SpacingTokens {
    /// Extra-small spacing (4.0 pt).
    pub xs: f32,
    /// Small spacing (8.0 pt).
    pub sm: f32,
    /// Medium spacing (16.0 pt).
    pub md: f32,
    /// Large spacing (24.0 pt).
    pub lg: f32,
    /// Extra-large spacing (32.0 pt).
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

/// Typography size and line-height scale tokens in logical points.
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct TypographyTokens {
    /// Caption and small annotation font size.
    pub font_size_caption: f32,
    /// Standard body text font size.
    pub font_size_body: f32,
    /// Section heading font size.
    pub font_size_heading: f32,
    /// Page title font size.
    pub font_size_title: f32,
    /// Standard proportional line height multiplier.
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

/// Active visual theme comprising color palette, spacing, typography, and border radii.
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct Theme {
    /// Palette of color tokens.
    pub colors: ColorTokens,
    /// Standard spacing tokens.
    pub spacing: SpacingTokens,
    /// Typography hierarchy tokens.
    pub typography: TypographyTokens,
    /// Small corner radius (4.0 pt).
    pub border_radius_sm: f32,
    /// Medium corner radius (8.0 pt).
    pub border_radius_md: f32,
    /// Large corner radius (14.0 pt).
    pub border_radius_lg: f32,
}

impl Theme {
    /// Creates the standard Obsidian dark mode theme.
    pub fn dark() -> Self {
        Self {
            colors: ColorTokens {
                background: [0.063, 0.063, 0.063, 1.0],
                surface: [0.094, 0.094, 0.094, 1.0],
                surface_hover: [0.133, 0.133, 0.133, 1.0],
                surface_active: [0.165, 0.165, 0.165, 1.0],
                primary: [0.20, 0.45, 0.90, 1.0],
                primary_hover: [0.26, 0.52, 0.96, 1.0],
                on_primary: [1.0, 1.0, 1.0, 1.0],
                text: [0.93, 0.93, 0.93, 1.0],
                text_secondary: [0.54, 0.54, 0.54, 1.0],
                border: [0.15, 0.15, 0.15, 1.0],
                border_focused: [0.23, 0.51, 0.96, 1.0],
                cursor: [0.93, 0.93, 0.93, 1.0],
                selection: [0.20, 0.45, 0.90, 0.35],
            },
            spacing: SpacingTokens::default(),
            typography: TypographyTokens::default(),
            border_radius_sm: 4.0,
            border_radius_md: 8.0,
            border_radius_lg: 14.0,
        }
    }

    /// Creates the clean light mode theme.
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
