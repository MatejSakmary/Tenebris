#include "renderer.hpp"

#include <string.h>

#include <imgui_impl_glfw.h>
#include <daxa/utils/imgui.hpp>

Renderer::Renderer(const AppWindow & window) :
    context { .daxa_context{daxa::create_context({.enable_validation = true})} }
{
    context.device = context.daxa_context.create_device({ .debug_name = "Daxa device" });

    context.swapchain = context.device.create_swapchain({ 
        .native_window = window.get_native_handle(),
#if defined(_WIN32)
        .native_window_platform = daxa::NativeWindowPlatform::WIN32_API,
#elif defined(__linux__)
        .native_window_platform = daxa::NativeWindowPlatform::XLIB_API,
#endif
        .present_mode = daxa::PresentMode::DOUBLE_BUFFER_WAIT_FOR_VBLANK,
        .image_usage = daxa::ImageUsageFlagBits::TRANSFER_DST | daxa::ImageUsageFlagBits::COLOR_ATTACHMENT,
        .debug_name = "Swapchain",
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
        .debug_name = "Pipeline Compiler",
    });

    context.pipelines.transmittance = context.pipeline_manager.add_compute_pipeline(get_transmittance_LUT_pipeline()).value();
    context.pipelines.multiscattering = context.pipeline_manager.add_compute_pipeline(get_multiscattering_LUT_pipeline()).value();
    context.pipelines.skyview = context.pipeline_manager.add_compute_pipeline(get_skyview_LUT_pipeline()).value();
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
    create_resolution_dependent_resources();
    create_main_tasklist();
}

void Renderer::resize()
{
    context.swapchain.resize();
    create_resolution_dependent_resources();
}

void Renderer::draw() 
{

    context.main_task_list.task_list.remove_runtime_image(
        context.main_task_list.task_images.t_swapchain,
        context.images.swapchain_image);

    context.images.swapchain_image = context.swapchain.acquire_next_image();

    context.main_task_list.task_list.add_runtime_image(
        context.main_task_list.task_images.t_swapchain,
        context.images.swapchain_image);

    if(!context.device.is_id_valid(context.images.swapchain_image))
    {
        DEBUG_OUT("[Renderer::draw()] Got empty image from swapchain");
        return;
    }
    context.main_task_list.task_list.execute();
    auto result = context.pipeline_manager.reload_all();
    if(result.is_ok()) {
        if (result.value() == true)
        {
            DEBUG_OUT("[Renderer::draw()] Shaders recompiled successfully");
        }
    } else {
        DEBUG_OUT(result.to_string());
    }
}

void Renderer::update_on_gui_state(const GuiState & gui_state)
{
}

void Renderer::create_resolution_dependent_resources()
{
}

