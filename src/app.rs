use std::time::{Duration, Instant};

use anyhow::{Context, Result};
use ash::khr::surface;
use ash::vk;
use calloop::EventLoop;
use tracing::info;

use crate::event::EventDispatcher;
use crate::layout::LayoutEngine;
use crate::platform::WaylandWindow;
use crate::render::{
    FrameRenderer, GlyphAtlas, GlyphPipeline, QuadPipeline, VulkanContext, VulkanSwapchain,
    VulkanTexture,
};
use crate::text::{GlyphRasterizer, TextShaper};
use crate::theme::Theme;
use crate::widget::WidgetTree;

pub struct Application {
    pub window: WaylandWindow,
    pub context: VulkanContext,
    pub surface_loader: surface::Instance,
    pub surface: vk::SurfaceKHR,
    pub swapchain: VulkanSwapchain,
    pub quad_pipeline: QuadPipeline,
    pub glyph_pipeline: GlyphPipeline,
    pub default_texture: VulkanTexture,
    pub atlas_textures: Vec<VulkanTexture>,
    pub frame_renderer: FrameRenderer,
    pub atlas: GlyphAtlas,
    pub shaper: TextShaper,
    pub rasterizer: GlyphRasterizer,
    pub layout_engine: LayoutEngine,
    pub event_dispatcher: EventDispatcher,
    pub theme: Theme,
}

impl Application {
    pub fn new(title: &str, width: u32, height: u32) -> Result<Self> {
        let window = WaylandWindow::new(title, width, height)?;
        let context = VulkanContext::new(Some(window.display_handle))?;

        let surface_loader = surface::Instance::new(&context.entry, &context.instance);
        let surface = unsafe {
            ash_window::create_surface(
                &context.entry,
                &context.instance,
                window.display_handle,
                window.window_handle,
                None,
            )
        }
        .context("Failed to create Vulkan surface via ash-window")?;

        let scale = window.state.scale;
        let phys_w = ((width as f32) * scale).round() as u32;
        let phys_h = ((height as f32) * scale).round() as u32;
        let swapchain = VulkanSwapchain::new(
            &context,
            &surface_loader,
            surface,
            phys_w.max(1),
            phys_h.max(1),
        )?;

        if let Some(viewport) = &window.state.viewport {
            viewport.set_destination(width as i32, height as i32);
        }

        let frame_renderer = FrameRenderer::new(&context)?;

        let default_texture = VulkanTexture::new_2d(
            &context,
            1,
            1,
            vk::Format::R8G8B8A8_UNORM,
            vk::ImageUsageFlags::SAMPLED,
        )?;
        let white_pixel = [255u8, 255u8, 255u8, 255u8];
        default_texture.upload_data(
            &context,
            frame_renderer.frames[0].command_pool,
            &white_pixel,
        )?;

        let atlas_w = 2048u32;
        let atlas_h = 2048u32;
        let initial_atlas_texture = VulkanTexture::new_2d(
            &context,
            atlas_w,
            atlas_h,
            vk::Format::R8_UNORM,
            vk::ImageUsageFlags::SAMPLED,
        )?;
        let atlas_textures = vec![initial_atlas_texture];

        let quad_pipeline =
            QuadPipeline::new(&context.device, swapchain.format, &default_texture)?;
        let glyph_pipeline =
            GlyphPipeline::new(&context.device, swapchain.format, &atlas_textures)?;

        let atlas = GlyphAtlas::new(atlas_w, atlas_h, 4);
        let shaper = TextShaper::default_system_font()?;
        let rasterizer = GlyphRasterizer::new(shaper.font_db().clone());
        let layout_engine = LayoutEngine::new();
        let event_dispatcher = EventDispatcher::new();
        let theme = Theme::dark();

        info!("Application successfully initialized with Vulkan & Wayland");

        Ok(Self {
            window,
            context,
            surface_loader,
            surface,
            swapchain,
            quad_pipeline,
            glyph_pipeline,
            default_texture,
            atlas_textures,
            frame_renderer,
            atlas,
            shaper,
            rasterizer,
            layout_engine,
            event_dispatcher,
            theme,
        })
    }

