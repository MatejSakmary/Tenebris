#include "renderer.hpp"

#include <string>

#include <imgui_impl_glfw.h>
#include <daxa/utils/imgui.hpp>

Renderer::Renderer(const AppWindow & window) :
    context { .daxa_instance{daxa::create_instance({.enable_validation = false})} }
{
    context.device = context.daxa_instance.create_device({ .name = "Daxa device" });

    context.swapchain = context.device.create_swapchain({ 
        .native_window = window.get_native_handle(),
#if defined(_WIN32)
        .native_window_platform = daxa::NativeWindowPlatform::WIN32_API,
#elif defined(__linux__)
        .native_window_platform = daxa::NativeWindowPlatform::XLIB_API,
#endif
        .present_mode = daxa::PresentMode::FIFO,
        .image_usage = daxa::ImageUsageFlagBits::TRANSFER_DST | daxa::ImageUsageFlagBits::COLOR_ATTACHMENT,
        .name = "Swapchain",
    });

    context.pipeline_manager = daxa::PipelineManager({
        .device = context.device,
        .shader_compile_options = {
            .root_paths = { 
                DAXA_SHADER_INCLUDE_DIR,
                "source/renderer",
                "source/renderer/shaders",
                "shaders",
                "shared"
            },
            .language = daxa::ShaderLanguage::GLSL,
        },
        .name = "Pipeline Compiler",
    });

    auto init_compute_pipeline = [&](daxa::ComputePipelineCompileInfo ci, std::shared_ptr<daxa::ComputePipeline> & pip)
    {
        if(auto result = context.pipeline_manager.add_compute_pipeline(ci); result.is_ok())
        {
            pip = result.value();
        } else {
            DBG_ASSERT_TRUE_M(false, result.to_string());
        }
    };
    
    auto init_raster_pipeline = [&](daxa::RasterPipelineCompileInfo ci, std::shared_ptr<daxa::RasterPipeline> & pip)
    {
        if(auto result = context.pipeline_manager.add_raster_pipeline(ci); result.is_ok())
        {
            pip = result.value();
        } else {
            DBG_ASSERT_TRUE_M(false, result.to_string());
        }
    };

    init_compute_pipeline(get_BC6H_pipeline(), context.pipelines.BC6H_compress);
    init_compute_pipeline(get_height_to_normal_pipeline(), context.pipelines.height_to_normal);
    init_compute_pipeline(get_transmittance_LUT_pipeline(), context.pipelines.transmittance);
    init_compute_pipeline(get_multiscattering_LUT_pipeline(), context.pipelines.multiscattering);
    init_compute_pipeline(get_skyview_LUT_pipeline(), context.pipelines.skyview);
    init_raster_pipeline(get_draw_terrain_pipeline(false), context.pipelines.draw_terrain_solid);
    init_raster_pipeline(get_draw_terrain_pipeline(true), context.pipelines.draw_terrain_wireframe);
    init_raster_pipeline(get_terrain_shadowmap_pipeline(), context.pipelines.draw_terrain_shadowmap);
    init_raster_pipeline(get_draw_far_sky_pipeline(), context.pipelines.draw_far_sky);
    init_raster_pipeline(get_post_process_pipeline(context), context.pipelines.post_process);

    context.linear_sampler = context.device.create_sampler({});

    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForVulkan(window.get_glfw_window_handle(), true);
    auto &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    context.imgui_renderer = daxa::ImGuiRenderer({
        .device = context.device,
        .format = context.swapchain.get_format(),
    });

    context.main_task_list.task_list = daxa::TaskGraph({
        .device = context.device,
        .swapchain = context.swapchain,
        .reorder_tasks = true,
        .use_split_barriers = true,
        .jit_compile_permutations = false,
        .permutation_condition_count = Context::MainTaskList::Conditionals::COUNT,
        .record_debug_information = true,
        .name = "main task list"
    });

    create_persistent_resources();

    manager = std::make_unique<TextureManager>(TextureManagerInfo{
        .device = context.device,
        .compress_pipeline = context.pipelines.BC6H_compress,
        .height_to_normal_pipeline = context.pipelines.height_to_normal,
    });

    load_textures();

    initialize_main_tasklist();
}

