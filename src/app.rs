//! Central application orchestrator, event loop, and frame coordinator.

use std::os::fd::AsRawFd;
use std::time::{Duration, Instant};

use anyhow::{Context, Result};
use ash::khr::surface;
use ash::vk;
use calloop::EventLoop;
use tracing::{info, warn};

use crate::event::{EventDispatcher, PointerEvent};
use crate::layout::{LayoutEngine, LayoutRect};
use crate::platform::WaylandWindow;
use crate::render::{
    FrameRenderer, GlyphAtlas, GlyphPipeline, QuadPipeline, VectorAtlas, VulkanContext,
    VulkanSwapchain, VulkanTexture,
};
use crate::text::{GlyphRasterizer, TextShaper};
use crate::theme::Theme;
use crate::widget::{PopupMenu, Widget, WidgetId, WidgetTree};

/// Represents an active popup menu session rendered onto an auxiliary Wayland subsurface or popup.
pub struct ActivePopupSession {
    /// Identifier of the widget that triggered this popup session.
    pub source_widget: WidgetId,
    /// The popup menu model and items.
    pub menu: PopupMenu,
    /// Vulkan surface handle associated with the popup window.
    pub surface: vk::SurfaceKHR,
    /// Swapchain providing swap images for popup presentation.
    pub swapchain: VulkanSwapchain,
    /// Popup width in physical pixels.
    pub width: u32,
    /// Popup height in physical pixels.
    pub height: u32,
    /// Semaphore signaled when an image is ready for presentation.
    pub image_available: vk::Semaphore,
}

impl ActivePopupSession {
    /// Cleans up and destroys Vulkan resources associated with the popup session.
    pub fn destroy(self, context: &VulkanContext, surface_loader: &surface::Instance) {
        unsafe {
            context.device.device_wait_idle().ok();
            context.device.destroy_semaphore(self.image_available, None);
            self.swapchain.destroy(&context.device);
            surface_loader.destroy_surface(self.surface, None);
        }
    }
}

/// The top-level North Toolkit application coordinator.
///
/// Integrates the Wayland client connection, Vulkan 1.3 swapchain and rendering pipelines,
/// dynamic text and vector atlases, layout engine, and Calloop event loop.
pub struct Application {
    /// Wayland window and client connection state.
    pub window: WaylandWindow,
    /// Vulkan instance, physical device, and logical device context.
    pub context: VulkanContext,
    /// Surface loader for surface lifecycle operations.
    pub surface_loader: surface::Instance,
    /// Main window Vulkan surface.
    pub surface: vk::SurfaceKHR,
    /// Main window swapchain.
    pub swapchain: VulkanSwapchain,
    /// Pipeline for rendering solid, rounded, and stroked quads.
    pub quad_pipeline: QuadPipeline,
    /// Pipeline for rendering rasterized text glyphs.
    pub glyph_pipeline: GlyphPipeline,
    /// Texture backing the vector graphics dynamic atlas.
    pub vector_texture: VulkanTexture,
    /// Textures backing glyph cache pages.
    pub atlas_textures: Vec<VulkanTexture>,
    /// Double/triple-buffered frame synchronizer.
    pub frame_renderer: FrameRenderer,
    /// Dynamic bin-packing glyph atlas.
    pub atlas: GlyphAtlas,
    /// Dynamic SVG vector atlas.
    pub vector_atlas: VectorAtlas,
    /// OpenType font shaper.
    pub shaper: TextShaper,
    /// Glyph rasterizer.
    pub rasterizer: GlyphRasterizer,
    /// Flexbox layout computation engine.
    pub layout_engine: LayoutEngine,
    /// Pointer and keyboard event dispatcher.
    pub event_dispatcher: EventDispatcher,
    /// Active visual design theme.
    pub theme: Theme,
    /// Currently active popup session, if any.
    pub active_popup: Option<ActivePopupSession>,
}

impl Application {
    /// Creates and initializes a new North Toolkit application window with Vulkan hardware acceleration.
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

