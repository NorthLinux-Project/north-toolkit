//! Per-frame synchronization, double/triple buffering, dynamic rendering, and command recording.

use std::mem;

use anyhow::Result;
use ash::vk;

use super::context::VulkanContext;
use super::pipeline::{
    GlyphInstance, GlyphPipeline, GlyphPushConstants, QuadPipeline, QuadPushConstants,
};
use crate::render::primitives::RenderPrimitive;

/// Number of frames recorded and presented concurrently in flight.
pub const MAX_FRAMES_IN_FLIGHT: usize = 2;
/// Initial instance buffer capacity for text glyph quads.
pub const INITIAL_GLYPH_CAPACITY: usize = 8192;

/// Synchronization primitives and transient vertex buffers for a single frame in flight.
pub struct FrameSync {
    /// Command pool owning the frame's command buffer.
    pub command_pool: vk::CommandPool,
    /// Primary command buffer for recording render passes.
    pub command_buffer: vk::CommandBuffer,
    /// Semaphore signaled when swapchain image acquisition completes.
    pub image_available: vk::Semaphore,
    /// Semaphore signaled when rendering commands have completed on the GPU.
    pub render_finished: vk::Semaphore,
    /// Fence ensuring the CPU does not overwrite in-flight command buffers.
    pub fence: vk::Fence,
    /// Dynamic vertex buffer holding per-frame glyph instances.
    pub glyph_buffer: vk::Buffer,
    /// Host-visible device memory backing `glyph_buffer`.
    pub glyph_memory: vk::DeviceMemory,
    /// Maximum number of glyph instances that can be stored before reallocation.
    pub glyph_capacity: usize,
    /// Host mapped pointer to `glyph_memory`.
    pub glyph_mapped: *mut GlyphInstance,
}

unsafe impl Send for FrameSync {}
unsafe impl Sync for FrameSync {}

/// Double-buffered frame synchronizer and command recorder.
pub struct FrameRenderer {
    /// Array of per-frame synchronization objects.
    pub frames: [FrameSync; MAX_FRAMES_IN_FLIGHT],
    /// Index of the current active frame in flight (0 or 1).
    pub current_frame: usize,
    scissor_stack: Vec<vk::Rect2D>,
}

impl FrameRenderer {
    /// Initializes frame synchronization objects and dynamic glyph instance buffers.
    pub fn new(context: &VulkanContext) -> Result<Self> {
        let mut frames = Vec::with_capacity(MAX_FRAMES_IN_FLIGHT);

        for _ in 0..MAX_FRAMES_IN_FLIGHT {
            let pool_info = vk::CommandPoolCreateInfo::default()
                .queue_family_index(context.graphics_queue_family)
                .flags(vk::CommandPoolCreateFlags::RESET_COMMAND_BUFFER);

            let command_pool = unsafe { context.device.create_command_pool(&pool_info, None) }?;

            let cmd_info = vk::CommandBufferAllocateInfo::default()
                .command_pool(command_pool)
                .level(vk::CommandBufferLevel::PRIMARY)
                .command_buffer_count(1);

            let command_buffer =
                unsafe { context.device.allocate_command_buffers(&cmd_info) }?[0];

            let sem_info = vk::SemaphoreCreateInfo::default();
            let image_available = unsafe { context.device.create_semaphore(&sem_info, None) }?;
            let render_finished = unsafe { context.device.create_semaphore(&sem_info, None) }?;

            let fence_info =
                vk::FenceCreateInfo::default().flags(vk::FenceCreateFlags::SIGNALED);
            let fence = unsafe { context.device.create_fence(&fence_info, None) }?;

            let glyph_capacity = INITIAL_GLYPH_CAPACITY;
            let buffer_size =
                (glyph_capacity * mem::size_of::<GlyphInstance>()) as vk::DeviceSize;

            let buffer_info = vk::BufferCreateInfo::default()
                .size(buffer_size)
                .usage(vk::BufferUsageFlags::VERTEX_BUFFER)
                .sharing_mode(vk::SharingMode::EXCLUSIVE);

            let glyph_buffer = unsafe { context.device.create_buffer(&buffer_info, None) }?;
            let mem_req =
                unsafe { context.device.get_buffer_memory_requirements(glyph_buffer) };
            let mem_type = context.find_memory_type(
                mem_req.memory_type_bits,
                vk::MemoryPropertyFlags::HOST_VISIBLE | vk::MemoryPropertyFlags::HOST_COHERENT,
            )?;

            let alloc_info = vk::MemoryAllocateInfo::default()
                .allocation_size(mem_req.size)
                .memory_type_index(mem_type);

            let glyph_memory = unsafe { context.device.allocate_memory(&alloc_info, None) }?;
            unsafe {
                context
                    .device
                    .bind_buffer_memory(glyph_buffer, glyph_memory, 0)?;
            }

            let glyph_mapped = unsafe {
                context.device.map_memory(
                    glyph_memory,
                    0,
                    buffer_size,
                    vk::MemoryMapFlags::empty(),
                )? as *mut GlyphInstance
            };

            frames.push(FrameSync {
                command_pool,
                command_buffer,
                image_available,
                render_finished,
                fence,
                glyph_buffer,
                glyph_memory,
                glyph_capacity,
                glyph_mapped,
            });
        }

        let frames_arr = frames
            .try_into()
            .map_err(|_| anyhow::anyhow!("Failed frame sync conversion"))?;

        Ok(Self {
            frames: frames_arr,
            current_frame: 0,
            scissor_stack: Vec::new(),
        })
    }

