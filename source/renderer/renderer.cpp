#include "renderer.hpp"

#include <string>

#include <imgui_impl_glfw.h>
#include <daxa/utils/imgui.hpp>

Renderer::Renderer(const AppWindow & window) :
    context { .daxa_context{daxa::create_context({.enable_validation = false})} }
{
    context.device = context.daxa_context.create_device({ .name = "Daxa device" });

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

    // TODO(msakmary) Move the device pass into texture manager constructor
    auto diffuse_handle = manager.load_texture({
        .path = "assets/terrain/rugged_terrain_diffuse.exr",
        .device = context.device
    });
    auto height_handle = manager.load_texture({
        .path = "assets/terrain/rugged_terrain_height.exr",
        .device = context.device
    });

    context.conditionals.at(Context::Conditionals::COPY_PLANET_GEOMETRY) = false;

    context.pipelines.transmittance = context.pipeline_manager.add_compute_pipeline(get_transmittance_LUT_pipeline()).value();
    context.pipelines.multiscattering = context.pipeline_manager.add_compute_pipeline(get_multiscattering_LUT_pipeline()).value();
    context.pipelines.skyview = context.pipeline_manager.add_compute_pipeline(get_skyview_LUT_pipeline()).value();
    context.pipelines.draw_terrain = context.pipeline_manager.add_raster_pipeline(get_draw_terrain_pipeline()).value();
    context.pipelines.draw_far_sky = context.pipeline_manager.add_raster_pipeline(get_draw_far_sky_pipeline()).value();
    context.pipelines.post_process = context.pipeline_manager.add_raster_pipeline(get_post_process_pipeline(context)).value();

    context.linear_sampler = context.device.create_sampler({});

    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForVulkan(window.get_glfw_window_handle(), true);
    auto &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    context.imgui_renderer = daxa::ImGuiRenderer({
        .device = context.device,
        .format = context.swapchain.get_format(),
    });

    create_resolution_independent_resources();
    create_main_tasklist();
    // TODO(msakmary) this needs to come after tasklist (I need to add into the runtime image) - rethink and improve
    // maybe it would be smart to move independent resource below also and make them add themselves in
    create_resolution_dependent_resources();
}

void Renderer::resize()
{
    context.swapchain.resize();
    create_resolution_dependent_resources();
}

void Renderer::update(const GuiState & state)
{
    auto &atmosphere_parameters = context.buffers.atmosphere_parameters.cpu_buffer;
    atmosphere_parameters.sun_direction =
    {
        f32(glm::cos(glm::radians(state.sun_angle.x)) * glm::sin(glm::radians(state.sun_angle.y))),
        f32(glm::sin(glm::radians(state.sun_angle.x)) * glm::sin(glm::radians(state.sun_angle.y))),
        f32(glm::cos(glm::radians(state.sun_angle.y)))
    };

    atmosphere_parameters.atmosphere_bottom = state.atmosphere_bottom;
    atmosphere_parameters.atmosphere_top = state.atmosphere_top;
    atmosphere_parameters.mie_scale_height = state.mie_scale_height;
    atmosphere_parameters.rayleigh_scale_height = state.rayleigh_scale_height;

    atmosphere_parameters.mie_density[1].exp_scale = -1.0f / atmosphere_parameters.mie_scale_height;
    atmosphere_parameters.rayleigh_density[1].exp_scale = -1.0f / atmosphere_parameters.rayleigh_scale_height;

    context.terrain_params = state.terrain_params;
}

