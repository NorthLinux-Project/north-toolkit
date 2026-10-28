use anyhow::{Context, Result};
use ash::vk;
use ash::Device;

use super::context::VulkanContext;

pub struct VulkanTexture {
    pub image: vk::Image,
    pub memory: vk::DeviceMemory,
    pub view: vk::ImageView,
    pub width: u32,
    pub height: u32,
    pub format: vk::Format,
}

impl VulkanTexture {
    pub fn new_2d(
        context: &VulkanContext,
        width: u32,
        height: u32,
        format: vk::Format,
        usage: vk::ImageUsageFlags,
    ) -> Result<Self> {
        let image_info = vk::ImageCreateInfo::default()
            .image_type(vk::ImageType::TYPE_2D)
            .extent(vk::Extent3D {
                width,
                height,
                depth: 1,
            })
            .mip_levels(1)
            .array_layers(1)
            .format(format)
            .tiling(vk::ImageTiling::OPTIMAL)
            .initial_layout(vk::ImageLayout::UNDEFINED)
            .usage(usage | vk::ImageUsageFlags::SAMPLED | vk::ImageUsageFlags::TRANSFER_DST)
            .sharing_mode(vk::SharingMode::EXCLUSIVE)
            .samples(vk::SampleCountFlags::TYPE_1);

        let image = unsafe { context.device.create_image(&image_info, None) }
            .context("Failed to create Vulkan image")?;

        let mem_requirements = unsafe { context.device.get_image_memory_requirements(image) };
        let memory_type_index = context.find_memory_type(
            mem_requirements.memory_type_bits,
            vk::MemoryPropertyFlags::DEVICE_LOCAL,
        )?;

        let alloc_info = vk::MemoryAllocateInfo::default()
            .allocation_size(mem_requirements.size)
            .memory_type_index(memory_type_index);

        let memory = unsafe { context.device.allocate_memory(&alloc_info, None) }
            .context("Failed to allocate image memory")?;

        unsafe {
            context.device.bind_image_memory(image, memory, 0)?;
        }

        let aspect_mask = vk::ImageAspectFlags::COLOR;
        let view_info = vk::ImageViewCreateInfo::default()
            .image(image)
            .view_type(vk::ImageViewType::TYPE_2D)
            .format(format)
            .subresource_range(vk::ImageSubresourceRange {
                aspect_mask,
                base_mip_level: 0,
                level_count: 1,
                base_array_layer: 0,
                layer_count: 1,
            });

        let view = unsafe { context.device.create_image_view(&view_info, None) }
            .context("Failed to create image view")?;

        Ok(Self {
            image,
            memory,
            view,
            width,
            height,
            format,
        })
    }