    /// Returns a reference to the active frame's synchronization primitives.
    pub fn current_sync(&self) -> &FrameSync {
        &self.frames[self.current_frame]
    }

    /// Advances to the next frame in the double-buffer ring.
    pub fn advance_frame(&mut self) {
        self.current_frame = (self.current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    /// Records GPU draw calls for the supplied primitive batch into a command buffer.
    pub fn record_render_commands(
        &mut self,
        context: &VulkanContext,
        target_image: vk::Image,
        target_view: vk::ImageView,
        extent: vk::Extent2D,
        clear_color: [f32; 4],
        primitives: &[RenderPrimitive],
        popup_target: Option<(
            vk::Image,
            vk::ImageView,
            vk::Extent2D,
            [f32; 4],
            &[RenderPrimitive],
        )>,
        quad_pipeline: &QuadPipeline,
        glyph_pipeline: &GlyphPipeline,
    ) -> Result<vk::CommandBuffer> {
        let (cmd, fence, glyph_capacity, glyph_mapped, glyph_buffer) = {
            let sync = &self.frames[self.current_frame];
            (
                sync.command_buffer,
                sync.fence,
                sync.glyph_capacity,
                sync.glyph_mapped,
                sync.glyph_buffer,
            )
        };

        // SAFETY: fence must be a valid Vulkan fence handle. We wait for fence completion
        // before recording to ensure previous GPU execution on this frame's resources is finished.
        unsafe {
            context
                .device
                .wait_for_fences(&[fence], true, u64::MAX)?;
            context.device.reset_fences(&[fence])?;

            let begin_info = vk::CommandBufferBeginInfo::default()
                .flags(vk::CommandBufferUsageFlags::ONE_TIME_SUBMIT);
            context.device.begin_command_buffer(cmd, &begin_info)?;

            let mut glyph_write_offset = 0usize;

            self.record_surface_pass(
                context,
                cmd,
                glyph_capacity,
                glyph_mapped,
                glyph_buffer,
                target_image,
                target_view,
                extent,
                clear_color,
                primitives,
                quad_pipeline,
                glyph_pipeline,
                &mut glyph_write_offset,
            );

            if let Some((popup_image, popup_view, popup_extent, popup_clear, popup_prims)) =
                popup_target
            {
                self.record_surface_pass(
                    context,
                    cmd,
                    glyph_capacity,
                    glyph_mapped,
                    glyph_buffer,
                    popup_image,
                    popup_view,
                    popup_extent,
                    popup_clear,
                    popup_prims,
                    quad_pipeline,
                    glyph_pipeline,
                    &mut glyph_write_offset,
                );
            }

            context.device.end_command_buffer(cmd)?;
        }

        Ok(cmd)
    }

    #[allow(clippy::too_many_arguments)]
    unsafe fn record_surface_pass(
        &mut self,
        context: &VulkanContext,
        cmd: vk::CommandBuffer,
        glyph_capacity: usize,
        glyph_mapped: *mut GlyphInstance,
        glyph_buffer: vk::Buffer,
        target_image: vk::Image,
        target_view: vk::ImageView,
        extent: vk::Extent2D,
        clear_color: [f32; 4],
        primitives: &[RenderPrimitive],
        quad_pipeline: &QuadPipeline,
        glyph_pipeline: &GlyphPipeline,
        glyph_write_offset: &mut usize,
    ) {
        // SAFETY: Transitioning target_image into COLOR_ATTACHMENT_OPTIMAL via pipeline barrier 2
        unsafe {
            let barrier_to_color = vk::ImageMemoryBarrier2::default()
                .src_stage_mask(vk::PipelineStageFlags2::COLOR_ATTACHMENT_OUTPUT)
                .src_access_mask(vk::AccessFlags2::empty())
                .dst_stage_mask(vk::PipelineStageFlags2::COLOR_ATTACHMENT_OUTPUT)
                .dst_access_mask(vk::AccessFlags2::COLOR_ATTACHMENT_WRITE)
                .old_layout(vk::ImageLayout::UNDEFINED)
                .new_layout(vk::ImageLayout::COLOR_ATTACHMENT_OPTIMAL)
                .image(target_image)
                .subresource_range(vk::ImageSubresourceRange {
                    aspect_mask: vk::ImageAspectFlags::COLOR,
                    base_mip_level: 0,
                    level_count: 1,
                    base_array_layer: 0,
                    layer_count: 1,
                });

            let dep_info_color =
                vk::DependencyInfo::default().image_memory_barriers(std::slice::from_ref(&barrier_to_color));
            context.device.cmd_pipeline_barrier2(cmd, &dep_info_color);

            let clear_value = vk::ClearValue {
                color: vk::ClearColorValue {
                    float32: clear_color,
                },
            };

            let color_attachment = vk::RenderingAttachmentInfo::default()
                .image_view(target_view)
                .image_layout(vk::ImageLayout::COLOR_ATTACHMENT_OPTIMAL)
                .load_op(vk::AttachmentLoadOp::CLEAR)
                .store_op(vk::AttachmentStoreOp::STORE)
                .clear_value(clear_value);

            let rendering_info = vk::RenderingInfo::default()
                .render_area(vk::Rect2D {
                    offset: vk::Offset2D { x: 0, y: 0 },
                    extent,
                })
                .layer_count(1)
                .color_attachments(std::slice::from_ref(&color_attachment));

            context.device.cmd_begin_rendering(cmd, &rendering_info);

            let viewport = vk::Viewport {
                x: 0.0,
                y: 0.0,
                width: extent.width as f32,
                height: extent.height as f32,
                min_depth: 0.0,
                max_depth: 1.0,
            };
            context.device.cmd_set_viewport(cmd, 0, &[viewport]);

            let initial_scissor = vk::Rect2D {
                offset: vk::Offset2D { x: 0, y: 0 },
                extent,
            };
            context.device.cmd_set_scissor(cmd, 0, &[initial_scissor]);

            self.scissor_stack.clear();
            self.scissor_stack.push(initial_scissor);

            let proj = orthographic_matrix(extent.width as f32, extent.height as f32);

            let mut current_pipeline_mode = 0u8;
            let mut glyph_instances: Vec<GlyphInstance> = Vec::with_capacity(256);
            let mut pending_page: Option<u8> = None;

            let flush_glyphs = |page_opt: &mut Option<u8>,
                                instances: &mut Vec<GlyphInstance>,
                                offset: &mut usize| {
                if let Some(page) = page_opt.take() {
                    let count = instances.len();
                    if count > 0 && (*offset + count) <= glyph_capacity {
                        std::ptr::copy_nonoverlapping(
                            instances.as_ptr(),
                            glyph_mapped.add(*offset),
                            count,
                        );

                        context.device.cmd_bind_pipeline(
                            cmd,
                            vk::PipelineBindPoint::GRAPHICS,
                            glyph_pipeline.pipeline,
                        );

                        let page_idx =
                            (page as usize).min(glyph_pipeline.descriptor_sets.len().saturating_sub(1));
                        let sets = [glyph_pipeline.descriptor_sets[page_idx]];
                        context.device.cmd_bind_descriptor_sets(
                            cmd,
                            vk::PipelineBindPoint::GRAPHICS,
                            glyph_pipeline.pipeline_layout,
                            0,
                            &sets,
                            &[],
                        );

                        let buffers = [glyph_buffer];
                        let offsets = [(*offset * mem::size_of::<GlyphInstance>()) as vk::DeviceSize];
                        context
                            .device
                            .cmd_bind_vertex_buffers(cmd, 0, &buffers, &offsets);

                        let pc = GlyphPushConstants { projection: proj };
                        let pc_bytes = core::slice::from_raw_parts(
                            &pc as *const _ as *const u8,
                            mem::size_of::<GlyphPushConstants>(),
                        );
                        context.device.cmd_push_constants(
                            cmd,
                            glyph_pipeline.pipeline_layout,
                            vk::ShaderStageFlags::VERTEX,
                            0,
                            pc_bytes,
                        );

                        context.device.cmd_draw(cmd, 6, count as u32, 0, 0);
                        *offset += count;
                        instances.clear();
                    }
                }
            };

            for prim in primitives {
                match *prim {
                    RenderPrimitive::ClipPush { rect } => {
                        flush_glyphs(&mut pending_page, &mut glyph_instances, glyph_write_offset);

                        let current_scissor = self.scissor_stack.last().cloned().unwrap();
                        let min_x = (rect[0].max(0.0) as i32).max(current_scissor.offset.x);
                        let min_y = (rect[1].max(0.0) as i32).max(current_scissor.offset.y);
                        let max_x = ((rect[0] + rect[2]) as i32).min(
                            current_scissor.offset.x + current_scissor.extent.width as i32,
                        );
                        let max_y = ((rect[1] + rect[3]) as i32).min(
                            current_scissor.offset.y + current_scissor.extent.height as i32,
                        );

                        let w = (max_x - min_x).max(0) as u32;
                        let h = (max_y - min_y).max(0) as u32;

                        let new_scissor = vk::Rect2D {
                            offset: vk::Offset2D { x: min_x, y: min_y },
                            extent: vk::Extent2D {
                                width: w,
                                height: h,
                            },
                        };

                        self.scissor_stack.push(new_scissor);
                        context.device.cmd_set_scissor(cmd, 0, &[new_scissor]);
                        current_pipeline_mode = 0;
                    }
                    RenderPrimitive::ClipPop => {
                        flush_glyphs(&mut pending_page, &mut glyph_instances, glyph_write_offset);

                        if self.scissor_stack.len() > 1 {
                            self.scissor_stack.pop();
                            let scissor = self.scissor_stack.last().unwrap();
                            context.device.cmd_set_scissor(cmd, 0, &[*scissor]);
                        }
                        current_pipeline_mode = 0;
                    }
                    RenderPrimitive::Rect {
                        rect,
                        color,
                        radii,
                        border,
                        uv_rect,
                        use_texture,
                    } => {
                        flush_glyphs(&mut pending_page, &mut glyph_instances, glyph_write_offset);

                        if current_pipeline_mode != 1 {
                            context.device.cmd_bind_pipeline(
                                cmd,
                                vk::PipelineBindPoint::GRAPHICS,
                                quad_pipeline.pipeline,
                            );
                            let sets = [quad_pipeline.descriptor_set];
                            context.device.cmd_bind_descriptor_sets(
                                cmd,
                                vk::PipelineBindPoint::GRAPHICS,
                                quad_pipeline.pipeline_layout,
                                0,
                                &sets,
                                &[],
                            );
                            current_pipeline_mode = 1;
                        }

                        let (border_color, border_width) = match border {
                            Some(b) => (b.color, b.width),
                            None => ([0.0, 0.0, 0.0, 0.0], 0.0),
                        };

                        let pc = QuadPushConstants {
                            projection: [
                                2.0 / extent.width as f32,
                                2.0 / extent.height as f32,
                                -1.0,
                                -1.0,
                            ],
                            rect,
                            uv_rect,
                            color,
                            border_color,
                            radii: radii.to_array(),
                            params: [
                                border_width,
                                if use_texture { 1.0 } else { 0.0 },
                                1.0,
                                0.0,
                            ],
                        };

                        let pc_bytes = core::slice::from_raw_parts(
                            &pc as *const _ as *const u8,
                            mem::size_of::<QuadPushConstants>(),
                        );

                        context.device.cmd_push_constants(
                            cmd,
                            quad_pipeline.pipeline_layout,
                            vk::ShaderStageFlags::VERTEX,
                            0,
                            pc_bytes,
                        );

                        context.device.cmd_draw(cmd, 6, 1, 0, 0);
                    }
                    RenderPrimitive::Glyph {
                        page,
                        screen_rect,
                        uv_rect,
                        color,
                    } => {
                        if pending_page != Some(page) {
                            flush_glyphs(&mut pending_page, &mut glyph_instances, glyph_write_offset);
                            pending_page = Some(page);
                        }
                        glyph_instances.push(GlyphInstance {
                            rect: screen_rect,
                            uv_rect,
                            color,
                        });
                        current_pipeline_mode = 2;
                    }
                }
            }

            flush_glyphs(&mut pending_page, &mut glyph_instances, glyph_write_offset);

            context.device.cmd_end_rendering(cmd);

            let barrier_to_present = vk::ImageMemoryBarrier2::default()
                .src_stage_mask(vk::PipelineStageFlags2::COLOR_ATTACHMENT_OUTPUT)
                .src_access_mask(vk::AccessFlags2::COLOR_ATTACHMENT_WRITE)
                .dst_stage_mask(vk::PipelineStageFlags2::BOTTOM_OF_PIPE)
                .dst_access_mask(vk::AccessFlags2::empty())
                .old_layout(vk::ImageLayout::COLOR_ATTACHMENT_OPTIMAL)
                .new_layout(vk::ImageLayout::PRESENT_SRC_KHR)
                .image(target_image)
                .subresource_range(vk::ImageSubresourceRange {
                    aspect_mask: vk::ImageAspectFlags::COLOR,
                    base_mip_level: 0,
                    level_count: 1,
                    base_array_layer: 0,
                    layer_count: 1,
                });

            let dep_info_present =
                vk::DependencyInfo::default().image_memory_barriers(std::slice::from_ref(&barrier_to_present));
            context.device.cmd_pipeline_barrier2(cmd, &dep_info_present);
        }
    }

    /// Destroys all command pools, fences, semaphores, and glyph buffers.
    pub fn destroy(&self, device: &ash::Device) {
        unsafe {
            for frame in &self.frames {
                device.unmap_memory(frame.glyph_memory);
                device.destroy_buffer(frame.glyph_buffer, None);
                device.free_memory(frame.glyph_memory, None);
                device.destroy_semaphore(frame.image_available, None);
                device.destroy_semaphore(frame.render_finished, None);
                device.destroy_fence(frame.fence, None);
                device.destroy_command_pool(frame.command_pool, None);
            }
        }
    }
}

/// Generates a 4x4 orthographic projection matrix mapping `[0, width] x [0, height]` to Vulkan NDC `[-1, 1]`.
pub fn orthographic_matrix(width: f32, height: f32) -> [f32; 16] {
    if width <= 0.0 || height <= 0.0 {
        return [
            1.0, 0.0, 0.0, 0.0,
            0.0, 1.0, 0.0, 0.0,
            0.0, 0.0, 1.0, 0.0,
            0.0, 0.0, 0.0, 1.0,
        ];
    }

    [
        2.0 / width,  0.0,           0.0, 0.0,
        0.0,          2.0 / height,  0.0, 0.0,
        0.0,          0.0,           1.0, 0.0,
        -1.0,        -1.0,           0.0, 1.0,
    ]
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_ortho_projection() {
        let mat = orthographic_matrix(800.0, 600.0);
        assert_eq!(mat[0], 2.0 / 800.0);
        assert_eq!(mat[5], 2.0 / 600.0);
        assert_eq!(mat[10], 1.0);
        assert_eq!(mat[12], -1.0);
        assert_eq!(mat[13], -1.0);
        assert_eq!(mat[15], 1.0);
    }
}