// TODO(msakmary) rethink a better way to do this - without copying the geometry data
void Renderer::upload_planet_geometry(const PlanetGeometry & geometry)
{
    if(context.device.is_id_valid(context.buffers.terrain_vertices.gpu_buffer))
    {
        context.main_task_list.task_list.remove_runtime_buffer(
            context.main_task_list.task_buffers.t_terrain_vertices,
            context.buffers.terrain_vertices.gpu_buffer);

        context.device.destroy_buffer(context.buffers.terrain_vertices.gpu_buffer);
        context.buffers.terrain_vertices.cpu_buffer.clear();
    }

    if(context.device.is_id_valid(context.buffers.terrain_indices.gpu_buffer))
    {
        context.main_task_list.task_list.remove_runtime_buffer(
            context.main_task_list.task_buffers.t_terrain_indices,
            context.buffers.terrain_indices.gpu_buffer);

        context.device.destroy_buffer(context.buffers.terrain_indices.gpu_buffer);
        context.buffers.terrain_indices.cpu_buffer.clear();
    }

    // TODO(msakmary) TEMPORARY
    context.buffers.terrain_vertices.cpu_buffer.reserve(geometry.vertices.size());
    for(auto v : geometry.vertices)
    {
        context.buffers.terrain_vertices.cpu_buffer.push_back(TerrainVertex{.position = {v.x, v.y, 6361.0}});
    }

    for(unsigned int indices : geometry.indices)
    {
        context.buffers.terrain_indices.cpu_buffer.push_back(TerrainIndex{.index = indices});
    }

    context.buffers.terrain_vertices.gpu_buffer = context.device.create_buffer({
        .memory_flags = daxa::MemoryFlagBits::DEDICATED_MEMORY,
        .size = static_cast<u32>(sizeof(TerrainVertex) * context.buffers.terrain_vertices.cpu_buffer.size()),
        .name = "terrain vertices",
    });

    context.buffers.terrain_indices.gpu_buffer = context.device.create_buffer({
        .memory_flags = daxa::MemoryFlagBits::DEDICATED_MEMORY,
        .size = static_cast<u32>(sizeof(TerrainIndex) * context.buffers.terrain_indices.cpu_buffer.size()),
        .name = "terrain indices",
    });

    context.main_task_list.task_list.add_runtime_buffer(
        context.main_task_list.task_buffers.t_terrain_vertices,
        context.buffers.terrain_vertices.gpu_buffer
    );

    context.main_task_list.task_list.add_runtime_buffer(
        context.main_task_list.task_buffers.t_terrain_indices,
        context.buffers.terrain_indices.gpu_buffer
    );

    context.conditionals.at(Context::Conditionals::COPY_PLANET_GEOMETRY) = true;
}

void Renderer::draw(const Camera & camera) 
{
    auto extent = context.swapchain.get_surface_extent();
    GetProjectionInfo info {
        .near_plane = 0.1f,
        .far_plane = 500.0f,
    };

    context.buffers.camera_parameters.cpu_buffer.view = camera.get_view_matrix();
    auto [front, top, right] = camera.get_frustum_info();
    context.buffers.camera_parameters.cpu_buffer.camera_front = front;
    context.buffers.camera_parameters.cpu_buffer.camera_frust_top_offset = top;
    context.buffers.camera_parameters.cpu_buffer.camera_frust_right_offset = right;
    context.buffers.camera_parameters.cpu_buffer.projection = camera.get_projection_matrix(info);
    context.buffers.camera_parameters.cpu_buffer.inv_view_projection = camera.get_inv_view_proj_matrix(info); 
    context.buffers.camera_parameters.cpu_buffer.camera_position = camera.get_camera_position();

    context.main_task_list.task_list.remove_runtime_image(
        context.main_task_list.task_images.at(Images::SWAPCHAIN),
        context.images.at(Images::SWAPCHAIN));

    context.images.at(Images::SWAPCHAIN) = context.swapchain.acquire_next_image();

    context.main_task_list.task_list.add_runtime_image(
        context.main_task_list.task_images.at(Images::SWAPCHAIN),
        context.images.at(Images::SWAPCHAIN));

    if(!context.device.is_id_valid(context.images.at(Images::SWAPCHAIN)))
    {
        DEBUG_OUT("[Renderer::draw()] Got empty image from swapchain");
        return;
    }

    context.main_task_list.task_list.execute({{context.conditionals.data(), context.conditionals.size()}});
    auto result = context.pipeline_manager.reload_all();
    if(result.is_ok()) {
        if (result.value())
        {
            DEBUG_OUT("[Renderer::draw()] Shaders recompiled successfully");
        }
    } else {
        DEBUG_OUT(result.to_string());
    }
}

