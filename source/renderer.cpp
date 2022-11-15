#include "renderer.hpp"

Renderer::Renderer(daxa::NativeWindowHandle window) :
    daxa_context{daxa::create_context({.enable_validation = true})},
    daxa_device{daxa_context.create_device({.debug_name = "Daxa device"})}
{
    daxa_swapchain = daxa_device.create_swapchain({ 
        .native_window = window,
        .native_window_platform = daxa::NativeWindowPlatform::UNKNOWN,
        .present_mode = daxa::PresentMode::DOUBLE_BUFFER_WAIT_FOR_VBLANK,
        .image_usage = daxa::ImageUsageFlagBits::TRANSFER_DST,
        .debug_name = "Swapchain",
    });

    daxa_pipeline_compiler = daxa_device.create_pipeline_compiler({
        .shader_compile_options = {
            .root_paths = { DAXA_SHADER_INCLUDE_DIR, "source", "shaders",},
            .language = daxa::ShaderLanguage::GLSL,
        },
        .debug_name = "Pipeline Compiler",
    });

    create_resources();
    record_tasks();
}

void Renderer::resize()
{
    daxa_swapchain.resize();
}

void Renderer::draw() 
{
    daxa_tasks.clear_present.remove_runtime_image(daxa_task_images.swapchain_image, daxa_images.swapchain_image);
    daxa_images.swapchain_image = daxa_swapchain.acquire_next_image();
    daxa_tasks.clear_present.add_runtime_image(daxa_task_images.swapchain_image, daxa_images.swapchain_image);
    if(daxa_images.swapchain_image.is_empty())
    {
        return;
    }
    daxa_tasks.clear_present.execute();
}

void Renderer::create_resources()
{
}

void Renderer::record_tasks()
{
    // Create tasklist
    daxa_tasks.clear_present = daxa::TaskList({
        .device = daxa_device,
        .dont_reorder_tasks = false,
        .dont_use_split_barriers = false,
        .swapchain = daxa_swapchain,
        .debug_name = "clear_present_task"
    });
    
    // Create tasklist swapchain image
    daxa_task_images.swapchain_image = daxa_tasks.clear_present.create_task_image({
        .initial_access = daxa::AccessConsts::NONE,
        .initial_layout = daxa::ImageLayout::UNDEFINED,
        .swapchain_image = true,
        .debug_name = "task_swapchain_image"
    });

    daxa_tasks.clear_present.add_task({
        .used_images = { { daxa_task_images.swapchain_image, daxa::TaskImageAccess::TRANSFER_WRITE, daxa::ImageMipArraySlice{} } },
        .task = [=, this](daxa::TaskRuntime const & runtime)
        {
            auto cmd_list = runtime.get_command_list();
            cmd_list.clear_image({
                .dst_image_layout = daxa::ImageLayout::TRANSFER_DST_OPTIMAL,
                .clear_value = {std::array<f32, 4>{0.07, 0.07, 0.15, 1.0}},
                .dst_image = daxa_images.swapchain_image
            });
        },
        .debug_name = "clear swapchain",
    });

    daxa_tasks.clear_present.submit({});
    daxa_tasks.clear_present.present({});
    daxa_tasks.clear_present.complete();
}


Renderer::~Renderer()
{
    daxa_device.wait_idle();
    daxa_device.collect_garbage();
}