void Renderer::create_persistent_resources()
{
    context.buffers.globals = daxa::TaskBuffer({
        .initial_buffers = {
            .buffers = std::array{
                context.device.create_buffer(daxa::BufferInfo{
                    .size = sizeof(Globals),
                    .allocate_info = daxa::AutoAllocInfo(
                        daxa::MemoryFlagBits::HOST_ACCESS_RANDOM |
                        daxa::MemoryFlagBits::DEDICATED_MEMORY
                    ),
                    .name = "atmosphere parameters",
                })
            },
        },
        .name = "globals task buffer"
    });

    context.buffers.terrain_vertices = daxa::TaskBuffer({ .name = "terrain vertices task buffer" });
    context.buffers.terrain_indices = daxa::TaskBuffer({ .name = "terrain indices task buffer" });
    context.images.swapchain = daxa::TaskImage({ .swapchain_image = true, .name = "swapchain task image" });
    context.images.diffuse_map = daxa::TaskImage({ .name = "diffuse map task image" });
    context.images.height_map = daxa::TaskImage({ .name = "height map task image" });
    context.images.normal_map = daxa::TaskImage({ .name = "normal map task image" });

    f32 mie_scale_height = 1.2f;
    f32 rayleigh_scale_height = 8.0f;
    Globals *globals_buffer_ptr = context.device.get_host_address_as<Globals>(context.buffers.globals.get_state().buffers[0]);
    this->globals = globals_buffer_ptr;

    // Random variables
    globals_buffer_ptr->trans_lut_dim = {256u, 64u};
    globals_buffer_ptr->mult_lut_dim = {32u, 32u};
    globals_buffer_ptr->sky_lut_dim = {192u, 128u};

    // Atmosphere
    globals_buffer_ptr->sun_direction = {0.99831, 0.0, 0.05814};
    globals_buffer_ptr->atmosphere_bottom = 6360.0f;
    globals_buffer_ptr->atmosphere_top = 6460.0f;
    globals_buffer_ptr->mie_scattering = { 0.003996f, 0.003996f, 0.003996f };
    globals_buffer_ptr->mie_extinction = { 0.004440f, 0.004440f, 0.004440f };
    globals_buffer_ptr->mie_scale_height = mie_scale_height;
    globals_buffer_ptr->mie_phase_function_g = 0.80f;
    globals_buffer_ptr->mie_density[0] = {
        .layer_width = 0.0f,
        .exp_term    = 0.0f,
        .exp_scale   = 0.0f,
        .lin_term    = 0.0f,
        .const_term  = 0.0f 
    };
    globals_buffer_ptr->mie_density[1] = {
        .layer_width = 0.0f,
        .exp_term    = 1.0f,
        .exp_scale   = -1.0f / mie_scale_height,
        .lin_term    = 0.0f,
        .const_term  = 0.0f 
    };
    globals_buffer_ptr->rayleigh_scattering = { 0.005802f, 0.013558f, 0.033100f };
    globals_buffer_ptr->rayleigh_scale_height = rayleigh_scale_height;
    globals_buffer_ptr->rayleigh_density[0] = {
        .layer_width = 0.0f,
        .exp_term    = 0.0f,
        .exp_scale   = 0.0f,
        .lin_term    = 0.0f,
        .const_term  = 0.0f 
    };
    globals_buffer_ptr->rayleigh_density[1] = {
        .layer_width = 0.0f,
        .exp_term    = 1.0f,
        .exp_scale   = -1.0f / rayleigh_scale_height,
        .lin_term    = 0.0f,
        .const_term  = 0.0f
    };
    globals_buffer_ptr->absorption_extinction = { 0.000650f, 0.001881f, 0.000085f };
    globals_buffer_ptr->absorption_density[0] = {
        .layer_width = 25.0f,
        .exp_term    = 0.0f,
        .exp_scale   = 0.0f,
        .lin_term    = 1.0f / 15.0f,
        .const_term  = -2.0f / 3.0f 
    };
    globals_buffer_ptr->absorption_density[1] = {
        .layer_width = 0.0f,
        .exp_term    = 1.0f,
        .exp_scale   = 0.0f,
        .lin_term    = -1.0f / 15.0f,
        .const_term  = 8.0f / 3.0f
    };
    
    // Terrain
    globals_buffer_ptr->terrain_scale = {100.0f, 100.0f};
    globals_buffer_ptr->terrain_height_scale = 70.0f;
    globals_buffer_ptr->terrain_midpoint = 0.3f;
    globals_buffer_ptr->terrain_scale = {100.0f, 100.0f};
    globals_buffer_ptr->terrain_delta = 8.0f;
    globals_buffer_ptr->terrain_min_depth = 1.0f;
    globals_buffer_ptr->terrain_max_depth = 10000.0f;
    globals_buffer_ptr->terrain_min_tess_level = 1;
    globals_buffer_ptr->terrain_max_tess_level = 40;
}

