//! Vulkan WSI swapchain creation, image view generation, and presentation synchronization.

use anyhow::{Context, Result};
use ash::khr::swapchain;
use ash::vk;

use super::context::VulkanContext;

/// Manages the Vulkan presentation swapchain and associated image views.
pub struct VulkanSwapchain {
    /// Vulkan swapchain device extension loader.
    pub swapchain_loader: swapchain::Device,
    /// Swapchain KHR handle.
    pub swapchain: vk::SwapchainKHR,
    /// Vector of swapchain presentable images.
    pub images: Vec<vk::Image>,
    /// Image views corresponding to each swapchain image.
    pub image_views: Vec<vk::ImageView>,
    /// Color format chosen for the swapchain.
    pub format: vk::Format,
    /// Physical resolution extent of swapchain images.
    pub extent: vk::Extent2D,
}

impl VulkanSwapchain {
    /// Creates a new presentation swapchain conforming to surface capabilities and target dimensions.
    pub fn new(
        context: &VulkanContext,
        surface_loader: &ash::khr::surface::Instance,
        surface: vk::SurfaceKHR,
        width: u32,
        height: u32,
    ) -> Result<Self> {
        let swapchain_loader = swapchain::Device::new(&context.instance, &context.device);

        let capabilities = unsafe {
            surface_loader.get_physical_device_surface_capabilities(context.physical_device, surface)?
        };

        let formats = unsafe {
            surface_loader.get_physical_device_surface_formats(context.physical_device, surface)?
        };

        let format = formats
            .iter()
            .cloned()
            .find(|f| {
                (f.format == vk::Format::B8G8R8A8_SRGB || f.format == vk::Format::R8G8B8A8_SRGB)
                    && f.color_space == vk::ColorSpaceKHR::SRGB_NONLINEAR
            })
            .or_else(|| {
                formats.iter().cloned().find(|f| {
                    (f.format == vk::Format::B8G8R8A8_UNORM || f.format == vk::Format::R8G8B8A8_UNORM)
                        && f.color_space == vk::ColorSpaceKHR::SRGB_NONLINEAR
                })
            })
            .unwrap_or(formats[0]);

        let mut image_count = capabilities.min_image_count + 1;
        if capabilities.max_image_count > 0 && image_count > capabilities.max_image_count {
            image_count = capabilities.max_image_count;
        }

        let extent = if capabilities.current_extent.width != u32::MAX {
            capabilities.current_extent
        } else {
            vk::Extent2D {
                width: width.clamp(
                    capabilities.min_image_extent.width,
                    capabilities.max_image_extent.width,
                ),
                height: height.clamp(
                    capabilities.min_image_extent.height,
                    capabilities.max_image_extent.height,
                ),
            }
        };

        let swapchain_info = vk::SwapchainCreateInfoKHR::default()
            .surface(surface)
            .min_image_count(image_count)
            .image_format(format.format)
            .image_color_space(format.color_space)
            .image_extent(extent)
            .image_array_layers(1)
            .image_usage(vk::ImageUsageFlags::COLOR_ATTACHMENT | vk::ImageUsageFlags::TRANSFER_DST)
            .image_sharing_mode(vk::SharingMode::EXCLUSIVE)
            .pre_transform(capabilities.current_transform)
            .composite_alpha(vk::CompositeAlphaFlagsKHR::OPAQUE)
            .present_mode(vk::PresentModeKHR::FIFO)
            .clipped(true);

        let swapchain = unsafe { swapchain_loader.create_swapchain(&swapchain_info, None) }
            .context("Failed to create Vulkan swapchain")?;

        let images = unsafe { swapchain_loader.get_swapchain_images(swapchain) }?;

        let image_views = images
            .iter()
            .map(|&img| {
                let view_info = vk::ImageViewCreateInfo::default()
                    .image(img)
                    .view_type(vk::ImageViewType::TYPE_2D)
                    .format(format.format)
                    .components(vk::ComponentMapping::default())
                    .subresource_range(vk::ImageSubresourceRange {
                        aspect_mask: vk::ImageAspectFlags::COLOR,
                        base_mip_level: 0,
                        level_count: 1,
                        base_array_layer: 0,
                        layer_count: 1,
                    });
                unsafe { context.device.create_image_view(&view_info, None) }
            })
            .collect::<Result<Vec<_>, _>>()
            .context("Failed to create swapchain image views")?;

        Ok(Self {
            swapchain_loader,
            swapchain,
            images,
            image_views,
            format: format.format,
            extent,
        })
    }

    /// Acquires the next presentable image index, signaling `semaphore` when ready.
    pub fn acquire_next_image(&self, semaphore: vk::Semaphore) -> Result<(u32, bool)> {
        let (idx, suboptimal) = unsafe {
            self.swapchain_loader
                .acquire_next_image(self.swapchain, u64::MAX, semaphore, vk::Fence::null())?
        };
        Ok((idx, suboptimal))
    }

    /// Destroys all swapchain image views and the swapchain handle.
    pub fn destroy(&self, device: &ash::Device) {
        unsafe {
            for &view in &self.image_views {
                device.destroy_image_view(view, None);
            }
            self.swapchain_loader.destroy_swapchain(self.swapchain, None);
        }
    }
}
