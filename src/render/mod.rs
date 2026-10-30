//! Rendering primitives, dynamic atlases, SVG vector rasterization, and Vulkan 1.3 hardware pipeline.

pub mod atlas;
pub mod primitives;
pub mod vector;
pub mod vulkan;

pub use atlas::{AtlasRegion, GlyphAtlas};
pub use primitives::{Border, CornerRadii, RenderPrimitive};
pub use vector::{StrokeStyle, VectorAtlas, VectorPath, VectorRasterizer};
pub use vulkan::{
    FrameRenderer, GlyphPipeline, QuadPipeline, VulkanContext, VulkanSwapchain, VulkanTexture,
};