void Renderer::load_textures()
{
    daxa::TaskImage tmp_raw_loaded_image = daxa::TaskImage({.name = "tmp raw loaded task image"});

    manager->load_texture({
        .path = "assets/terrain/rugged_terrain_diffuse.exr",
        .dest_image = tmp_raw_loaded_image
    });

    manager->compress_hdr_texture({
        .raw_texture = tmp_raw_loaded_image,
        .compressed_texture = context.images.diffuse_map
    });

    context.device.destroy_image(tmp_raw_loaded_image.get_state().images[0]);

    manager->load_texture({
        .path = "assets/terrain/rugged_terrain_height.exr",
        .dest_image = context.images.height_map,
    });

    manager->normals_from_heightmap({
        .height_texture = context.images.height_map,
        .normals_texture = context.images.normal_map
    });
    
    // manager->compress_hdr_texture({
    //     .raw_texture = tmp_raw_loaded_image,
    //     .compressed_texture = context.images.normal_map
    // });

    // context.device.destroy_image(tmp_raw_loaded_image.get_state().images[0]);
}

void Renderer::initialize_main_tasklist()
{
    context.main_task_list.task_list.use_persistent_buffer(context.buffers.globals);
    context.main_task_list.task_list.use_persistent_buffer(context.buffers.terrain_indices);
    context.main_task_list.task_list.use_persistent_buffer(context.buffers.terrain_vertices);
    context.main_task_list.task_list.use_persistent_image(context.images.swapchain);
    context.main_task_list.task_list.use_persistent_image(context.images.height_map);
    context.main_task_list.task_list.use_persistent_image(context.images.diffuse_map);
    context.main_task_list.task_list.use_persistent_image(context.images.normal_map);

    /* ========================================= PERSISTENT RESOURCES =============================================*/
    auto extent = context.swapchain.get_surface_extent();
    Globals const * globals = context.device.get_host_address_as<Globals>(context.buffers.globals.get_state().buffers[0]);

    auto & tl = context.main_task_list;

    tl.images.depth = tl.task_list.create_transient_image({
        .format = daxa::Format::D32_SFLOAT,
        .size = {extent.x, extent.y, 1},
        .name = "transient depth"
    });
    
    tl.images.shadowmap = tl.task_list.create_transient_image({
        .format = daxa::Format::D32_SFLOAT,
        .size = {2048u, 2048u, 1u},
        .name = "transient shadowmap"
    });

    tl.images.offscreen = tl.task_list.create_transient_image({
        .format = daxa::Format::R16G16B16A16_SFLOAT,
        .size = {extent.x, extent.y, 1},
        .name = "transient offscreen"
    });

    tl.images.transmittance_lut = tl.task_list.create_transient_image({
        .format = daxa::Format::R16G16B16A16_SFLOAT,
        .size = {globals->trans_lut_dim.x, globals->trans_lut_dim.y, 1},
        .name = "transient transmittance lut"
    });

    tl.images.multiscattering_lut = tl.task_list.create_transient_image({
        .format = daxa::Format::R16G16B16A16_SFLOAT,
        .size = {globals->mult_lut_dim.x, globals->mult_lut_dim.y, 1},
        .name = "transient multiscattering lut"
    });

    tl.images.skyview_lut = tl.task_list.create_transient_image({
        .format = daxa::Format::R16G16B16A16_SFLOAT,
        .size = {globals->sky_lut_dim.x, globals->sky_lut_dim.y, 1},
        .name = "transient skyview lut"
    });
    
    /* ============================================================================================================ */
    /* ===============================================  TASKS  ==================================================== */
    /* ============================================================================================================ */

    /* =========================================== COMPUTE TRANSMITTANCE ========================================== */
    tl.task_list.add_task(ComputeTransmittanceTask{{
        .uses = {
            ._globals = context.buffers.globals.view(),
            ._transmittance_LUT = tl.images.transmittance_lut,
        }},
        &context
    });

    /* =========================================== COMPUTE MULTISCATTERING ======================================== */
    tl.task_list.add_task(ComputeMultiscatteringTask{{
        .uses = {
            ._globals = context.buffers.globals.view(),
            ._transmittance_LUT = tl.images.transmittance_lut,
            ._multiscattering_LUT = tl.images.multiscattering_lut,
        }},
        &context
    });

    /* =========================================== COMPUTE SKYVIEW ================================================ */
    tl.task_list.add_task(ComputeSkyViewTask{{
        .uses = {
            ._globals = context.buffers.globals.view(),
            ._transmittance_LUT = tl.images.transmittance_lut,
            ._multiscattering_LUT = tl.images.multiscattering_lut,
            ._skyview_LUT = tl.images.skyview_lut
        }},
        &context
    });

    /* =========================================== DRAW SHADOWMAP =================================================== */
    tl.task_list.add_task(TerrainShadowmapTask{{
        .uses = {
            ._vertices = context.buffers.terrain_vertices.view(),
            ._indices = context.buffers.terrain_indices.view(),
            ._globals = context.buffers.globals.view(),
            ._shadowmap = tl.images.shadowmap,
            ._height_map = context.images.height_map.view(),
        }},
        &context,
    });

    /* =========================================== DRAW TERRAIN =================================================== */
    tl.task_list.add_task(DrawTerrainTask{{
        .uses = {
            ._vertices = context.buffers.terrain_vertices.view(),
            ._indices = context.buffers.terrain_indices.view(),
            ._globals = context.buffers.globals.view(),
            ._offscreen = tl.images.offscreen,
            ._depth = tl.images.depth,
            ._shadowmap = tl.images.shadowmap,
            ._height_map = context.images.height_map.view(),
            ._diffuse_map = context.images.diffuse_map.view(),
            ._normal_map = context.images.normal_map.view(),
        }},
        &context,
        &wireframe_terrain
    });

    /* =========================================== DRAW FAR SKY =================================================== */
    tl.task_list.add_task(DrawFarSkyTask{{
        .uses = {
            ._globals = context.buffers.globals.view(),
            ._offscreen = tl.images.offscreen,
            ._depth = tl.images.depth,
            ._skyview = tl.images.skyview_lut
        }},
        &context
    });

    /* =========================================== POST PROCESS =================================================== */
    tl.task_list.add_task(PostProcessTask{{
        .uses = {
            ._swapchain = context.images.swapchain.view(),
            ._offscreen = tl.images.offscreen,
        }},
        &context
    });

    /* =========================================== IMGUI ========================================================== */
    tl.task_list.add_task(ImGuiTask{{
        .uses = {
            ._swapchain = context.images.swapchain.view(),
        }},
        &context
    });

    tl.task_list.submit({});
    tl.task_list.present({});
    tl.task_list.complete({});
}

