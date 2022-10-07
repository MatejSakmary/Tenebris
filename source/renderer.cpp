#include "renderer.hpp"

template<typename T>
T& insert_or_throw(std::unordered_map<std::string, T> & map, const std::pair<std::string, T>&& elem)
{
    auto inserted = map.insert(elem);
    if(!inserted.second)
    {
        throw std::runtime_error("Inserted element " + elem.first + " already exists in map");
    }
    return inserted.first->second;
}

Renderer::Renderer(daxa::NativeWindowHandle window) :
    daxa_ctx{daxa::create_context({.enable_validation = true})},
    device{daxa_ctx.create_device({.debug_name = "Daxa device"})},
    swapchain
    {
        device.create_swapchain(
        { 
            .native_window = window,
            .native_window_platform = daxa::NativeWindowPlatform::UNKNOWN,
            .present_mode = daxa::PresentMode::DOUBLE_BUFFER_WAIT_FOR_VBLANK,
            .image_usage = daxa::ImageUsageFlagBits::TRANSFER_DST,
            .debug_name = "Swapchain",
        })
    },
    pipeline_compiler
    {
        device.create_pipeline_compiler(
            {
                .shader_compile_options = 
                {
                    .root_paths = { DAXA_SHADER_INCLUDE_DIR, "source", "shaders",},
                    .language = daxa::ShaderLanguage::GLSL,
                },
                .debug_name = "Pipeline Compiler",
            }
        )
    },
    clear_present_task
    {
        daxa::TaskListInfo{
            .device = device,
            .dont_use_split_barriers = false,
            .swapchain = swapchain,
            .debug_name = "Clear and present",
        },
        {},
        {},
    }
{
    create_resources();
    record_tasks();
}

void Renderer::create_resources()
{
}

void Renderer::record_tasks()
{
    insert_or_throw(
        clear_present_task.task_images,
        {
            "task_swapchain_image",
            clear_present_task.task.create_task_image({
                .image = &images.swapchain_image,
                .swapchain_image = true,
                .debug_name = "task_swapchain_image" 
            }),
        }
    );

    clear_present_task.task.add_task({
        .used_images = 
        {
            {
                clear_present_task.task_images.at("task_swapchain_image"),
                daxa::TaskImageAccess::TRANSFER_WRITE,
                daxa::ImageMipArraySlice{}
            }
        },
        .task = [=, this](daxa::TaskRuntime const & runtime)
        {
            auto cmd_list = runtime.get_command_list();
            cmd_list.clear_image({
                .dst_image_layout = daxa::ImageLayout::TRANSFER_DST_OPTIMAL,
                .clear_value = {std::array<f32, 4>{1.0, 0.0, 0.0, 1.0}},
                .dst_image = runtime.get_image(clear_present_task.task_images.at("task_swapchain_image"))
            });
        },
        .debug_name = "clear swapchain",
    });

    clear_present_task.task.submit({});
    clear_present_task.task.present({});
    clear_present_task.task.complete();
}

void Renderer::draw() 
{
    images.swapchain_image = swapchain.acquire_next_image();
    if(images.swapchain_image.is_empty())
    {
        return;
    }
    clear_present_task.task.execute();
}

Renderer::~Renderer()
{
    device.wait_idle();
    device.collect_garbage();
}