void Renderer::resize_LUT(Images::ID id, i32vec3 new_size)
{
    auto daxa_image_id = context.images.at(id);

    DEBUG_OUT("[Renderer::resize_LUT] Resizing " << Images::get_image_name(id));

    if(context.device.is_id_valid(daxa_image_id))
    {
        context.device.wait_idle();

        context.main_task_list.task_list.remove_runtime_image(
            context.main_task_list.task_images.at(id),
            context.images.at(id));

        context.device.destroy_image(daxa_image_id);
    }

    context.images.at(id) = context.device.create_image({
        .dimensions = 2,
        .format = daxa::Format::R16G16B16A16_SFLOAT,
        .aspect = daxa::ImageAspectFlagBits::COLOR,
        .size = {u32(new_size.x), u32(new_size.y), u32(new_size.z)},
        .mip_level_count = 1,
        .array_layer_count = 1,
        .sample_count = 1,
        .usage = daxa::ImageUsageFlagBits::SHADER_READ_ONLY | daxa::ImageUsageFlagBits::SHADER_READ_WRITE,
        .memory_flags = daxa::MemoryFlagBits::DEDICATED_MEMORY,
        .name = Images::get_image_name(id).data()
    });

    context.main_task_list.task_list.add_runtime_image(
        context.main_task_list.task_images.at(id),
        context.images.at(id));
}

void Renderer::create_resolution_dependent_resources()
{
    auto daxa_image_id = context.images.at(Images::OFFSCREEN);
    if(context.device.is_id_valid(daxa_image_id))
    {
        context.device.wait_idle();

        context.main_task_list.task_list.remove_runtime_image(
            context.main_task_list.task_images.at(Images::OFFSCREEN),
            context.images.at(Images::OFFSCREEN));

        context.device.destroy_image(daxa_image_id);
    }

    auto daxa_depth_id = context.images.at(Images::DEPTH);
    if(context.device.is_id_valid(daxa_depth_id))
    {
        context.device.wait_idle();

        context.main_task_list.task_list.remove_runtime_image(
            context.main_task_list.task_images.at(Images::DEPTH),
            context.images.at(Images::DEPTH));

        context.device.destroy_image(daxa_depth_id);
    }
    
    auto extent = context.swapchain.get_surface_extent();
    context.images.at(Images::OFFSCREEN) = context.device.create_image({
        .dimensions = 2,
        .format = daxa::Format::R16G16B16A16_SFLOAT,
        .aspect = daxa::ImageAspectFlagBits::COLOR,
        .size = {extent.x, extent.y, 1},
        .mip_level_count = 1,
        .array_layer_count = 1,
        .sample_count = 1,
        .usage = 
            daxa::ImageUsageFlagBits::SHADER_READ_ONLY  |
            daxa::ImageUsageFlagBits::SHADER_READ_WRITE |
            daxa::ImageUsageFlagBits::COLOR_ATTACHMENT,
        .memory_flags = daxa::MemoryFlagBits::DEDICATED_MEMORY,
        .name = Images::get_image_name(Images::OFFSCREEN).data()
    });

    context.images.at(Images::DEPTH) = context.device.create_image({
        .format = daxa::Format::D32_SFLOAT,
        .aspect = daxa::ImageAspectFlagBits::DEPTH,
        .size = {extent.x, extent.y, 1},
        .usage = daxa::ImageUsageFlagBits::DEPTH_STENCIL_ATTACHMENT,
        .name = "debug image"
    });

    context.main_task_list.task_list.add_runtime_image(
        context.main_task_list.task_images.at(Images::OFFSCREEN),
        context.images.at(Images::OFFSCREEN));

    context.main_task_list.task_list.add_runtime_image(
        context.main_task_list.task_images.at(Images::DEPTH),
        context.images.at(Images::DEPTH));
}