void Renderer::resize()
{
    context.swapchain.resize();
    
    context.main_task_list.task_list = daxa::TaskGraph({
        .device = context.device,
        .swapchain = context.swapchain,
        .reorder_tasks = true,
        .use_split_barriers = true,
        .jit_compile_permutations = true,
        .permutation_condition_count = Context::MainTaskList::Conditionals::COUNT,
        .name = "main task list"
    });

    initialize_main_tasklist();
}

void Renderer::upload_planet_geometry(PlanetGeometry const & geometry)
{
    auto destroy_if_valid = [&](daxa::TaskBuffer & buffer)
    {
        if(buffer.get_state().buffers.size() > 0 && context.device.is_id_valid(buffer.get_state().buffers[0]))
        {
            context.device.destroy_buffer(buffer.get_state().buffers[0]);
        }
    };

    destroy_if_valid(context.buffers.terrain_vertices);
    destroy_if_valid(context.buffers.terrain_indices);
    context.terrain_index_size = geometry.indices.size();
    u32 vertices_size = geometry.vertices.size() * sizeof(f32vec2);
    u32 indices_size = geometry.indices.size() * sizeof(u32);
    u32 total_size = vertices_size + indices_size;

    context.buffers.terrain_vertices.set_buffers({
        .buffers = std::array{
            context.device.create_buffer({
                .size = vertices_size,
                .allocate_info = daxa::AutoAllocInfo{daxa::MemoryFlagBits::DEDICATED_MEMORY},
                .name = "vertices buffer"
            })
        }
    });

    context.buffers.terrain_indices.set_buffers({
        .buffers = std::array{
            context.device.create_buffer({
                .size = indices_size,
                .allocate_info = daxa::AutoAllocInfo{daxa::MemoryFlagBits::DEDICATED_MEMORY},
                .name = "indices buffer"
            })
        }
    });

    daxa::TaskGraph upload_geom_tl = daxa::TaskGraph({
        .device = context.device,
        .staging_memory_pool_size = total_size,
        .name = "copy geometry task list"
    });
    upload_geom_tl.use_persistent_buffer(context.buffers.terrain_indices);
    upload_geom_tl.use_persistent_buffer(context.buffers.terrain_vertices);

    upload_geom_tl.add_task({
        .uses = { 
            daxa::BufferHostTransferWrite{context.buffers.terrain_vertices},
            daxa::BufferHostTransferWrite{context.buffers.terrain_indices}
        },
        .task = [=, this](daxa::TaskInterface ti)
        {
            auto cmd_list = ti.get_command_list();

            {
                auto vert_staging_mem_res = ti.get_allocator().allocate(vertices_size);
                DBG_ASSERT_TRUE_M(
                    vert_staging_mem_res.has_value(),
                    "[Renderer::UploadGeometry()] Failed to create vertices staging buffer"
                );
                auto vert_staging_mem = vert_staging_mem_res.value();
                memcpy(vert_staging_mem.host_address, geometry.vertices.data(), vertices_size);
                cmd_list.copy_buffer_to_buffer({
                    .src_buffer = ti.get_allocator().get_buffer(),
                    .src_offset = vert_staging_mem.buffer_offset,
                    .dst_buffer = ti.uses[context.buffers.terrain_vertices].buffer(),
                    .size = vertices_size
                });
            }
            {
                auto index_staging_mem_res = ti.get_allocator().allocate(indices_size);
                DBG_ASSERT_TRUE_M(
                    index_staging_mem_res.has_value(),
                    "[Renderer::UploadGeometry()] Failed to create indices staging buffer"
                );
                auto index_staging_mem = index_staging_mem_res.value();
                memcpy(index_staging_mem.host_address, geometry.indices.data(), indices_size);
                cmd_list.copy_buffer_to_buffer({
                    .src_buffer = ti.get_allocator().get_buffer(),
                    .src_offset = index_staging_mem.buffer_offset,
                    .dst_buffer = ti.uses[context.buffers.terrain_indices].buffer(),
                    .size = indices_size
                });
            }
        },
        .name = "transfer geometry",
    });

    upload_geom_tl.submit({});
    upload_geom_tl.complete({});
    upload_geom_tl.execute({});
};

