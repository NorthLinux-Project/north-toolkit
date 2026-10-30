//! Vector graphics subsystem: SVG path parsing, tiny-skia rasterization, and dynamic texture atlas.

pub mod atlas;
pub mod path;
pub mod rasterizer;

pub use atlas::VectorAtlas;
pub use path::{PathVerb, VectorPath};
pub use rasterizer::{StrokeStyle, VectorRasterizer};