    pub fn upload_data(
        &self,
        context: &VulkanContext,
        command_pool: vk::CommandPool,
        data: &[u8],
    ) -> Result<()> {
        let buffer_size = data.len() as vk::DeviceSize;

        let buffer_info = vk::BufferCreateInfo::default()
            .size(buffer_size)
            .usage(vk::BufferUsageFlags::TRANSFER_SRC)
            .sharing_mode(vk::SharingMode::EXCLUSIVE);

        let staging_buffer = unsafe { context.device.create_buffer(&buffer_info, None) }
            .context("Failed to create staging buffer")?;

        let mem_requirements =
            unsafe { context.device.get_buffer_memory_requirements(staging_buffer) };
        let memory_type_index = context.find_memory_type(
            mem_requirements.memory_type_bits,
            vk::MemoryPropertyFlags::HOST_VISIBLE | vk::MemoryPropertyFlags::HOST_COHERENT,
        )?;

        let alloc_info = vk::MemoryAllocateInfo::default()
            .allocation_size(mem_requirements.size)
            .memory_type_index(memory_type_index);

        let staging_memory = unsafe { context.device.allocate_memory(&alloc_info, None) }
            .context("Failed to allocate staging memory")?;

        unsafe {
            context
                .device
                .bind_buffer_memory(staging_buffer, staging_memory, 0)?;

            let ptr = context.device.map_memory(
                staging_memory,
                0,
                buffer_size,
                vk::MemoryMapFlags::empty(),
            )?;
            std::ptr::copy_nonoverlapping(data.as_ptr(), ptr as *mut u8, data.len());
            context.device.unmap_memory(staging_memory);
        }

        let cmd_alloc = vk::CommandBufferAllocateInfo::default()
            .command_pool(command_pool)
            .level(vk::CommandBufferLevel::PRIMARY)
            .command_buffer_count(1);

        let cmd = unsafe { context.device.allocate_command_buffers(&cmd_alloc) }?[0];

        let begin_info = vk::CommandBufferBeginInfo::default()
            .flags(vk::CommandBufferUsageFlags::ONE_TIME_SUBMIT);

        unsafe {
            context.device.begin_command_buffer(cmd, &begin_info)?;

            let barrier_to_transfer = vk::ImageMemoryBarrier2::default()
                .src_stage_mask(vk::PipelineStageFlags2::TOP_OF_PIPE)
                .src_access_mask(vk::AccessFlags2::empty())
                .dst_stage_mask(vk::PipelineStageFlags2::TRANSFER)
                .dst_access_mask(vk::AccessFlags2::TRANSFER_WRITE)
                .old_layout(vk::ImageLayout::UNDEFINED)
                .new_layout(vk::ImageLayout::TRANSFER_DST_OPTIMAL)
                .image(self.image)
                .subresource_range(vk::ImageSubresourceRange {
                    aspect_mask: vk::ImageAspectFlags::COLOR,
                    base_mip_level: 0,
                    level_count: 1,
                    base_array_layer: 0,
                    layer_count: 1,
                });

            let dep_info = vk::DependencyInfo::default()
                .image_memory_barriers(std::slice::from_ref(&barrier_to_transfer));
            context.device.cmd_pipeline_barrier2(cmd, &dep_info);

            let region = vk::BufferImageCopy::default()
                .buffer_offset(0)
                .buffer_row_length(0)
                .buffer_image_height(0)
                .image_subresource(vk::ImageSubresourceLayers {
                    aspect_mask: vk::ImageAspectFlags::COLOR,
                    mip_level: 0,
                    base_array_layer: 0,
                    layer_count: 1,
                })
                .image_offset(vk::Offset3D { x: 0, y: 0, z: 0 })
                .image_extent(vk::Extent3D {
                    width: self.width,
                    height: self.height,
                    depth: 1,
                });

            context.device.cmd_copy_buffer_to_image(
                cmd,
                staging_buffer,
                self.image,
                vk::ImageLayout::TRANSFER_DST_OPTIMAL,
                &[region],
            );

            let barrier_to_shader = vk::ImageMemoryBarrier2::default()
                .src_stage_mask(vk::PipelineStageFlags2::TRANSFER)
                .src_access_mask(vk::AccessFlags2::TRANSFER_WRITE)
                .dst_stage_mask(vk::PipelineStageFlags2::FRAGMENT_SHADER)
                .dst_access_mask(vk::AccessFlags2::SHADER_READ)
                .old_layout(vk::ImageLayout::TRANSFER_DST_OPTIMAL)
                .new_layout(vk::ImageLayout::SHADER_READ_ONLY_OPTIMAL)
                .image(self.image)
                .subresource_range(vk::ImageSubresourceRange {
                    aspect_mask: vk::ImageAspectFlags::COLOR,
                    base_mip_level: 0,
                    level_count: 1,
                    base_array_layer: 0,
                    layer_count: 1,
                });

            let dep_info_shader = vk::DependencyInfo::default()
                .image_memory_barriers(std::slice::from_ref(&barrier_to_shader));
            context.device.cmd_pipeline_barrier2(cmd, &dep_info_shader);

            context.device.end_command_buffer(cmd)?;

            let submit_cmd = [cmd];
            let submit_info = vk::SubmitInfo::default().command_buffers(&submit_cmd);
            context
                .device
                .queue_submit(context.graphics_queue, &[submit_info], vk::Fence::null())?;
            context.device.queue_wait_idle(context.graphics_queue)?;

            context
                .device
                .free_command_buffers(command_pool, &[cmd]);
            context.device.destroy_buffer(staging_buffer, None);
            context.device.free_memory(staging_memory, None);
        }

        Ok(())
    }

    pub fn destroy(&self, device: &Device) {
        unsafe {
            device.destroy_image_view(self.view, None);
            device.destroy_image(self.image, None);
            device.free_memory(self.memory, None);
        }
    }
}