void Renderer::draw(const Camera & camera) 
{
    auto extent = context.swapchain.get_surface_extent();
    GetProjectionInfo info {
        .near_plane = 0.1f,
        .far_plane = 500.0f,
    };

    GetShadowmapProjectionInfo shadow_info {
        .left = -60.0f,
        .right = 60.0f,
        .bottom = -60.0f,
        .top = 60.0f,
        .near_plane = 1.0f,
        .far_plane = 500.0f
    };

    Globals* globals = context.device.get_host_address_as<Globals>(context.buffers.globals.get_state().buffers[0]); 
    auto [front, top, right] = camera.get_frustum_info();
    globals->camera_front = front;
    globals->camera_frust_top_offset = top;
    globals->camera_frust_right_offset = right;
    globals->view = camera.get_view_matrix();
    globals->projection = camera.get_projection_matrix(info);
    globals->inv_view_projection = camera.get_inv_view_proj_matrix(info); 
    globals->shadowmap_view = camera.get_shadowmap_view_matrix(globals->sun_direction);
    globals->shadowmap_projection = camera.get_shadowmap_projection_matrix(shadow_info);
    globals->camera_position = camera.get_camera_position();

    context.images.swapchain.set_images({std::array{context.swapchain.acquire_next_image()}});

    if(!context.device.is_id_valid(context.images.swapchain.get_state().images[0]))
    {
        DEBUG_OUT("[Renderer::draw()] Got empty image from swapchain");
        return;
    }

    context.main_task_list.task_list.execute({{
        context.main_task_list.conditionals.data(),
        context.main_task_list.conditionals.size()
    }});
    // DEBUG_OUT(context.main_task_list.task_list.get_debug_string());

    auto result = context.pipeline_manager.reload_all();
    if(std::holds_alternative<daxa::PipelineReloadSuccess>(result)) 
    {
        DEBUG_OUT("[Renderer::draw()] Shaders recompiled successfully");
    } else if (std::holds_alternative<daxa::PipelineReloadError>(result)) 
    {
        DEBUG_OUT(std::get<daxa::PipelineReloadError>(result).message);
    }
}

Renderer::~Renderer()
{
    auto destroy_buffer_if_valid = [&](daxa::TaskBuffer & buffer)
    {
        if(context.device.is_id_valid(buffer.get_state().buffers[0]))
        {
            context.device.destroy_buffer(buffer.get_state().buffers[0]);
        }
    };

    auto destroy_image_if_valid = [&](daxa::TaskImage & image)
    {
        if(context.device.is_id_valid(image.get_state().images[0]))
        {
            context.device.destroy_image(image.get_state().images[0]);
        }
    };

    context.device.wait_idle();
    ImGui_ImplGlfw_Shutdown();
    destroy_buffer_if_valid(context.buffers.terrain_indices);
    destroy_buffer_if_valid(context.buffers.terrain_vertices);
    destroy_buffer_if_valid(context.buffers.globals);
    destroy_image_if_valid(context.images.diffuse_map);
    destroy_image_if_valid(context.images.height_map);
    destroy_image_if_valid(context.images.normal_map);
    context.device.destroy_sampler(context.linear_sampler);
    context.device.collect_garbage();
}