void Renderer::create_resolution_independent_resources()
{
    context.buffers.atmosphere_parameters.gpu_buffer = context.device.create_buffer(daxa::BufferInfo{
        .size = sizeof(AtmosphereParameters),
        .name = "atmosphere parameters",
    });

    context.buffers.camera_parameters.gpu_buffer = context.device.create_buffer(daxa::BufferInfo{
        .size = sizeof(CameraParameters),
        .name = "camera parameters"
    });

    f32 mie_scale_height = 1.2f;
    f32 rayleigh_scale_height = 8.0f;
    context.buffers.atmosphere_parameters.cpu_buffer = {
        .sun_direction = {0.99831, 0.0, 0.05814},
        .atmosphere_bottom = 6360.0f,
        .atmosphere_top = 6460.0f,
        .mie_scattering = { 0.003996f, 0.003996f, 0.003996f },
        .mie_extinction = { 0.004440f, 0.004440f, 0.004440f },
        .mie_scale_height = mie_scale_height,
        .mie_phase_function_g = 0.80f,
        .mie_density = {
            {
                .layer_width = 0.0f,
                .exp_term    = 0.0f,
                .exp_scale   = 0.0f,
                .lin_term    = 0.0f,
                .const_term  = 0.0f 
            },
            {
                .layer_width = 0.0f,
                .exp_term    = 1.0f,
                .exp_scale   = -1.0f / mie_scale_height,
                .lin_term    = 0.0f,
                .const_term  = 0.0f
            }},
        .rayleigh_scattering = { 0.005802f, 0.013558f, 0.033100f },
        .rayleigh_scale_height = rayleigh_scale_height,
        .rayleigh_density = {
            {
                .layer_width = 0.0f,
                .exp_term    = 0.0f,
                .exp_scale   = 0.0f,
                .lin_term    = 0.0f,
                .const_term  = 0.0f 
            },
            {
                .layer_width = 0.0f,
                .exp_term    = 1.0f,
                .exp_scale   = -1.0f / rayleigh_scale_height,
                .lin_term    = 0.0f,
                .const_term  = 0.0f
            }},
        .absorption_extinction = { 0.000650f, 0.001881f, 0.000085f },
        .absorption_density = {
            {
                .layer_width = 25.0f,
                .exp_term    = 0.0f,
                .exp_scale   = 0.0f,
                .lin_term    = 1.0f / 15.0f,
                .const_term  = -2.0f / 3.0f 
            },
            {
                .layer_width = 0.0f,
                .exp_term    = 1.0f,
                .exp_scale   = 0.0f,
                .lin_term    = -1.0f / 15.0f,
                .const_term  = 8.0f / 3.0f
            }},
    };
}