void Renderer::create_resolution_independent_resources()
{
    context.images.transmittance = context.device.create_image({
        .dimensions = 2,
        .format = daxa::Format::R16G16B16A16_SFLOAT,
        .aspect = daxa::ImageAspectFlagBits::COLOR,
        .size = {256, 64, 1},
        .mip_level_count = 1,
        .array_layer_count = 1,
        .sample_count = 1,
        .usage = daxa::ImageUsageFlagBits::SHADER_READ_ONLY | daxa::ImageUsageFlagBits::SHADER_READ_WRITE,
        .memory_flags = daxa::MemoryFlagBits::DEDICATED_MEMORY,
        .debug_name = "transmittance"
    });

    context.images.multiscattering = context.device.create_image({
        .dimensions = 2,
        .format = daxa::Format::R16G16B16A16_SFLOAT,
        .aspect = daxa::ImageAspectFlagBits::COLOR,
        .size = {32, 32, 1},
        .mip_level_count = 1,
        .array_layer_count = 1,
        .sample_count = 1,
        .usage = daxa::ImageUsageFlagBits::SHADER_READ_ONLY | daxa::ImageUsageFlagBits::SHADER_READ_WRITE,
        .memory_flags = daxa::MemoryFlagBits::DEDICATED_MEMORY,
        .debug_name = "multiscattering"
    });

    context.images.skyview = context.device.create_image({
        .dimensions = 2,
        .format = daxa::Format::R16G16B16A16_SFLOAT,
        .aspect = daxa::ImageAspectFlagBits::COLOR,
        .size = {192, 128, 1},
        .mip_level_count = 1,
        .array_layer_count = 1,
        .sample_count = 1,
        .usage = daxa::ImageUsageFlagBits::SHADER_READ_ONLY | daxa::ImageUsageFlagBits::SHADER_READ_WRITE,
        .memory_flags = daxa::MemoryFlagBits::DEDICATED_MEMORY,
        .debug_name = "skyview"
    });

    context.buffers.atmosphere_parameters.gpu_buffer = context.device.create_buffer(daxa::BufferInfo{
        .size = sizeof(AtmosphereParameters),
        .debug_name = "atmosphere_parameters",
    }); 

    f32 mie_scale_height = 1.2f;
    f32 rayleigh_scale_height = 8.0f;
    context.buffers.atmosphere_parameters.cpu_buffer = {
        .camera_position = {0.0, 0.0, 0.11},
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
    context.main_task_list.task_list = daxa::TaskList({
        .device = context.device,
        .reorder_tasks = true,
        .use_split_barriers = true,
        .swapchain = context.swapchain,
        .debug_name = "main task list"
    });

    context.main_task_list.task_images.t_swapchain =
        context.main_task_list.task_list.create_task_image({
        .initial_access = daxa::AccessConsts::NONE,
        .initial_layout = daxa::ImageLayout::UNDEFINED,
        .swapchain_image = true,
        .debug_name = "swapchain task image"
    });

    context.main_task_list.task_images.t_offscreen = 
        context.main_task_list.task_list.create_task_image({
        .initial_access = daxa::AccessConsts::NONE,
        .initial_layout = daxa::ImageLayout::UNDEFINED,
        .swapchain_image = false,
        .debug_name = "backbuffer image"
    });

    context.main_task_list.task_images.t_transmittance = 
        context.main_task_list.task_list.create_task_image({
            .initial_access = daxa::AccessConsts::NONE,
            .initial_layout = daxa::ImageLayout::UNDEFINED,
            .swapchain_image = false,
            .debug_name = "transmittance task image"
    });

    context.main_task_list.task_images.t_multiscattering =
        context.main_task_list.task_list.create_task_image({
            .initial_access = daxa::AccessConsts::NONE,
            .initial_layout = daxa::ImageLayout::UNDEFINED,
            .swapchain_image = false,
            .debug_name = "multiscattering task image"
    });

    context.main_task_list.task_images.t_skyview = 
        context.main_task_list.task_list.create_task_image({
            .initial_access = daxa::AccessConsts::NONE,
            .initial_layout = daxa::ImageLayout::UNDEFINED,
            .swapchain_image = false,
            .debug_name = "skyview task image"
    });

    // TASK BUFFERS
    context.main_task_list.task_buffers.t_atmosphere_parameters = 
        context.main_task_list.task_list.create_task_buffer({
            .initial_access = daxa::AccessConsts::NONE,
            .debug_name = "atmosphere_parameters"
    });

    context.main_task_list.task_list.add_runtime_buffer(
        context.main_task_list.task_buffers.t_atmosphere_parameters,
        context.buffers.atmosphere_parameters.gpu_buffer);

    context.main_task_list.task_list.add_runtime_image(
        context.main_task_list.task_images.t_transmittance,
        context.images.transmittance);

    context.main_task_list.task_list.add_runtime_image(
        context.main_task_list.task_images.t_multiscattering,
        context.images.multiscattering);

    context.main_task_list.task_list.add_runtime_image(
        context.main_task_list.task_images.t_skyview,
        context.images.skyview);

    context.main_task_list.task_list.add_runtime_image(
        context.main_task_list.task_images.t_offscreen,
        context.images.skyview);

    task_upload_input_data(context);
    task_compute_transmittance_LUT(context);
    task_compute_multiscattering_LUT(context);
    task_compute_skyview_LUT(context);
    task_post_process(context);
    task_draw_imgui(context);

    context.main_task_list.task_list.submit({});
    context.main_task_list.task_list.present({});
    context.main_task_list.task_list.complete();
}

Renderer::~Renderer()
{
    context.device.wait_idle();
    context.device.destroy_buffer(context.buffers.atmosphere_parameters.gpu_buffer);
    context.device.destroy_image(context.images.transmittance);
    context.device.destroy_image(context.images.multiscattering);
    context.device.destroy_image(context.images.skyview);
    context.device.destroy_sampler(context.linear_sampler);
    context.device.collect_garbage();
}