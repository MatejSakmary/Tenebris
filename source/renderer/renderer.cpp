#include "renderer.hpp"

#include <string>

#include <imgui_impl_glfw.h>
#include <daxa/utils/imgui.hpp>

Renderer::Renderer(const AppWindow & window, Globals * globals) :
    context { .daxa_instance{daxa::create_instance({})} },
    globals{globals}
{
    context.device = context.daxa_instance.create_device({ .name = "Daxa device" });

    context.swapchain = context.device.create_swapchain({ 
        .native_window = window.get_native_handle(),
#if defined(_WIN32)
        .native_window_platform = daxa::NativeWindowPlatform::WIN32_API,
#elif defined(__linux__)
        .native_window_platform = daxa::NativeWindowPlatform::XLIB_API,
#endif
        .present_mode = daxa::PresentMode::MAILBOX,
        .image_usage = daxa::ImageUsageFlagBits::TRANSFER_DST | daxa::ImageUsageFlagBits::COLOR_ATTACHMENT,
        .max_allowed_frames_in_flight = 1,
        .name = "Swapchain",
    });

    context.pipeline_manager = daxa::PipelineManager({
        .device = context.device,
        .shader_compile_options = {
            .root_paths = { 
                DAXA_SHADER_INCLUDE_DIR,
                "source/renderer",
                "source/renderer/shaders",
                "source/renderer/texture_manager/shaders",
                "shaders",
                "shared"
            },
            .language = daxa::ShaderLanguage::GLSL,
            .enable_debug_info = true
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

    init_compute_pipeline(get_transmittance_LUT_pipeline(), context.pipelines.transmittance);
    init_compute_pipeline(get_multiscattering_LUT_pipeline(), context.pipelines.multiscattering);
    init_compute_pipeline(get_skyview_LUT_pipeline(), context.pipelines.skyview);
    init_compute_pipeline(get_esm_pass_pipeline(true), context.pipelines.first_esm_pass);
    init_compute_pipeline(get_esm_pass_pipeline(false), context.pipelines.second_esm_pass);
    init_compute_pipeline(get_analyze_depthbuffer_pipeline(true), context.pipelines.analyze_depthbuffer_first_pass);
    init_compute_pipeline(get_analyze_depthbuffer_pipeline(false), context.pipelines.analyze_depthbuffer_subsequent_pass);
    init_compute_pipeline(get_prepare_shadow_matrices_pipeline(), context.pipelines.prepare_shadow_matrices);
    init_compute_pipeline(get_luminance_histogram_pipeline(), context.pipelines.luminance_histogram);
    init_compute_pipeline(get_adapt_average_luminance_pipeline(), context.pipelines.adapt_average_luminance);

    init_compute_pipeline(get_vsm_free_wrapped_pages_pipeline(), context.pipelines.vsm_free_wrapped_pages);
    init_compute_pipeline(get_vsm_find_free_pages_pipeline(), context.pipelines.vsm_find_free_pages);
    init_compute_pipeline(get_vsm_allocate_pages_pipeline(), context.pipelines.vsm_allocate_pages);
    init_compute_pipeline(get_vsm_clear_pages_pipeline(), context.pipelines.vsm_clear_pages);
    init_raster_pipeline(get_vsm_draw_pages_pipeline(), context.pipelines.vsm_draw_pages);
    init_compute_pipeline(get_vsm_clear_dirty_bit_pipeline(), context.pipelines.vsm_clear_dirty_bit);
    init_compute_pipeline(get_vsm_debug_page_table_pipeline(), context.pipelines.vsm_debug_page_table);
    init_compute_pipeline(get_vsm_debug_meta_memory_table_pipeline(), context.pipelines.vsm_debug_meta_memory_table);

    init_raster_pipeline(get_draw_terrain_pipeline(false), context.pipelines.draw_terrain_solid);
    init_raster_pipeline(get_draw_terrain_pipeline(true), context.pipelines.draw_terrain_wireframe);
    init_raster_pipeline(get_debug_draw_frustum_pipeline(context), context.pipelines.debug_draw_frustum);
    init_raster_pipeline(get_terrain_shadowmap_pipeline(), context.pipelines.draw_terrain_shadowmap);
    init_raster_pipeline(get_deferred_pass_pipeline(), context.pipelines.deferred_pass);
    init_raster_pipeline(get_post_process_pipeline(context), context.pipelines.post_process);

    context.linear_sampler = context.device.create_sampler({
        .address_mode_u = daxa::SamplerAddressMode::CLAMP_TO_BORDER,
        .address_mode_v = daxa::SamplerAddressMode::CLAMP_TO_BORDER,
        .address_mode_w = daxa::SamplerAddressMode::CLAMP_TO_BORDER,
        .border_color = daxa::BorderColor::FLOAT_OPAQUE_BLACK,
    });

    context.nearest_sampler = context.device.create_sampler({
        .magnification_filter = daxa::Filter::NEAREST,
        .minification_filter = daxa::Filter::NEAREST
    });

    context.llce_sampler = context.device.create_sampler({
        .address_mode_u = daxa::SamplerAddressMode::CLAMP_TO_EDGE,
        .address_mode_v = daxa::SamplerAddressMode::CLAMP_TO_EDGE,
        .address_mode_w = daxa::SamplerAddressMode::CLAMP_TO_EDGE,
    });

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
        .pipeline_manager = context.pipeline_manager,
    });

    context.sun_camera = Camera({
        .position = {0.0f, 0.0f, 0.0f},
        .front    = {0.0f, 1.0f, 0.0f},
        .up       = {0.0f, 0.0f, 1.0f}
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
                    .allocate_info = daxa::MemoryFlagBits::DEDICATED_MEMORY,
                    .name = "atmosphere parameters",
                })
            },
        },
        .name = "globals task buffer"
    });

    context.buffers.histogram_readback = daxa::TaskBuffer({
        .initial_buffers = {
            .buffers = std::array{
                context.device.create_buffer(daxa::BufferInfo{
                    .size = static_cast<daxa_u32>(sizeof(Histogram) * HISTOGRAM_BIN_COUNT * 2),
                    .allocate_info = 
                        daxa::MemoryFlagBits::DEDICATED_MEMORY |
                        daxa::MemoryFlagBits::HOST_ACCESS_RANDOM
                    ,
                    .name = "histogram readback buffer",
                })
            },
        },
        .name = "histogram readback task buffer"
    });


    context.buffers.frustum_indices = daxa::TaskBuffer({
        .initial_buffers = {
            .buffers = std::array{
                context.device.create_buffer(daxa::BufferInfo{
                    .size = sizeof(FrustumIndex) * DebugDrawFrustumTask::index_count,
                    .allocate_info = daxa::MemoryFlagBits::DEDICATED_MEMORY,
                    .name = "debug frustum indices",
                })
            },
        },
        .name = "frustum indices task buffer"
    });

    context.buffers.average_luminance = daxa::TaskBuffer({
        .initial_buffers = {
            .buffers = std::array{
                context.device.create_buffer(daxa::BufferInfo{
                    .size = sizeof(AverageLuminance),
                    .allocate_info = daxa::MemoryFlagBits::DEDICATED_MEMORY,
                    .name = "average luminance buffer"
                })
            },
        },
        .name = "average luminance task buffer"
    });

    #pragma region vsm
    context.images.vsm_memory = daxa::TaskImage({
        .initial_images = {
            .images = std::array{
                context.device.create_image(daxa::ImageInfo{
                    .flags = daxa::ImageCreateFlagBits::ALLOW_MUTABLE_FORMAT,
                    .format = daxa::Format::R32_SFLOAT,
                    .size = {VSM_MEMORY_RESOLUTION, VSM_MEMORY_RESOLUTION, 1},
                    .usage = daxa::ImageUsageFlagBits::SHADER_SAMPLED | daxa::ImageUsageFlagBits::SHADER_STORAGE,
                    .name = "vsm memory physical image"
                })
            },
        },
        .name = "vsm memory"
    });

    context.images.vsm_meta_memory_table = daxa::TaskImage({
        .initial_images = {
            .images = std::array{
                context.device.create_image(daxa::ImageInfo{
                    .format = daxa::Format::R32_UINT,
                    .size = { VSM_META_MEMORY_RESOLUTION, VSM_META_MEMORY_RESOLUTION, 1 },
                    .usage = 
                        daxa::ImageUsageFlagBits::SHADER_SAMPLED |
                        daxa::ImageUsageFlagBits::SHADER_STORAGE |
                        daxa::ImageUsageFlagBits::TRANSFER_DST,
                    .name = "vsm meta memory physical image"
                })
            },
        },
        .name = "vsm meta memory table"
    });

    context.images.vsm_debug_meta_memory_table = daxa::TaskImage({
        .initial_images = {
            .images = std::array{
                context.device.create_image(daxa::ImageInfo{
                    .format = daxa::Format::R8G8B8A8_UNORM,
                    .size = { VSM_DEBUG_META_MEMORY_RESOLUTION, VSM_DEBUG_META_MEMORY_RESOLUTION, 1 },
                    .usage = 
                        daxa::ImageUsageFlagBits::SHADER_SAMPLED |
                        daxa::ImageUsageFlagBits::SHADER_STORAGE |
                        daxa::ImageUsageFlagBits::TRANSFER_DST,
                    .name = "vsm debug meta memory physical image"
                })
            },
        },
        .name = "vsm debug meta memory table"
    });

    context.images.vsm_page_table = daxa::TaskImage({
        .initial_images = {
            .images = std::array{
                context.device.create_image(daxa::ImageInfo{
                    .format = daxa::Format::R32_UINT,
                    .size = { VSM_PAGE_TABLE_RESOLUTION, VSM_PAGE_TABLE_RESOLUTION, 1 },
                    .array_layer_count = VSM_CLIP_LEVELS,
                    .usage = 
                        daxa::ImageUsageFlagBits::SHADER_STORAGE |
                        daxa::ImageUsageFlagBits::SHADER_SAMPLED |
                        daxa::ImageUsageFlagBits::TRANSFER_DST,
                    .name = "vsm page table physical image"
                })
            },
        },
        .name = "vsm page table"
    });

    context.images.vsm_page_height_offset = daxa::TaskImage({
        .initial_images = {
            .images = std::array{
                context.device.create_image(daxa::ImageInfo{
                    .format = daxa::Format::R32_SINT,
                    .size = { VSM_PAGE_TABLE_RESOLUTION, VSM_PAGE_TABLE_RESOLUTION, 1 },
                    .array_layer_count = VSM_CLIP_LEVELS,
                    .usage = 
                        daxa::ImageUsageFlagBits::SHADER_STORAGE |
                        daxa::ImageUsageFlagBits::SHADER_SAMPLED |
                        daxa::ImageUsageFlagBits::TRANSFER_DST,
                    .name = "vsm page table height offsets image"
                })
            },
        },
        .name = "vsm page table height offsets"
    });

    context.images.vsm_debug_page_table = daxa::TaskImage({
        .initial_images = {
            .images = std::array{
                context.device.create_image(daxa::ImageInfo{
                    .format = daxa::Format::R8G8B8A8_UNORM,
                    .size = { vsm_debug_paging_table_resolution(), vsm_debug_paging_table_resolution(), 1 },
                    .usage = 
                        daxa::ImageUsageFlagBits::SHADER_SAMPLED |
                        daxa::ImageUsageFlagBits::SHADER_STORAGE |
                        daxa::ImageUsageFlagBits::TRANSFER_DST,
                    .name = "vsm debug page table physical image"
                })
            },
        },
        .name = "vsm debug page table"
    });

    #pragma endregion

    auto upload_task_list = daxa::TaskGraph({
        .device = context.device,
        .swapchain = context.swapchain,
        .reorder_tasks = true,
        .use_split_barriers = true,
        .jit_compile_permutations = false,
        .permutation_condition_count = 0,
        .record_debug_information = true,
        .name = "upload_task_list"
    });
    
    upload_task_list.use_persistent_buffer(context.buffers.frustum_indices);
    upload_task_list.use_persistent_buffer(context.buffers.average_luminance);
    upload_task_list.use_persistent_image(context.images.vsm_page_table);

    daxa::TaskImageView vsm_array_view = context.images.vsm_page_table.view().view(
        {.base_array_layer = 0, .layer_count = VSM_CLIP_LEVELS}
    );

    upload_task_list.add_task({
        .uses = { 
            daxa::BufferHostTransferWrite{context.buffers.frustum_indices},
            daxa::BufferHostTransferWrite{context.buffers.average_luminance},
            daxa::ImageTransferWrite<daxa::ImageViewType::REGULAR_2D_ARRAY>{vsm_array_view}
        },
        .task = [&, this](daxa::TaskInterface ti)
        {
            auto & cmd_list = ti.get_recorder();
            {
                {
                    daxa_u32 size = sizeof(FrustumIndex) * DebugDrawFrustumTask::index_count;
                    auto staging_mem_result = ti.get_allocator().allocate(size);
                    DBG_ASSERT_TRUE_M(
                        staging_mem_result.has_value(),
                        "[Renderer::create_presistent_resources()] Failed to create frustum indices staging buffer"
                    );
                    auto staging_mem = staging_mem_result.value();
                    std::vector<daxa_u32> indices = { 
                        0, 1, 2, 3, 4, 5, 0, 3, 0xFFFFFFFF,
                        6, 5, 4, 7, 2, 1, 6, 7, 0xFFFFFFFF
                    }; 
                    memcpy(staging_mem.host_address, indices.data(), size);
                    cmd_list.copy_buffer_to_buffer({
                        .src_buffer = ti.get_allocator().buffer(),
                        .dst_buffer = ti.uses[context.buffers.frustum_indices].buffer(),
                        .src_offset = staging_mem.buffer_offset,
                        .size = size
                    });
                }
                {
                    daxa_u32 size = sizeof(AverageLuminance);
                    auto staging_mem_result = ti.get_allocator().allocate(size);
                    DBG_ASSERT_TRUE_M(
                        staging_mem_result.has_value(),
                        "[Renderer::create_presistent_resources()] Failed to create average luminance staging buffer"
                    );
                    auto staging_mem = staging_mem_result.value();
                    daxa_f32 inital_value = -1000.0;
                    memcpy(staging_mem.host_address, &inital_value, size);
                    cmd_list.copy_buffer_to_buffer({
                        .src_buffer = ti.get_allocator().buffer(),
                        .dst_buffer = ti.uses[context.buffers.average_luminance].buffer(),
                        .src_offset = staging_mem.buffer_offset,
                        .size = size
                    });
                }
                {
                    cmd_list.clear_image({
                        .clear_value = std::array<daxa_u32, 4>{0u, 0u, 0u, 0u},
                        .dst_image = ti.uses[vsm_array_view].image(),
                        .dst_slice = daxa::ImageMipArraySlice{
                            .base_array_layer = 0,
                            .layer_count = VSM_CLIP_LEVELS
                        }
                    });
                }
            }
        },
        .name = "upload indices",
    });
    upload_task_list.submit({});
    upload_task_list.complete({});
    upload_task_list.execute({});
    context.device.wait_idle();

    context.buffers.terrain_vertices = daxa::TaskBuffer({ .name = "terrain vertices task buffer" });
    context.buffers.terrain_indices = daxa::TaskBuffer({ .name = "terrain indices task buffer" });
    context.images.swapchain = daxa::TaskImage({ .swapchain_image = true, .name = "swapchain task image" });
    context.images.diffuse_map = daxa::TaskImage({ .name = "diffuse map task image" });
    context.images.height_map = daxa::TaskImage({ .name = "height map task image" });
    context.images.normal_map = daxa::TaskImage({ .name = "normal map task image" });
    context.images.tonemapping_lut = daxa::TaskImage({ .name = "tonemapping lut task image" });
}