        let vector_w = 2048u32;
        let vector_h = 2048u32;
        let mut vector_atlas = VectorAtlas::new(vector_w, vector_h);
        let vector_texture = VulkanTexture::new_2d(
            &context,
            vector_w,
            vector_h,
            vk::Format::R8G8B8A8_UNORM,
            vk::ImageUsageFlags::SAMPLED,
        )?;
        let _ = vector_atlas.take_dirty_rects();
        vector_texture.upload_data(
            &context,
            frame_renderer.frames[0].command_pool,
            &vector_atlas.data,
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
            QuadPipeline::new(&context.device, swapchain.format, &vector_texture)?;
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
            vector_texture,
            atlas_textures,
            frame_renderer,
            atlas,
            vector_atlas,
            shaper,
            rasterizer,
            layout_engine,
            event_dispatcher,
            theme,
            active_popup: None,
        })
    }

    /// Configures the window decoration mode (server-side vs client-side).
    pub fn with_decorations(mut self, mode: crate::platform::WindowDecorations) -> Self {
        self.window.set_decoration_mode(mode);
        self
    }

    /// Sets the window decoration mode dynamically.
    pub fn set_decorations(&mut self, mode: crate::platform::WindowDecorations) {
        self.window.set_decoration_mode(mode);
    }

    /// Enters the main Calloop event loop and renders frames for the given widget tree.
    pub fn run(mut self, mut tree: WidgetTree) -> Result<()> {
        let mut event_loop: EventLoop<crate::platform::WaylandState> =
            EventLoop::try_new().context("Failed to create Calloop event loop")?;

        let backend = self.window.connection.backend();
        let owned_fd = backend
            .poll_fd()
            .try_clone_to_owned()
            .context("Failed to duplicate Wayland poll FD")?;
        let loop_handle = event_loop.handle();
        loop_handle
            .insert_source(
                calloop::generic::Generic::new(
                    owned_fd,
                    calloop::Interest::READ,
                    calloop::Mode::Level,
                ),
                |_event, _metadata, _state| Ok(calloop::PostAction::Continue),
            )
            .context("Failed to register Wayland connection FD into Calloop")?;

        let mut last_frame = Instant::now();

        info!("Entering north-toolkit main loop");

        while !self.window.state.should_exit {
            let now = Instant::now();
            let delta_ms = (now - last_frame).as_secs_f32() * 1000.0;
            last_frame = now;

            let anim_dirty = tree.tick_animations(delta_ms);
            let is_dirty = tree.is_dirty() || anim_dirty || self.active_popup.is_some();
            let has_pending_pipes = !self.window.state.pending_paste_pipes.is_empty();

            let timeout = if anim_dirty || has_pending_pipes {
                Some(Duration::from_millis(4))
            } else if is_dirty && !self.window.state.frame_ready {
                Some(Duration::from_millis(4))
            } else {
                Some(Duration::from_millis(50))
            };

            event_loop.dispatch(timeout, &mut self.window.state)?;

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

            let mut i = 0;
            while i < self.window.state.pending_paste_pipes.len() {
                let pipe = &mut self.window.state.pending_paste_pipes[i];
                let mut chunk = [0u8; 4096];
                // SAFETY: reading from non-blocking pipe owned by PipeReader
                let ret = unsafe {
                    libc::read(
                        pipe.fd.as_raw_fd(),
                        chunk.as_mut_ptr() as *mut libc::c_void,
                        chunk.len(),
                    )
                };
                if ret > 0 {
                    pipe.buffer.extend_from_slice(&chunk[..ret as usize]);
                    i += 1;
                } else if ret == 0 {
                    let finished = self.window.state.pending_paste_pipes.remove(i);
                    let is_primary = finished.target == crate::platform::clipboard::SelectionTarget::Primary;
                    let text = String::from_utf8_lossy(&finished.buffer).to_string();
                    self.window.state.paste_events.push((text, is_primary));
                } else {
                    let err = std::io::Error::last_os_error();
                    if err.kind() == std::io::ErrorKind::WouldBlock {
                        i += 1;
                    } else {
                        self.window.state.pending_paste_pipes.remove(i);
                    }
                }
            }

            let mut paste_events = Vec::new();
            std::mem::swap(&mut paste_events, &mut self.window.state.paste_events);
            for (text, is_primary) in paste_events {
                tree.dispatch_paste(&self.event_dispatcher, &text, is_primary);
                self.window.request_frame();
            }

            if let Some(popup) = &self.active_popup {
                let dismissed = self
                    .window
                    .state
                    .popup
                    .as_ref()
                    .map(|p| p.is_dismissed)
                    .unwrap_or(true);
                if dismissed {
                    let source = popup.source_widget;
                    let selected = popup.menu.selected_index;
                    let active = self.active_popup.take().unwrap();
                    active.destroy(&self.context, &self.surface_loader);
                    self.window.dismiss_popup();
                    tree.notify_popup_closed(source, selected);
                    tree.dirty = true;
                    self.window.request_frame();
                }
            }

            let mut pointer_events = Vec::new();
            std::mem::swap(&mut pointer_events, &mut self.window.state.pointer_events);
            for evt in &pointer_events {
                if let Some(popup) = &mut self.active_popup {
                    if self.window.is_pointer_on_popup() {
                        let popup_rect =
                            LayoutRect::new(0.0, 0.0, popup.width as f32, popup.height as f32);
                        let resp = popup.menu.on_pointer_event(evt, popup_rect);
                        if resp == crate::event::EventResponse::Handled {
                            self.window.request_frame();
                        }
                        if popup.menu.is_dismissed {
                            let source = popup.source_widget;
                            let selected = popup.menu.selected_index;
                            let active = self.active_popup.take().unwrap();
                            active.destroy(&self.context, &self.surface_loader);
                            self.window.dismiss_popup();
                            tree.notify_popup_closed(source, selected);
                            tree.dirty = true;
                            self.window.request_frame();
                        }
                        continue;
                    } else if let PointerEvent::Down { .. } = evt {
                        let source = popup.source_widget;
                        let active = self.active_popup.take().unwrap();
                        active.destroy(&self.context, &self.surface_loader);
                        self.window.dismiss_popup();
                        tree.notify_popup_closed(source, None);
                        tree.dirty = true;
                        self.window.request_frame();
                    }
                }

                let resp = tree.dispatch_pointer_event(
                    &mut self.event_dispatcher,
                    &self.layout_engine,
                    evt,
                );
                if resp == crate::event::EventResponse::Handled {
                    tree.dirty = true;
                }
            }

            if !pointer_events.is_empty() {
                let cursor_icon = if self.active_popup.is_some() && self.window.is_pointer_on_popup() {
                    crate::event::CursorIcon::Pointer
                } else {
                    tree.query_cursor_icon(&self.layout_engine, &self.event_dispatcher)
                };
                self.window.set_cursor_shape(cursor_icon);
            }

            let mut key_events = Vec::new();
            std::mem::swap(&mut key_events, &mut self.window.state.key_events);
            for evt in &key_events {
                if let Some(popup) = &mut self.active_popup {
                    let resp = popup.menu.on_key_event(evt);
                    if resp == crate::event::EventResponse::Handled {
                        self.window.request_frame();
                    }
                    if popup.menu.is_dismissed {
                        let source = popup.source_widget;
                        let selected = popup.menu.selected_index;
                        let active = self.active_popup.take().unwrap();
                        active.destroy(&self.context, &self.surface_loader);
                        self.window.dismiss_popup();
                        tree.notify_popup_closed(source, selected);
                        tree.dirty = true;
                        self.window.request_frame();
                    }
                    continue;
                }

                let resp = tree.dispatch_key_event(&mut self.event_dispatcher, evt);
                if resp == crate::event::EventResponse::Handled {
                    tree.dirty = true;
                }
            }

            for action in tree.take_clipboard_actions() {
                match action {
                    crate::widget::ClipboardAction::SetClipboard(text) => {
                        self.window.set_clipboard_text(text);
                    }
                    crate::widget::ClipboardAction::SetPrimary(text) => {
                        self.window.set_primary_text(text);
                    }
                    crate::widget::ClipboardAction::RequestPaste(target) => {
                        self.window.request_paste(target);
                    }
                }
            }

            tree.set_maximized(self.window.state.is_maximized);
            tree.set_csd_enabled(self.window.state.decoration_mode == crate::platform::WindowDecorations::ClientSide);

            for action in tree.take_window_actions() {
                match action {
                    crate::widget::WindowAction::InteractiveMove => {
                        self.window.start_interactive_move();
                    }
                    crate::widget::WindowAction::InteractiveResize(edge) => {
                        self.window.start_interactive_resize(edge);
                    }
                    crate::widget::WindowAction::Minimize => {
                        self.window.minimize();
                    }
                    crate::widget::WindowAction::ToggleMaximize => {
                        self.window.toggle_maximize();
                    }
                    crate::widget::WindowAction::Close => {
                        self.window.state.should_exit = true;
                    }
                }
            }

            if self.active_popup.is_none() {
                if let Some(req) = tree.take_pending_popup_request(&self.layout_engine) {
                    let measure_func = req.menu.measure(&self.shaper);
                    let (popup_w, popup_h) = if let Some(m) = measure_func {
                        m(Some(f32::INFINITY), Some(f32::INFINITY))
                    } else {
                        (180.0, req.menu.total_height())
                    };

                    let scale = self.window.state.scale;
                    let w_u32 = popup_w.ceil() as u32;
                    let h_u32 = popup_h.ceil() as u32;

                    let res = self.window.open_popup(
                        req.anchor_rect.x.round() as i32,
                        req.anchor_rect.y.round() as i32,
                        req.anchor_rect.width.round() as u32,
                        req.anchor_rect.height.round() as u32,
                        w_u32,
                        h_u32,
                    );

                    if res.is_ok() {
                        if let Some(ref mut eq) = self.window.event_queue {
                            eq.roundtrip(&mut self.window.state).ok();
                        }

                        if let Some(popup_surface) = self.window.state.popup.as_ref() {
                            let phys_w = ((w_u32 as f32) * scale).round() as u32;
                            let phys_h = ((h_u32 as f32) * scale).round() as u32;

                            // SAFETY: raw handles provided from WaylandWindow must be valid
                            let vk_surface = unsafe {
                                ash_window::create_surface(
                                    &self.context.entry,
                                    &self.context.instance,
                                    self.window.display_handle,
                                    popup_surface.window_handle,
                                    None,
                                )
                            };

                            if let Ok(surf) = vk_surface {
                                let swapchain_res = VulkanSwapchain::new(
                                    &self.context,
                                    &self.surface_loader,
                                    surf,
                                    phys_w.max(1),
                                    phys_h.max(1),
                                );

                                match swapchain_res {
                                    Ok(swapchain) => {
                                        let sem_info = vk::SemaphoreCreateInfo::default();
                                        // SAFETY: valid logical device handle
                                        let image_available = unsafe {
                                            self.context.device.create_semaphore(&sem_info, None)?
                                        };
                                        self.active_popup = Some(ActivePopupSession {
                                            source_widget: req.source_widget,
                                            menu: req.menu,
                                            surface: surf,
                                            swapchain,
                                            width: w_u32,
                                            height: h_u32,
                                            image_available,
                                        });
                                        tree.dirty = true;
                                        self.window.request_frame();
                                    }
                                    Err(e) => {
                                        warn!("Failed to create swapchain for popup: {:?}", e);
                                        // SAFETY: clean up orphan surface handle on swapchain creation failure
                                        unsafe {
                                            self.surface_loader.destroy_surface(surf, None);
                                        }
                                        self.window.dismiss_popup();
                                    }
                                }
                            }
                        }
                    }
                }
            }

            if let Some((new_w, new_h)) = self.window.state.resize_requested.take() {
                // SAFETY: wait for device idle before swapchain recreation
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

            let has_popup = self.active_popup.is_some();
            if (tree.is_dirty() || anim_dirty || has_popup) && self.window.state.frame_ready {
                self.window.request_frame();

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
                    &mut self.vector_atlas,
                    self.window.state.scale,
                );

                let (popup_target, popup_idx) = if let Some(popup) = &self.active_popup {
                    match popup.swapchain.acquire_next_image(popup.image_available) {
                        Ok((idx, _)) => {
                            let p_img = popup.swapchain.images[idx as usize];
                            let p_view = popup.swapchain.image_views[idx as usize];
                            let p_extent = popup.swapchain.extent;
                            let p_rect =
                                LayoutRect::new(0.0, 0.0, popup.width as f32, popup.height as f32);
                            let p_prims = popup.menu.paint(
                                p_rect,
                                &self.theme,
                                &mut self.atlas,
                                &self.rasterizer,
                                &self.shaper,
                                &mut self.vector_atlas,
                                self.window.state.scale,
                            );
                            (
                                Some((
                                    p_img,
                                    p_view,
                                    p_extent,
                                    self.theme.colors.surface,
                                    p_prims,
                                )),
                                Some(idx),
                            )
                        }
                        Err(e) => {
                            warn!("Failed to acquire popup swapchain image: {:?}", e);
                            (None, None)
                        }
                    }
                } else {
                    (None, None)
                };

                if self.vector_atlas.is_dirty() {
                    let _ = self.vector_atlas.take_dirty_rects();
                    self.vector_texture.upload_data(
                        &self.context,
                        command_pool,
                        &self.vector_atlas.data,
                    )?;
                }

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

                let popup_arg = popup_target.as_ref().map(|(img, view, ext, clr, prims)| {
                    (*img, *view, *ext, *clr, prims.as_slice())
                });

                let cmd = self.frame_renderer.record_render_commands(
                    &self.context,
                    target_image,
                    target_view,
                    self.swapchain.extent,
                    self.theme.colors.background,
                    &primitives,
                    popup_arg,
                    &self.quad_pipeline,
                    &self.glyph_pipeline,
                )?;

                let mut wait_semaphores = vec![image_available];
                let mut wait_stages = vec![vk::PipelineStageFlags::COLOR_ATTACHMENT_OUTPUT];
                if let (Some(popup), Some(_)) = (&self.active_popup, popup_idx) {
                    wait_semaphores.push(popup.image_available);
                    wait_stages.push(vk::PipelineStageFlags::COLOR_ATTACHMENT_OUTPUT);
                }

                let signal_semaphores = [render_finished];
                let command_buffers = [cmd];

                let submit_info = vk::SubmitInfo::default()
                    .wait_semaphores(&wait_semaphores)
                    .wait_dst_stage_mask(&wait_stages)
                    .command_buffers(&command_buffers)
                    .signal_semaphores(&signal_semaphores);

                // SAFETY: valid command buffers and semaphores submitted to graphics queue
                unsafe {
                    self.context.device.queue_submit(
                        self.context.graphics_queue,
                        &[submit_info],
                        fence,
                    )?;
                }

                let mut swapchains = vec![self.swapchain.swapchain];
                let mut image_indices = vec![image_index];
                if let (Some(popup), Some(idx)) = (&self.active_popup, popup_idx) {
                    swapchains.push(popup.swapchain.swapchain);
                    image_indices.push(idx);
                }

                let present_info = vk::PresentInfoKHR::default()
                    .wait_semaphores(&signal_semaphores)
                    .swapchains(&swapchains)
                    .image_indices(&image_indices);

                // SAFETY: presentation of valid swapchain images onto graphics queue
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
        // SAFETY: Wait for GPU idle before destroying pipelines, descriptor pools, command pools, and swapchains
        unsafe {
            self.context.device.device_wait_idle().ok();
            if let Some(popup) = self.active_popup.take() {
                popup.destroy(&self.context, &self.surface_loader);
            }
            self.frame_renderer.destroy(&self.context.device);
            self.quad_pipeline.destroy(&self.context.device);
            self.glyph_pipeline.destroy(&self.context.device);
            self.vector_texture.destroy(&self.context.device);
            for texture in &self.atlas_textures {
                texture.destroy(&self.context.device);
            }
            self.swapchain.destroy(&self.context.device);
            self.surface_loader.destroy_surface(self.surface, None);
        }
    }
}