void Renderer::create_main_tasklist()
{
    auto create_task_image = [&](Images::ID id) -> void
    {
        context.main_task_list.task_images.at(id) = 
            context.main_task_list.task_list.create_task_image({
                .pre_task_list_slice_states = {},
                .swapchain_image = (id == Images::SWAPCHAIN),
                .name = std::string("task").append(Images::get_image_name(id))
            });
    };

    context.main_task_list.task_list = daxa::TaskList({
        .device = context.device,
        .reorder_tasks = true,
        .use_split_barriers = true,
        .swapchain = context.swapchain,
        .jit_compile_permutations = true,
        .permutation_condition_count = Context::Conditionals::COUNT,
        .name = "main task list"
    });

    for(i32 i = Images::BEGIN; i < Images::IMAGE_COUNT; i++)
    {
        create_task_image(static_cast<Images::ID>(i));
    }

    // TASK BUFFERS
    context.main_task_list.task_buffers.t_terrain_indices = 
        context.main_task_list.task_list.create_task_buffer({
            .name = "terrain_indices"
    });

    context.main_task_list.task_buffers.t_terrain_vertices = 
        context.main_task_list.task_list.create_task_buffer({
            .name = "terrain_vertices"
    });
    
    context.main_task_list.task_buffers.t_atmosphere_parameters = 
        context.main_task_list.task_list.create_task_buffer({
            .name = "atmosphere parameters"
    });

    context.main_task_list.task_list.add_runtime_buffer( context.main_task_list.task_buffers.t_atmosphere_parameters, context.buffers.atmosphere_parameters.gpu_buffer);

    context.main_task_list.task_buffers.t_camera_parameters = 
        context.main_task_list.task_list.create_task_buffer({
            .name = "camera parameters"
    });

    context.main_task_list.task_list.add_runtime_buffer(
        context.main_task_list.task_buffers.t_camera_parameters,
        context.buffers.camera_parameters.gpu_buffer);


    auto skyview_image = runtime.get_images(context.main_task_list.task_images.at(Images::SKYVIEW))[0];
    auto offscreen_image = runtime.get_images(context.main_task_list.task_images.at(Images::OFFSCREEN))[0];
    auto depth_image = runtime.get_images(context.main_task_list.task_images.at(Images::DEPTH))[0];

    auto atmosphere_gpu_buffer = runtime.get_buffers(context.main_task_list.task_buffers.t_atmosphere_parameters)[0];
    auto camera_gpu_buffer = runtime.get_buffers(context.main_task_list.task_buffers.t_camera_parameters)[0];
    context.main_task_list.task_list.add_task(DrawFarSkyTask{
        .uses{
            ._atmosphere_parameters = context.main_task_list.task_buffers.t_atmosphere_parameters.handle(),
            ._camera_parameters = context.main_task_list.task_buffers.t_camera_parameters.handle(),
            ._offscreen = context.main_task_list.task_images.at(Images::OFFSCREEN).handle(),
            ._depth = context.main_task_list.task_images.at(Images::DEPTH).handle(),
            ._skyview = context.main_task_list.task_images.at(Images::SKYVIEW).handle()
        }
    });
    task_upload_input_data(context);
    task_compute_transmittance_LUT(context);
    task_compute_multiscattering_LUT(context);
    task_compute_skyview_LUT(context);
    task_draw_terrain(context);
    task_draw_far_sky(context);
    task_post_process(context);
    task_draw_imgui(context);

    context.main_task_list.task_list.submit({});
    context.main_task_list.task_list.present({});
    context.main_task_list.task_list.complete({});
}

Renderer::~Renderer()
{
    context.device.wait_idle();
    ImGui_ImplGlfw_Shutdown();
    if(context.device.is_id_valid(context.buffers.terrain_vertices.gpu_buffer))
    {
        context.device.destroy_buffer(context.buffers.terrain_vertices.gpu_buffer);
    }
    if(context.device.is_id_valid(context.buffers.terrain_indices.gpu_buffer))
    {
        context.device.destroy_buffer(context.buffers.terrain_indices.gpu_buffer);
    }
    context.device.destroy_buffer(context.buffers.atmosphere_parameters.gpu_buffer);
    context.device.destroy_buffer(context.buffers.camera_parameters.gpu_buffer);
    context.device.destroy_image(context.images.at(Images::TRANSMITTANCE));
    context.device.destroy_image(context.images.at(Images::MULTISCATTERING));
    context.device.destroy_image(context.images.at(Images::SKYVIEW));
    context.device.destroy_image(context.images.at(Images::OFFSCREEN));
    context.device.destroy_image(context.images.at(Images::DEPTH));
    context.device.destroy_sampler(context.linear_sampler);
    context.device.collect_garbage();
}