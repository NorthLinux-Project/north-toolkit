//! Vulkan 1.3 physical device discovery, instance creation, queue allocation, and memory types.

use std::ffi::{CStr, CString};
use std::sync::Arc;

use anyhow::{bail, Context, Result};
use ash::vk;
use ash::{Device, Entry, Instance};
use raw_window_handle::RawDisplayHandle;
use tracing::info;

/// Vulkan 1.3 instance and logical device context.
pub struct VulkanContext {
    /// Vulkan function loader entry point.
    pub entry: Entry,
    /// Vulkan instance handle.
    pub instance: Instance,
    /// Selected physical GPU device handle.
    pub physical_device: vk::PhysicalDevice,
    /// Logical device handle wrapped in `Arc`.
    pub device: Arc<Device>,
    /// Primary graphics queue handle.
    pub graphics_queue: vk::Queue,
    /// Index of the graphics queue family.
    pub graphics_queue_family: u32,
    /// Properties of the chosen physical device.
    pub device_properties: vk::PhysicalDeviceProperties,
    /// Memory heaps and types of the physical device.
    pub memory_properties: vk::PhysicalDeviceMemoryProperties,
}

impl VulkanContext {
    /// Initializes Vulkan 1.3 with dynamic rendering and synchronization2.
    pub fn new(display_handle: Option<RawDisplayHandle>) -> Result<Self> {
        let entry = unsafe { Entry::load() }.context("Failed to load Vulkan entry point")?;

        let app_name = CString::new("north-toolkit")?;
        let engine_name = CString::new("NorthToolkitVulkan")?;
        let app_info = vk::ApplicationInfo::default()
            .application_name(&app_name)
            .application_version(vk::make_api_version(0, 0, 1, 0))
            .engine_name(&engine_name)
            .engine_version(vk::make_api_version(0, 0, 1, 0))
            .api_version(vk::API_VERSION_1_3);

        let mut instance_extensions = Vec::new();
        if let Some(handle) = display_handle {
            let required = ash_window::enumerate_required_extensions(handle)
                .context("Failed to enumerate required window surface extensions")?;
            for &ext in required {
                instance_extensions.push(ext);
            }
        }

        let instance_create_info = vk::InstanceCreateInfo::default()
            .application_info(&app_info)
            .enabled_extension_names(&instance_extensions);

        let instance = unsafe { entry.create_instance(&instance_create_info, None) }
            .context("Failed to create Vulkan instance")?;

        let physical_devices = unsafe { instance.enumerate_physical_devices() }
            .context("Failed to enumerate physical devices")?;
        if physical_devices.is_empty() {
            bail!("No Vulkan-compatible physical devices found");
        }

        let (physical_device, properties) = select_physical_device(&instance, &physical_devices)?;
        let device_name = unsafe { CStr::from_ptr(properties.device_name.as_ptr()) }
            .to_string_lossy();
        info!(
            device = %device_name,
            api_version = format!(
                "{}.{}.{}",
                vk::api_version_major(properties.api_version),
                vk::api_version_minor(properties.api_version),
                vk::api_version_patch(properties.api_version)
            ),
            "Initialized Vulkan device in north-toolkit"
        );

        let queue_family_properties =
            unsafe { instance.get_physical_device_queue_family_properties(physical_device) };
        let graphics_queue_family = queue_family_properties
            .iter()
            .position(|info| info.queue_flags.contains(vk::QueueFlags::GRAPHICS))
            .map(|idx| idx as u32)
            .context("Failed to find graphics queue family")?;

        let queue_priorities = [1.0f32];
        let queue_create_info = vk::DeviceQueueCreateInfo::default()
            .queue_family_index(graphics_queue_family)
            .queue_priorities(&queue_priorities);

        let supported_device_extensions =
            unsafe { instance.enumerate_device_extension_properties(physical_device) }
                .context("Failed to enumerate device extensions")?;

        let has_ext = |ext_name: &CStr| {
            supported_device_extensions.iter().any(|ext| {
                let name = unsafe { CStr::from_ptr(ext.extension_name.as_ptr()) };
                name == ext_name
            })
        };

        let mut desired_extensions: Vec<&'static CStr> = Vec::new();
        if display_handle.is_some() && has_ext(ash::khr::swapchain::NAME) {
            desired_extensions.push(ash::khr::swapchain::NAME);
        }

        let device_extension_ptrs: Vec<*const i8> = desired_extensions
            .iter()
            .map(|ext| ext.as_ptr())
            .collect();

        let mut dynamic_rendering_features = vk::PhysicalDeviceDynamicRenderingFeatures::default()
            .dynamic_rendering(true);
        let mut sync2_features = vk::PhysicalDeviceSynchronization2Features::default()
            .synchronization2(true);

        let queue_create_infos = [queue_create_info];
        let device_create_info = vk::DeviceCreateInfo::default()
            .queue_create_infos(&queue_create_infos)
            .enabled_extension_names(&device_extension_ptrs)
            .push_next(&mut dynamic_rendering_features)
            .push_next(&mut sync2_features);

        let device = unsafe { instance.create_device(physical_device, &device_create_info, None) }
            .context("Failed to create Vulkan logical device")?;

        let graphics_queue = unsafe { device.get_device_queue(graphics_queue_family, 0) };
        let memory_properties =
            unsafe { instance.get_physical_device_memory_properties(physical_device) };

        Ok(Self {
            entry,
            instance,
            physical_device,
            device: Arc::new(device),
            graphics_queue,
            graphics_queue_family,
            device_properties: properties,
            memory_properties,
        })
    }

    /// Finds a compatible memory type index satisfying given type filter and property flags.
    pub fn find_memory_type(
        &self,
        type_filter: u32,
        properties: vk::MemoryPropertyFlags,
    ) -> Result<u32> {
        for (i, memory_type) in self.memory_properties.memory_types.iter().enumerate() {
            if (type_filter & (1 << i)) != 0
                && memory_type.property_flags.contains(properties)
            {
                return Ok(i as u32);
            }
        }
        bail!("Failed to find suitable Vulkan memory type")
    }
}

impl Drop for VulkanContext {
    fn drop(&mut self) {
        unsafe {
            self.device.device_wait_idle().ok();
            self.device.destroy_device(None);
            self.instance.destroy_instance(None);
        }
    }
}

fn select_physical_device(
    instance: &Instance,
    devices: &[vk::PhysicalDevice],
) -> Result<(vk::PhysicalDevice, vk::PhysicalDeviceProperties)> {
    let mut best_device = None;
    let mut highest_score = -1i64;

    for &device in devices {
        let properties = unsafe { instance.get_physical_device_properties(device) };
        let mut score = 0i64;

        match properties.device_type {
            vk::PhysicalDeviceType::DISCRETE_GPU => score += 100_000,
            vk::PhysicalDeviceType::INTEGRATED_GPU => score += 50_000,
            vk::PhysicalDeviceType::VIRTUAL_GPU => score += 10_000,
            vk::PhysicalDeviceType::CPU => score += 1_000,
            _ => score += 500,
        }

        score += properties.limits.max_image_dimension2_d as i64;

        if score > highest_score {
            highest_score = score;
            best_device = Some((device, properties));
        }
    }

    best_device.context("Failed to find suitable physical device")
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_vulkan_headless_context() {
        let ctx = VulkanContext::new(None);
        assert!(ctx.is_ok(), "Headless Vulkan context failed: {:?}", ctx.err());
    }
}