void Renderer::load_textures()
{
    daxa::TaskImage tmp_raw_loaded_image = daxa::TaskImage({.name = "tmp raw loaded task image"});

    manager->load_texture({
        .filepath = "assets/tonemapping_luts/tony_mc_mapface_f32.dds",
        // .filepath = "C:/Developement/Tenebris/assets/tonemapping_luts/tony_mc_mapface_f32.dds",
        .dest_image = context.images.tonemapping_lut
    });

    manager->load_texture({
        .filepath = "assets/terrain/rugged_terrain_diffuse.exr",
        // .filepath = "assets/terrain/boulder/color.exr",
        // .path = "assets/terrain/8k/mountain_range_diffuse.exr",
        .dest_image = tmp_raw_loaded_image
    });

    manager->compress_hdr_texture({
        .raw_texture = tmp_raw_loaded_image,
        .compressed_texture = context.images.diffuse_map
    });

    context.device.destroy_image(tmp_raw_loaded_image.get_state().images[0]);

    manager->load_texture({
        .filepath = "assets/terrain/rugged_terrain_height.exr",
        // .filepath = "assets/terrain/boulder/height.exr",
        // .path = "assets/terrain/8k/mountain_range_height.exr",
        .dest_image = context.images.height_map,
    });

    manager->normals_from_heightmap({
        .height_texture = context.images.height_map,
        .normals_texture = context.images.normal_map
    });
}