    pub fn run(mut self, mut tree: WidgetTree) -> Result<()> {
        let mut event_loop: EventLoop<crate::platform::WaylandState> =
            EventLoop::try_new().context("Failed to create Calloop event loop")?;

        let backend = self.window.connection.backend();
        let owned_fd = backend.poll_fd().try_clone_to_owned().context("Failed to duplicate Wayland poll FD")?;
        let loop_handle = event_loop.handle();
        loop_handle.insert_source(
            calloop::generic::Generic::new(owned_fd, calloop::Interest::READ, calloop::Mode::Level),
            |_event, _metadata, _state| {
                Ok(calloop::PostAction::Continue)
            },
        ).context("Failed to register Wayland connection FD into Calloop")?;

        let mut last_frame = Instant::now();

        info!("Entering north-toolkit main loop");

        while !self.window.state.should_exit {
            let now = Instant::now();
            let delta_ms = (now - last_frame).as_secs_f32() * 1000.0;
            last_frame = now;

            event_loop.dispatch(Some(Duration::from_millis(16)), &mut self.window.state)?;

            if let Some(ref mut eq) = self.window.event_queue {
                while let Some(guard) = eq.prepare_read() {
                    match guard.read() {
                        Ok(_) => break,
                        Err(wayland_client::backend::WaylandError::Io(e))
                            if e.kind() == std::io::ErrorKind::WouldBlock =>
                        {
                            break;
                        }
                        Err(e) => return Err(e.into()),
                    }
                }
                eq.dispatch_pending(&mut self.window.state)?;
                self.window.connection.flush()?;
            }

            let mut pointer_events = Vec::new();
            std::mem::swap(&mut pointer_events, &mut self.window.state.pointer_events);
            for evt in &pointer_events {
                let resp = tree.dispatch_pointer_event(
                    &mut self.event_dispatcher,
                    &self.layout_engine,
                    evt,
                );
                if resp == crate::event::EventResponse::Handled {
                    tree.dirty = true;
                }
            }

            let mut key_events = Vec::new();
            std::mem::swap(&mut key_events, &mut self.window.state.key_events);
            for evt in &key_events {
                let resp = tree.dispatch_key_event(&self.event_dispatcher, evt);
                if resp == crate::event::EventResponse::Handled {
                    tree.dirty = true;
                }
            }

            let anim_dirty = tree.tick_animations(delta_ms);

            if let Some((new_w, new_h)) = self.window.state.resize_requested.take() {
                unsafe {
                    self.context.device.device_wait_idle()?;
                }
                self.swapchain.destroy(&self.context.device);
                let scale = self.window.state.scale;
                let phys_w = ((new_w as f32) * scale).round() as u32;
                let phys_h = ((new_h as f32) * scale).round() as u32;
                self.swapchain = VulkanSwapchain::new(
                    &self.context,
                    &self.surface_loader,
                    self.surface,
                    phys_w.max(1),
                    phys_h.max(1),
                )?;
                if let Some(viewport) = &self.window.state.viewport {
                    viewport.set_destination(new_w as i32, new_h as i32);
                }
                tree.dirty = true;
            }

            if tree.is_dirty() || anim_dirty {
                tree.sync_layout(&mut self.layout_engine, &self.shaper);
                if let Some(root) = tree.root() {
                    let root_layout = tree.nodes[root.0].as_ref().unwrap().layout_node;
                    self.layout_engine.compute(
                        root_layout,
                        self.window.state.width as f32,
                        self.window.state.height as f32,
                    );
                }

                let (image_available, render_finished, fence, command_pool) = {
                    let sync = self.frame_renderer.current_sync();
                    (
                        sync.image_available,
                        sync.render_finished,
                        sync.fence,
                        sync.command_pool,
                    )
                };
                let (image_index, _) = match self.swapchain.acquire_next_image(image_available) {
                    Ok(res) => res,
                    Err(_) => continue,
                };

                let target_image = self.swapchain.images[image_index as usize];
                let target_view = self.swapchain.image_views[image_index as usize];

                let primitives = tree.paint(
                    &self.layout_engine,
                    &self.theme,
                    &mut self.atlas,
                    &self.rasterizer,
                    &self.shaper,
                    self.window.state.scale,
                );

                if self.atlas.is_dirty() {
                    let (atlas_w, atlas_h) = self.atlas.dimensions();
                    while self.atlas_textures.len() < self.atlas.page_count() {
                        let new_tex = VulkanTexture::new_2d(
                            &self.context,
                            atlas_w,
                            atlas_h,
                            vk::Format::R8_UNORM,
                            vk::ImageUsageFlags::SAMPLED,
                        )?;
                        self.glyph_pipeline.add_atlas_page(&self.context.device, &new_tex)?;
                        self.atlas_textures.push(new_tex);
                    }

                    for (page_idx, _rect) in self.atlas.take_dirty_rects() {
                        if (page_idx as usize) < self.atlas_textures.len() {
                            self.atlas_textures[page_idx as usize].upload_data(
                                &self.context,
                                command_pool,
                                self.atlas.page_data(page_idx),
                            )?;
                        }
                    }
                }

                let cmd = self.frame_renderer.record_render_commands(
                    &self.context,
                    target_image,
                    target_view,
                    self.swapchain.extent,
                    self.theme.colors.background,
                    &primitives,
                    &self.quad_pipeline,
                    &self.glyph_pipeline,
                )?;

                let wait_semaphores = [image_available];
                let wait_stages = [vk::PipelineStageFlags::COLOR_ATTACHMENT_OUTPUT];
                let signal_semaphores = [render_finished];
                let command_buffers = [cmd];

                let submit_info = vk::SubmitInfo::default()
                    .wait_semaphores(&wait_semaphores)
                    .wait_dst_stage_mask(&wait_stages)
                    .command_buffers(&command_buffers)
                    .signal_semaphores(&signal_semaphores);

                unsafe {
                    self.context.device.queue_submit(
                        self.context.graphics_queue,
                        &[submit_info],
                        fence,
                    )?;
                }

                let swapchains = [self.swapchain.swapchain];
                let image_indices = [image_index];
                let present_info = vk::PresentInfoKHR::default()
                    .wait_semaphores(&signal_semaphores)
                    .swapchains(&swapchains)
                    .image_indices(&image_indices);

                unsafe {
                    self.swapchain
                        .swapchain_loader
                        .queue_present(self.context.graphics_queue, &present_info)?;
                }

                self.frame_renderer.advance_frame();
            }
        }

        info!("north-toolkit application cleanly exited");
        Ok(())
    }
}

impl Drop for Application {
    fn drop(&mut self) {
        unsafe {
            self.context.device.device_wait_idle().ok();
            self.frame_renderer.destroy(&self.context.device);
            self.quad_pipeline.destroy(&self.context.device);
            self.glyph_pipeline.destroy(&self.context.device);
            self.default_texture.destroy(&self.context.device);
            for texture in &self.atlas_textures {
                texture.destroy(&self.context.device);
            }
            self.swapchain.destroy(&self.context.device);
            self.surface_loader.destroy_surface(self.surface, None);
        }
    }
}