void Renderer::initialize_main_tasklist()
{
    context.main_task_list.task_list.use_persistent_buffer(context.buffers.globals);
    context.main_task_list.task_list.use_persistent_buffer(context.buffers.terrain_indices);
    context.main_task_list.task_list.use_persistent_buffer(context.buffers.terrain_vertices);
    context.main_task_list.task_list.use_persistent_buffer(context.buffers.frustum_indices);
    context.main_task_list.task_list.use_persistent_buffer(context.buffers.average_luminance);
    context.main_task_list.task_list.use_persistent_buffer(context.buffers.histogram_readback);
    context.main_task_list.task_list.use_persistent_image(context.images.swapchain);
    context.main_task_list.task_list.use_persistent_image(context.images.height_map);
    context.main_task_list.task_list.use_persistent_image(context.images.diffuse_map);
    context.main_task_list.task_list.use_persistent_image(context.images.normal_map);
    context.main_task_list.task_list.use_persistent_image(context.images.tonemapping_lut);

    context.main_task_list.task_list.use_persistent_image(context.images.vsm_memory);
    context.main_task_list.task_list.use_persistent_image(context.images.vsm_meta_memory_table);
    context.main_task_list.task_list.use_persistent_image(context.images.vsm_debug_meta_memory_table);
    context.main_task_list.task_list.use_persistent_image(context.images.vsm_page_height_offset);
    context.main_task_list.task_list.use_persistent_image(context.images.vsm_page_table);
    context.main_task_list.task_list.use_persistent_image(context.images.vsm_debug_page_table);

    auto extent = context.swapchain.get_surface_extent();

    auto & tl = context.main_task_list;

    daxa_u32vec2 limits_size;
    daxa_u32vec2 wg_size = AnalyzeDepthbufferTask::wg_total_reads_per_axis;
    limits_size.x = (extent.x + wg_size.x - 1) / wg_size.x;
    limits_size.y = (extent.y + wg_size.y - 1) / wg_size.y;
    tl.buffers.depth_limits = tl.task_list.create_transient_buffer({
        .size = static_cast<daxa_u32>(sizeof(DepthLimits) * limits_size.x * limits_size.y),
        .name = "depth limits"
    });

    #pragma region shadowmap_resources
    DBG_ASSERT_TRUE_M(NUM_CASCADES <= 8, "[Renderer::initalize_main_tasklist()] More than 8 cascades are not supported");
    const auto resolution_multiplier = TerrainShadowmapTask::resolution_table[NUM_CASCADES - 1];
    tl.images.shadowmap_cascades = tl.task_list.create_transient_image({
        .format = daxa::Format::D32_SFLOAT,
        .size = {
            SHADOWMAP_RESOLUTION * resolution_multiplier.x,
            SHADOWMAP_RESOLUTION * resolution_multiplier.y, 
            1u
        },
        .array_layer_count = 1,
        .name = "transient shadowmap cascades"
    });

    tl.images.esm_cascades = tl.task_list.create_transient_image({
        .format = daxa::Format::R16_UNORM,
        .size = {SHADOWMAP_RESOLUTION, SHADOWMAP_RESOLUTION, 1u},
        .array_layer_count = NUM_CASCADES,
        .name = "transient esm cascades"
    });

    tl.images.esm_tmp_cascades = tl.task_list.create_transient_image({
        .format = daxa::Format::R16_UNORM,
        .size = {SHADOWMAP_RESOLUTION, SHADOWMAP_RESOLUTION, 1u},
        .array_layer_count = NUM_CASCADES,
        .name = "transient esm temporary cascades"
    });

    tl.buffers.shadowmap_data = tl.task_list.create_transient_buffer({
        .size = static_cast<daxa_u32>(sizeof(ShadowmapCascadeData) * NUM_CASCADES),
        .name = "shadowmap matrix data"
    });
    #pragma endregion

    #pragma region debug_frustum_draw_resources
    tl.buffers.frustum_vertices = tl.task_list.create_transient_buffer({
        .size = static_cast<daxa_u32>(sizeof(FrustumVertex) * FRUSTUM_VERTEX_COUNT * MAX_FRUSTUM_COUNT),
        .name = "debug frustum vertices"
    });

    tl.buffers.frustum_colors = tl.task_list.create_transient_buffer({
        .size = static_cast<daxa_u32>(sizeof(FrustumColor) * MAX_FRUSTUM_COUNT),
        .name = "debug frustum colors"
    });

    tl.buffers.frustum_indirect = tl.task_list.create_transient_buffer({
        .size = static_cast<daxa_u32>(sizeof(DrawIndexedIndirectStruct)),
        .name = "debug frustum indirect struct"
    });
    #pragma endregion

    #pragma region vsm_resources
    tl.images.vsm_debug_image = tl.task_list.create_transient_image({
        .format = daxa::Format::R16G16B16A16_SFLOAT,
        .size = {VSM_TEXTURE_RESOLUTION, VSM_TEXTURE_RESOLUTION, 1u},
        .name = "transient vsm debug image"
    });

    tl.buffers.vsm_free_wrapped_pages_info = tl.task_list.create_transient_buffer({
        .size = static_cast<daxa_u32>(sizeof(FreeWrappedPagesInfo) * VSM_CLIP_LEVELS),
        .name = "vsm free wrapped pages info"
    });

    tl.buffers.vsm_allocation_count = tl.task_list.create_transient_buffer({
        .size = static_cast<daxa_u32>(sizeof(AllocationCount)),
        .name = "vsm allocation count"
    });

    tl.buffers.vsm_allocation_requests = tl.task_list.create_transient_buffer({
        .size = static_cast<daxa_u32>(sizeof(AllocationRequest) * MAX_NUM_VSM_ALLOC_REQUEST),
        .name = "vsm allocation buffer"
    });

    tl.buffers.vsm_allocate_indirect = tl.task_list.create_transient_buffer({
        .size = static_cast<daxa_u32>(sizeof(DispatchIndirectStruct)),
        .name = "vsm allocate indirect"
    });

    tl.buffers.vsm_clear_indirect = tl.task_list.create_transient_buffer({
        .size = static_cast<daxa_u32>(sizeof(DispatchIndirectStruct)),
        .name = "vsm clear indirect"
    });

    tl.buffers.vsm_clear_dirty_bit_indirect = tl.task_list.create_transient_buffer({
        .size = static_cast<daxa_u32>(sizeof(DispatchIndirectStruct)),
        .name = "vsm clear dirty bit indirect"
    });

    tl.buffers.vsm_free_page_buffer = tl.task_list.create_transient_buffer({
        .size = static_cast<daxa_u32>(sizeof(PageCoordBuffer) * (MAX_NUM_VSM_ALLOC_REQUEST)),
        .name = "vsm free page buffer"
    });

    tl.buffers.vsm_not_visited_page_buffer = tl.task_list.create_transient_buffer({
        .size = static_cast<daxa_u32>(sizeof(PageCoordBuffer) * (MAX_NUM_VSM_ALLOC_REQUEST)),
        .name = "vsm not visited buffer"
    });

    tl.buffers.vsm_find_free_pages_header = tl.task_list.create_transient_buffer({
        .size = static_cast<daxa_u32>(sizeof(FindFreePagesHeader)),
        .name = "find free pages header"
    });

    tl.buffers.vsm_sun_projection_matrices = tl.task_list.create_transient_buffer({
        .size = static_cast<daxa_u32>(sizeof(VSMClipProjection) * VSM_CLIP_LEVELS),
        .name = "sun projection matrices"
    });
    #pragma endregion

    tl.buffers.luminance_histogram = tl.task_list.create_transient_buffer({
        .size = sizeof(Histogram) * HISTOGRAM_BIN_COUNT,
        .name = "histogram"
    });

    tl.images.depth = tl.task_list.create_transient_image({
        .format = daxa::Format::D32_SFLOAT,
        .size = {extent.x, extent.y, 1},
        .name = "transient depth"
    });

    tl.images.g_albedo = tl.task_list.create_transient_image({
        .format = daxa::Format::R16G16B16A16_SFLOAT,
        .size = {extent.x, extent.y, 1},
        .name = "transient gbuffer albedo"
    });

    tl.images.g_normals = tl.task_list.create_transient_image({
        .format = daxa::Format::R16G16B16A16_SFLOAT,
        .size = {extent.x, extent.y, 1},
        .name = "transient gbuffer normals"
    });

    tl.images.offscreen = tl.task_list.create_transient_image({
        .format = daxa::Format::R16G16B16A16_SFLOAT,
        .size = {extent.x, extent.y, 1},
        .name = "offscreen"
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

    #pragma region upload_data
    tl.task_list.add_task({
        .uses = { 
            daxa::ImageTransferWrite<>{context.images.vsm_debug_page_table},
            daxa::BufferHostTransferWrite{context.buffers.globals},
            daxa::BufferHostTransferWrite{tl.buffers.frustum_vertices},
            daxa::BufferHostTransferWrite{tl.buffers.frustum_indirect},
            daxa::BufferHostTransferWrite{tl.buffers.frustum_colors},
            daxa::BufferHostTransferWrite{tl.buffers.luminance_histogram},
            daxa::BufferHostTransferWrite{tl.buffers.vsm_allocation_count},
            daxa::BufferHostTransferWrite{tl.buffers.vsm_find_free_pages_header},
            daxa::BufferHostTransferWrite{tl.buffers.vsm_sun_projection_matrices},
            daxa::BufferHostTransferWrite{tl.buffers.vsm_free_wrapped_pages_info},
        },
        .task = [&, this](daxa::TaskInterface ti)
        {
            auto & cmd_list = ti.get_recorder();
            {
                auto upload_cpu_to_gpu = [&](BufferId gpu_buffer, void* cpu_buffer, size_t size)
                {
                    if(size == 0) { return; }
                    auto staging_mem_result = ti.get_allocator().allocate(size);
                    DBG_ASSERT_TRUE_M(
                        staging_mem_result.has_value(),
                        "[Renderer::initialize_task_list()] Failed to create staging buffer"
                    );
                    auto staging_mem = staging_mem_result.value();
                    memcpy(staging_mem.host_address, cpu_buffer, size);
                    cmd_list.copy_buffer_to_buffer({
                        .src_buffer = ti.get_allocator().buffer(),
                        .dst_buffer = gpu_buffer,
                        .src_offset = staging_mem.buffer_offset,
                        .size = size
                    });
                };
                // Clear vsm debug page table
                cmd_list.clear_image({
                    .clear_value = daxa::ClearValue{std::array{0.0f, 0.0f, 0.0f, 1.0f}},
                    .dst_image = ti.uses[context.images.vsm_debug_page_table].image()
                });
                // Globals
                upload_cpu_to_gpu(ti.uses[context.buffers.globals].buffer(), globals, sizeof(Globals));
                // Frustum vertices
                upload_cpu_to_gpu(
                    ti.uses[tl.buffers.frustum_vertices].buffer(),
                    context.frustum_vertices.data(),
                    sizeof(FrustumVertex) * 8 * context.debug_frustum_cpu_count
                );
                // Frustum colors
                upload_cpu_to_gpu(
                    ti.uses[tl.buffers.frustum_colors].buffer(),
                    context.frustum_colors.data(),
                    sizeof(FrustumColor) * context.debug_frustum_cpu_count
                );
                // Frustum indirect draw buffer
                DrawIndexedIndirectStruct indexed_indirect{
                    .index_count = 18,
                    .instance_count = context.debug_frustum_cpu_count,
                    .first_index = 0,
                    .vertex_offset = 0,
                    .first_instance = 0
                };
                upload_cpu_to_gpu(
                    ti.uses[tl.buffers.frustum_indirect].buffer(),
                    &indexed_indirect,
                    sizeof(DrawIndexedIndirectStruct)
                );
                // Histogram
                std::array<daxa_u32, HISTOGRAM_BIN_COUNT> reset_bin_values {};
                upload_cpu_to_gpu(
                    ti.uses[tl.buffers.luminance_histogram].buffer(),
                    reset_bin_values.data(),
                    sizeof(Histogram) * HISTOGRAM_BIN_COUNT
                );
                // Reset vsm allocation count to 0
                AllocationCount allocation_count{ .count = 0};
                upload_cpu_to_gpu(
                    ti.uses[tl.buffers.vsm_allocation_count].buffer(),
                    &allocation_count,
                    sizeof(AllocationCount)
                );
                // Vsm find free pages header
                FindFreePagesHeader header = FindFreePagesHeader{
                    .free_buffer_counter = 0,
                    .not_visited_buffer_counter = 0
                };
                upload_cpu_to_gpu(
                    ti.uses[tl.buffers.vsm_find_free_pages_header].buffer(),
                    &header,
                    sizeof(FindFreePagesHeader)
                );
                // VSM sun clip matrices
                upload_cpu_to_gpu(
                    ti.uses[tl.buffers.vsm_sun_projection_matrices].buffer(),
                    context.vsm_sun_projections.data(),
                    sizeof(VSMClipProjection) * VSM_CLIP_LEVELS
                );
                // VSM free wrapped apges info
                upload_cpu_to_gpu(
                    ti.uses[tl.buffers.vsm_free_wrapped_pages_info].buffer(),
                    context.vsm_free_wrapped_pages_info.data(),
                    sizeof(FreeWrappedPagesInfo) * VSM_CLIP_LEVELS
                );
            }
        },
        .name = "upload data",
    });
    #pragma endregion

    #pragma region compute_transmittance
    /* =========================================== COMPUTE TRANSMITTANCE ========================================== */
    tl.task_list.add_task(ComputeTransmittanceTask{{
        .uses = {
            ._globals = context.buffers.globals.view(),
            ._transmittance_LUT = tl.images.transmittance_lut,
        }},
        &context
    });
    #pragma endregion

    #pragma region compute_multiscattering
    /* =========================================== COMPUTE MULTISCATTERING ======================================== */
    tl.task_list.add_task(ComputeMultiscatteringTask{{
        .uses = {
            ._globals = context.buffers.globals.view(),
            ._transmittance_LUT = tl.images.transmittance_lut,
            ._multiscattering_LUT = tl.images.multiscattering_lut,
        }},
        &context
    });
    #pragma endregion

    #pragma region compute_skyview
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
    #pragma endregion

    #pragma region vsm_free_wrapped_pages
    tl.task_list.add_task(VSMFreeWrappedPagesTask{{
        .uses = {
            ._free_wrapped_pages_info = tl.buffers.vsm_free_wrapped_pages_info,
            ._vsm_sun_projections = tl.buffers.vsm_sun_projection_matrices,
            ._vsm_page_table = context.images.vsm_page_table.view().view(
                {.base_array_layer = 0, .layer_count = VSM_CLIP_LEVELS}),
            ._vsm_meta_memory_table = context.images.vsm_meta_memory_table.view()
        }},
        &context
    });
    #pragma endregion

    tl.task_list.conditional({
        .condition_index = MainConditionals::USE_DEBUG_CAMERA,
        .when_true = [&]()
        {
            // primary camera is DEBUG
            // secondary camera is MAIN
            auto secondary_camera_depth = tl.task_list.create_transient_image({
                .format = daxa::Format::D32_SFLOAT,
                .size = {extent.x, extent.y, 1},
                .name = "transient secondary camera depth"
            });
            
            #pragma region draw_main_camera_terrain
            tl.task_list.add_task(DrawTerrainTask{{
                .uses = {
                    ._vertices = context.buffers.terrain_vertices.view(),
                    ._indices = context.buffers.terrain_indices.view(),
                    ._globals = context.buffers.globals.view(),
                    ._vsm_sun_projections = tl.buffers.vsm_sun_projection_matrices,
                    ._g_albedo = tl.images.g_albedo,
                    ._g_normals = tl.images.g_normals,
                    ._depth = secondary_camera_depth,
                    ._height_map = context.images.height_map.view(),
                    ._diffuse_map = context.images.diffuse_map.view(),
                    ._normal_map = context.images.normal_map.view(),
                }},
                &context,
                &wireframe_terrain,
                true
            });
            #pragma endregion

            #pragma region draw_debug_camera_terrain
            tl.task_list.add_task(DrawTerrainTask{{
                .uses = {
                    ._vertices = context.buffers.terrain_vertices.view(),
                    ._indices = context.buffers.terrain_indices.view(),
                    ._globals = context.buffers.globals.view(),
                    ._vsm_sun_projections = tl.buffers.vsm_sun_projection_matrices,
                    ._g_albedo = tl.images.g_albedo,
                    ._g_normals = tl.images.g_normals,
                    ._depth = tl.images.depth,
                    ._height_map = context.images.height_map.view(),
                    ._diffuse_map = context.images.diffuse_map.view(),
                    ._normal_map = context.images.normal_map.view(),
                }},
                &context,
                &wireframe_terrain,
                false
            });
            #pragma endregion

            #pragma region analyze_deptbuffer
            tl.task_list.add_task(AnalyzeDepthbufferTask{{
                .uses = {
                    ._globals = context.buffers.globals.view(),
                    ._depth_limits = tl.buffers.depth_limits,
                    ._vsm_allocation_count = tl.buffers.vsm_allocation_count,
                    ._vsm_allocation_buffer = tl.buffers.vsm_allocation_requests,
                    ._vsm_allocate_indirect = tl.buffers.vsm_allocate_indirect,
                    ._vsm_clear_indirect = tl.buffers.vsm_clear_indirect,
                    ._vsm_clear_dirty_bit_indirect = tl.buffers.vsm_clear_dirty_bit_indirect,
                    ._vsm_sun_projections = tl.buffers.vsm_sun_projection_matrices,
                    ._depth = secondary_camera_depth,
                    ._vsm_page_table = context.images.vsm_page_table.view().view(
                        {.base_array_layer = 0, .layer_count = VSM_CLIP_LEVELS}
                    ),
                    ._vsm_meta_memory_table = context.images.vsm_meta_memory_table.view(),
                }},
                &context
            });
            #pragma endregion
        },
        .when_false = [&]()
        {
            #pragma region draw_terrain
            tl.task_list.add_task(DrawTerrainTask{{
                .uses = {
                    ._vertices = context.buffers.terrain_vertices.view(),
                    ._indices = context.buffers.terrain_indices.view(),
                    ._globals = context.buffers.globals.view(),
                    ._vsm_sun_projections = tl.buffers.vsm_sun_projection_matrices,
                    ._g_albedo = tl.images.g_albedo,
                    ._g_normals = tl.images.g_normals,
                    ._depth = tl.images.depth,
                    ._height_map = context.images.height_map.view(),
                    ._diffuse_map = context.images.diffuse_map.view(),
                    ._normal_map = context.images.normal_map.view(),
                }},
                &context,
                &wireframe_terrain
            });
            #pragma endregion

            #pragma region analyze_deptbuffer
            tl.task_list.add_task(AnalyzeDepthbufferTask{{
                .uses = {
                    ._globals = context.buffers.globals.view(),
                    ._depth_limits = tl.buffers.depth_limits,
                    ._vsm_allocation_count = tl.buffers.vsm_allocation_count,
                    ._vsm_allocation_buffer = tl.buffers.vsm_allocation_requests,
                    ._vsm_allocate_indirect = tl.buffers.vsm_allocate_indirect,
                    ._vsm_clear_indirect = tl.buffers.vsm_clear_indirect,
                    ._vsm_clear_dirty_bit_indirect = tl.buffers.vsm_clear_dirty_bit_indirect,
                    ._vsm_sun_projections = tl.buffers.vsm_sun_projection_matrices,
                    ._depth = tl.images.depth,
                    ._vsm_page_table = context.images.vsm_page_table.view().view(
                        {.base_array_layer = 0, .layer_count = VSM_CLIP_LEVELS}
                    ),
                    ._vsm_meta_memory_table = context.images.vsm_meta_memory_table.view(),
                }},
                &context
            });
            #pragma endregion
        }
    });
    #pragma region vsm_find_free_pages
    tl.task_list.add_task(VSMFindFreePagesTask{{
        .uses = {
            ._vsm_allocation_buffer = tl.buffers.vsm_allocation_requests,
            ._vsm_allocate_indirect = tl.buffers.vsm_allocate_indirect,
            ._vsm_free_pages_buffer = tl.buffers.vsm_free_page_buffer,
            ._vsm_not_visited_pages_buffer = tl.buffers.vsm_not_visited_page_buffer,
            ._vsm_find_free_pages_header = tl.buffers.vsm_find_free_pages_header,
            ._vsm_page_table = context.images.vsm_page_table.view().view(
                {.base_array_layer = 0, .layer_count = VSM_CLIP_LEVELS}
            ),
            ._vsm_meta_memory_table = context.images.vsm_meta_memory_table.view(),
        }},
        &context
    });
    #pragma endregion

    #pragma region allocate_vsm_pages
    tl.task_list.add_task(VSMAllocatePagesTask{{
        .uses = {
            ._vsm_allocation_count = tl.buffers.vsm_allocation_count,
            ._vsm_allocation_buffer = tl.buffers.vsm_allocation_requests,
            ._vsm_allocate_indirect = tl.buffers.vsm_allocate_indirect,
            ._vsm_clear_indirect = tl.buffers.vsm_clear_indirect,
            ._vsm_free_pages_buffer = tl.buffers.vsm_free_page_buffer,
            ._vsm_not_visited_pages_buffer = tl.buffers.vsm_not_visited_page_buffer,
            ._vsm_find_free_pages_header = tl.buffers.vsm_find_free_pages_header,
            ._vsm_sun_projections = tl.buffers.vsm_sun_projection_matrices,
            ._vsm_page_table = context.images.vsm_page_table.view().view(
                {.base_array_layer = 0, .layer_count = VSM_CLIP_LEVELS}),
            ._vsm_page_height_offset = context.images.vsm_page_height_offset.view().view(
                {.base_array_layer = 0, .layer_count = VSM_CLIP_LEVELS}),
            ._vsm_meta_memory_table = context.images.vsm_meta_memory_table.view(),
        }},
        &context
    });
    #pragma endregion

    #pragma region clear_vsm_pages
    tl.task_list.add_task(VSMClearPagesTask{{
        .uses = {
            ._vsm_allocation_buffer = tl.buffers.vsm_allocation_requests,
            ._vsm_clear_indirect = tl.buffers.vsm_clear_indirect,
            ._vsm_page_table = context.images.vsm_page_table.view().view(
                {.base_array_layer = 0, .layer_count = VSM_CLIP_LEVELS}),
            ._vsm_memory = context.images.vsm_memory
        }},
        &context
    });
    #pragma endregion

    #pragma region draw_vsm_pages
    tl.task_list.add_task(VSMDrawPagesTask{{
        .uses = {
            ._globals = context.buffers.globals.view(),
            ._vertices = context.buffers.terrain_vertices.view(),
            ._indices = context.buffers.terrain_indices.view(),
            ._vsm_sun_projections = tl.buffers.vsm_sun_projection_matrices,
            ._height_map = context.images.height_map.view(),
            ._debug = tl.images.vsm_debug_image,
            ._vsm_page_table = context.images.vsm_page_table.view().view(
                {.base_array_layer = 0, .layer_count = VSM_CLIP_LEVELS}
            ),
            ._vsm_memory = context.images.vsm_memory.view()
        }},
        &context
    });
    #pragma endregion

    #pragma region vsm_clear_dirty_bit
    tl.task_list.add_task(VSMClearDirtyBitTask{{
        .uses = {
            ._vsm_allocation_count = tl.buffers.vsm_allocation_count,
            ._vsm_allocation_buffer = tl.buffers.vsm_allocation_requests,
            ._vsm_clear_dirty_bit_indirect = tl.buffers.vsm_clear_dirty_bit_indirect,
            ._vsm_page_table = context.images.vsm_page_table.view().view(
                {.base_array_layer = 0, .layer_count = VSM_CLIP_LEVELS}
            ),
        }},
        &context
    });
    #pragma endregion

    #pragma region prepare_shadowmap_matrices
    tl.task_list.add_task(PrepareShadowmapMatricesTask{{
        .uses = {
            ._globals = context.buffers.globals.view(),
            ._depth_limits = tl.buffers.depth_limits,
            ._cascade_data = tl.buffers.shadowmap_data,
            ._frustum_vertices = tl.buffers.frustum_vertices,
            ._frustum_colors = tl.buffers.frustum_colors,
            ._frustum_indirect = tl.buffers.frustum_indirect,
        }},
        &context
    });
    #pragma endregion

    #pragma region draw_shadowmap
    tl.task_list.add_task(TerrainShadowmapTask{{
        .uses = {
            ._vertices = context.buffers.terrain_vertices.view(),
            ._indices = context.buffers.terrain_indices.view(),
            ._globals = context.buffers.globals.view(),
            ._cascade_data = tl.buffers.shadowmap_data,
            ._shadowmap_cascades = tl.images.shadowmap_cascades,
            ._height_map = context.images.height_map.view(),
        }},
        &context,
    });
    #pragma endregion

    #pragma region esm_pass
    tl.task_list.add_task(ESMFirstPassTask{{
        .uses = {
            ._shadowmap = tl.images.shadowmap_cascades,
            ._esm_map = tl.images.esm_tmp_cascades.view({ .base_array_layer = 0, .layer_count = NUM_CASCADES })
        }},
        &context
    });

    tl.task_list.add_task(ESMSecondPassTask{{
        .uses = {
            ._esm_tmp_map = tl.images.esm_tmp_cascades.view({ .base_array_layer = 0, .layer_count = NUM_CASCADES }),
            ._esm_target_map = tl.images.esm_cascades.view({ .base_array_layer = 0, .layer_count = NUM_CASCADES })
        }},
        &context
    });
    #pragma endregion

    #pragma region deferred_pass
    tl.task_list.add_task(DeferredPassTask{{
        .uses = {
            ._globals = context.buffers.globals.view(),
            ._cascade_data = tl.buffers.shadowmap_data,
            ._vsm_sun_projections = tl.buffers.vsm_sun_projection_matrices,
            ._offscreen = tl.images.offscreen,
            ._g_albedo = tl.images.g_albedo,
            ._g_normals = tl.images.g_normals,
            ._transmittance = tl.images.transmittance_lut,
            ._esm = tl.images.esm_cascades.view({.base_array_layer = 0, .layer_count = NUM_CASCADES}),
            ._skyview = tl.images.skyview_lut,
            ._depth = tl.images.depth,
            ._vsm_page_table = context.images.vsm_page_table.view().view(
                {.base_array_layer = 0, .layer_count = VSM_CLIP_LEVELS}),
            ._vsm_page_height_offset = context.images.vsm_page_height_offset.view().view(
                {.base_array_layer = 0, .layer_count = VSM_CLIP_LEVELS}),
            ._vsm_physical_memory = context.images.vsm_memory.view()
        }},
        &context
    });
    #pragma endregion

    #pragma region luminance_histogram
    tl.task_list.add_task(LuminanceHistogramTask{{
        .uses = {
            ._globals = context.buffers.globals.view(),
            ._histogram = tl.buffers.luminance_histogram,
            ._offscreen = tl.images.offscreen
        }},
        &context
    });
    #pragma endregion

    #pragma region readback_histogram
    tl.task_list.add_task({
        .uses = { 
            daxa::BufferTransferRead{tl.buffers.luminance_histogram},
            daxa::BufferTransferWrite{context.buffers.histogram_readback}
        },
        .task = [&, this](daxa::TaskInterface ti)
        {
            auto & cmd_list = ti.get_recorder();
            const daxa_u32 size = static_cast<daxa_u32>(sizeof(Histogram)) * HISTOGRAM_BIN_COUNT;
            const bool is_frame_even = globals->frame_index % 2 == 0;

            cmd_list.copy_buffer_to_buffer({
                .src_buffer = ti.uses[tl.buffers.luminance_histogram].buffer(),
                .dst_buffer = ti.uses[context.buffers.histogram_readback].buffer(),
                .src_offset = 0,
                .dst_offset = is_frame_even ? 0u : sizeof(Histogram) * HISTOGRAM_BIN_COUNT,
                .size = size
            });
        },
        .name = "readback histogram"
    });
    #pragma endregion

    #pragma region adapt_average_luminance
    tl.task_list.add_task(AdaptAverageLuminanceTask{{
        .uses = {
            ._globals = context.buffers.globals.view(),
            ._histogram = tl.buffers.luminance_histogram,
            ._average_luminance = context.buffers.average_luminance.view()
        }},
        &context
    });
    #pragma endregion

    #pragma region post_process
    tl.task_list.add_task(PostProcessTask{{
        .uses = {
            ._average_luminance = context.buffers.average_luminance.view(),
            ._swapchain = context.images.swapchain.view(),
            ._offscreen = tl.images.offscreen,
            ._tonemapping_lut = context.images.tonemapping_lut.view(),
        }},
        &context
    });
    #pragma endregion

    #pragma region draw_debug_frustum
    tl.task_list.add_task(DebugDrawFrustumTask{{
        .uses = {
            ._globals = context.buffers.globals.view(),
            ._frustum_indices = context.buffers.frustum_indices.view(),
            ._frustum_vertices = tl.buffers.frustum_vertices,
            ._frustum_colors = tl.buffers.frustum_colors,
            ._frustum_indirect = tl.buffers.frustum_indirect,
            ._swapchain = context.images.swapchain.view(),
            ._depth = tl.images.depth
        }},
        &context,
    });
    #pragma endregion

#if VSM_DEBUG_VIZ_PASS == 1
    #pragma region vsm_debug_page_table
    tl.task_list.add_task(VSMDebugVirtualPageTableTask{{
        .uses = {
            ._globals = context.buffers.globals.view(),
            ._vsm_page_table = context.images.vsm_page_table.view().view(
                {.base_array_layer = 0, .layer_count = VSM_CLIP_LEVELS}
            ),
            ._vsm_debug_page_table = context.images.vsm_debug_page_table.view()
        }},
        &context
    });
    #pragma endregion

    #pragma region vsm_debug_meta_table
    tl.task_list.add_task(VSMDebugMetaTableTask{{
        .uses = {
            ._vsm_page_table_mem_pass = context.images.vsm_page_table.view().view(
                {.base_array_layer = 0, .layer_count = VSM_CLIP_LEVELS}
            ),
            ._vsm_meta_memory_table = context.images.vsm_meta_memory_table.view(),
            ._vsm_debug_meta_memory_table = context.images.vsm_debug_meta_memory_table.view()
        }},
        &context
    });
    #pragma endregion
#endif

    #pragma region imgui
    tl.task_list.add_task(ImGuiTask{{
        .uses = {
            ._swapchain = context.images.swapchain.view(),
            ._vsm_debug_page_table = context.images.vsm_debug_page_table.view(),
        }},
        &context
    });
    #pragma endregion

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
    daxa_u32 vertices_size = geometry.vertices.size() * sizeof(daxa_f32vec2);
    daxa_u32 indices_size = geometry.indices.size() * sizeof(daxa_u32);
    daxa_u32 total_size = vertices_size + indices_size;

    context.buffers.terrain_vertices.set_buffers({
        .buffers = std::array{
            context.device.create_buffer({
                .size = vertices_size,
                .allocate_info = daxa::MemoryFlagBits::DEDICATED_MEMORY,
                .name = "vertices buffer"
            })
        }
    });

    context.buffers.terrain_indices.set_buffers({
        .buffers = std::array{
            context.device.create_buffer({
                .size = indices_size,
                .allocate_info = daxa::MemoryFlagBits::DEDICATED_MEMORY,
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
            auto & cmd_list = ti.get_recorder();

            {
                auto vert_staging_mem_res = ti.get_allocator().allocate(vertices_size);
                DBG_ASSERT_TRUE_M(
                    vert_staging_mem_res.has_value(),
                    "[Renderer::UploadGeometry()] Failed to create vertices staging buffer"
                );
                auto vert_staging_mem = vert_staging_mem_res.value();
                memcpy(vert_staging_mem.host_address, geometry.vertices.data(), vertices_size);
                cmd_list.copy_buffer_to_buffer({
                    .src_buffer = ti.get_allocator().buffer(),
                    .dst_buffer = ti.uses[context.buffers.terrain_vertices].buffer(),
                    .src_offset = vert_staging_mem.buffer_offset,
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
                    .src_buffer = ti.get_allocator().buffer(),
                    .dst_buffer = ti.uses[context.buffers.terrain_indices].buffer(),
                    .src_offset = index_staging_mem.buffer_offset,
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

void Renderer::draw(DrawInfo const & info) 
{
    context.debug_frustum_cpu_count = 0;
    auto extent = context.swapchain.get_surface_extent();


    Camera * primary_camera = globals->use_debug_camera ? &info.debug_camera : &info.main_camera;
    Camera * secondary_camera = globals->use_debug_camera ? &info.main_camera : &info.debug_camera;

    globals->camera_position     = primary_camera->get_camera_position();
    globals->offset              = primary_camera->offset;
    globals->view                = primary_camera->get_view_matrix();
    globals->projection          = primary_camera->get_projection_matrix();
    globals->inv_projection      = primary_camera->get_inv_projection_matrix();
    globals->inv_view_projection = primary_camera->get_inv_view_proj_matrix(); 

    globals->secondary_camera_position     = secondary_camera->get_camera_position();
    globals->secondary_offset              = secondary_camera->offset;
    globals->secondary_view                = secondary_camera->get_view_matrix();
    globals->secondary_projection          = secondary_camera->get_projection_matrix();
    globals->secondary_inv_view_projection = secondary_camera->get_inv_view_proj_matrix(); 

    if(globals->use_debug_camera && globals->control_main_camera)
    {
        secondary_camera->write_frustum_vertices({
            std::span<FrustumVertex, 8>{&context.frustum_vertices[8 * context.debug_frustum_cpu_count], 8 }
        });
        context.frustum_colors[context.debug_frustum_cpu_count].color = daxa_f32vec3{1.0, 1.0, 1.0};
        context.debug_frustum_cpu_count += 1;
    }

    // Setup VSM Clip projection matrices

    // context.sun_camera.set_position( (globals->sun_direction * -1000.0f) + camera_fp_offset);
    context.sun_camera.set_front(daxa_f32vec3{
        -globals->sun_direction.x,
        -globals->sun_direction.y,
        -globals->sun_direction.z
    });
    // OrthographicInfo curr_clip_projection = OrthographicInfo {
    //     .left   = -1000.0f,
    //     .right  =  1000.0f,
    //     .top    =  1000.0f,
    //     .bottom = -1000.0f,
    //     .near   =  10.00f,
    //     .far    =  10'000.0f
    // };
    OrthographicInfo curr_clip_projection = OrthographicInfo {
        .left   = -10.0f,
        .right  =  10.0f,
        .top    =  10.0f,
        .bottom = -10.0f,
        .near   =  1.0f,
        .far    =  100.0f
    };
    daxa_i32 sun_offset_factor = 50; 
    daxa_f32 curr_clip_texel_world_size = (curr_clip_projection.right - curr_clip_projection.left) / VSM_TEXTURE_RESOLUTION;
    globals->vsm_sun_offset = context.sun_camera.offset;
    globals->vsm_clip0_texel_world_size = curr_clip_texel_world_size;
    context.main_task_list.conditionals.at(MainConditionals::USE_DEBUG_CAMERA) = globals->use_debug_camera;

    for(daxa_i32 clip_level = 0; clip_level < VSM_CLIP_LEVELS; clip_level++)
    {
        context.sun_camera.proj_info = curr_clip_projection;
        context.sun_camera.update_front_vector(0.0f, 0.0f);
        const daxa_f32vec3 to_sun_camera_offset = globals->sun_direction;
        const daxa_f32 clip_page_world_size = curr_clip_texel_world_size * VSM_PAGE_SIZE;
        const bool should_draw_debug_clip = 
            (clip_level == globals->vsm_debug_clip_level) && globals->force_view_clip_level;

        const auto align_page_info = context.sun_camera.align_clip_to_player(
            &info.main_camera,
            to_sun_camera_offset, 
            std::span<FrustumVertex, VSM_PAGE_TABLE_RESOLUTION * VSM_PAGE_TABLE_RESOLUTION * 8>{
                &context.frustum_vertices[8 * context.debug_frustum_cpu_count],
                VSM_PAGE_TABLE_RESOLUTION * VSM_PAGE_TABLE_RESOLUTION * 8
            },
            should_draw_debug_clip,
            sun_offset_factor
        );

        if(should_draw_debug_clip)
        {
            for(int i = 0; i < VSM_PAGE_TABLE_RESOLUTION * VSM_PAGE_TABLE_RESOLUTION; i++)
            {
                context.frustum_colors[context.debug_frustum_cpu_count + i].color = daxa_f32vec3{0.0, 0.0, 1.0};
            }
            if(globals->use_debug_camera)
            {
                context.debug_frustum_cpu_count += VSM_PAGE_TABLE_RESOLUTION * VSM_PAGE_TABLE_RESOLUTION;
            }
        }

        const auto clear_offset = daxa_i32vec2(
            align_page_info.page_offset.x - context.vsm_last_frame_offset.at(clip_level).x,
            align_page_info.page_offset.y - context.vsm_last_frame_offset.at(clip_level).y
        );
        context.vsm_last_frame_offset.at(clip_level) = align_page_info.page_offset;
        context.vsm_free_wrapped_pages_info.at(clip_level).clear_offset = clear_offset;

        context.vsm_sun_projections.at(clip_level) = VSMClipProjection{
            .camera_height_offset = align_page_info.sun_height_offset,
            .depth_page_offset = align_page_info.per_page_depth_offset,
            .page_offset = daxa_i32vec2{
                align_page_info.page_offset.x % VSM_PAGE_TABLE_RESOLUTION,
                align_page_info.page_offset.y % VSM_PAGE_TABLE_RESOLUTION
            },
            .offset = context.sun_camera.offset,
            .view = context.sun_camera.get_view_matrix(),
            .projection = context.sun_camera.get_projection_matrix(),
            .projection_view = context.sun_camera.get_projection_view_matrix(),
            .inv_projection_view = context.sun_camera.get_inv_view_proj_matrix()
        };

        if(clip_level == globals->vsm_debug_clip_level && globals->use_debug_camera)
        {
            context.sun_camera.write_frustum_vertices({
                std::span<FrustumVertex, 8>{&context.frustum_vertices[8 * context.debug_frustum_cpu_count], 8 }
            });
            context.frustum_colors[context.debug_frustum_cpu_count].color = daxa_f32vec3{1.0, 1.0, 0.2};
            context.debug_frustum_cpu_count += 1;
        }

        curr_clip_projection.left *= 2;
        curr_clip_projection.right *= 2;
        curr_clip_projection.top *= 2;
        curr_clip_projection.bottom *= 2;
        curr_clip_projection.near *= 2;
        curr_clip_projection.far *= 2;
        sun_offset_factor *= 2;
        curr_clip_texel_world_size *= 2;
    }

    auto [front, top, right] = info.main_camera.get_frustum_info();
    globals->camera_front = front;
    globals->camera_frust_top_offset = top;
    globals->camera_frust_right_offset = right;

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

    auto * histogram_host_pointer = context.device.get_host_address_as<Histogram>(context.buffers.histogram_readback.get_state().buffers[0]).value();
    const bool was_last_frame_even = ((globals->frame_index - 1) % 2) == 0;
    const daxa_u32 offset = was_last_frame_even ? 0 : HISTOGRAM_BIN_COUNT;
    memcpy(context.cpu_histogram.data(), histogram_host_pointer + offset, sizeof(Histogram) * HISTOGRAM_BIN_COUNT);

    globals->frame_index++;

    auto result = context.pipeline_manager.reload_all();
    if(daxa::holds_alternative<daxa::PipelineReloadSuccess>(result)) 
    {
        DEBUG_OUT("[Renderer::draw()] Shaders recompiled successfully");
    } else if (daxa::holds_alternative<daxa::PipelineReloadError>(result)) 
    {
        DEBUG_OUT(daxa::get<daxa::PipelineReloadError>(result).message);
    }
    context.device.collect_garbage();
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
    destroy_buffer_if_valid(context.buffers.frustum_indices);
    destroy_buffer_if_valid(context.buffers.terrain_vertices);
    destroy_buffer_if_valid(context.buffers.globals);
    destroy_buffer_if_valid(context.buffers.average_luminance);
    destroy_buffer_if_valid(context.buffers.histogram_readback);
    destroy_image_if_valid(context.images.diffuse_map);
    destroy_image_if_valid(context.images.height_map);
    destroy_image_if_valid(context.images.normal_map);
    destroy_image_if_valid(context.images.tonemapping_lut);
    destroy_image_if_valid(context.images.vsm_debug_page_table);
    destroy_image_if_valid(context.images.vsm_page_table);
    destroy_image_if_valid(context.images.vsm_page_height_offset);
    destroy_image_if_valid(context.images.vsm_memory);
    destroy_image_if_valid(context.images.vsm_meta_memory_table);
    destroy_image_if_valid(context.images.vsm_debug_meta_memory_table);
    context.device.destroy_sampler(context.linear_sampler);
    context.device.destroy_sampler(context.nearest_sampler);
    context.device.destroy_sampler(context.llce_sampler);
    context.device.collect_garbage();
}