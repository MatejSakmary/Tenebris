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
    daxa_ctx{daxa::create_context({.enable_validation = false})},
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
    ApplicationTask & clear_task = insert_or_throw(
        tasks,
        {
            "clear_present",
            {
                daxa::TaskList({
                    .device = device,
                    .dont_use_split_barriers = false,
                    .swapchain = swapchain,
                    .debug_name = "Clear and present"
                }), {}, {}
            }
        }
    );

    insert_or_throw(
        clear_task.task_images,
        {
            "task_swapchain_image",
            clear_task.task.create_task_image({
                .image = &images.swapchain_image,
                .swapchain_image = true,
                .debug_name = "task_swapchain_image" 
            }),
        }
    );

    clear_task.task.add_task({
        .used_images = 
        {
            {
                clear_task.task_images.at("task_swapchain_image"),
                daxa::TaskImageAccess::COLOR_ATTACHMENT,
                daxa::ImageMipArraySlice{}
            }
        },
        .task = [=, this](daxa::TaskRuntime const & runtime)
        {
            auto cmd_list = runtime.get_command_list();
            cmd_list.clear_image({
                .dst_image_layout = daxa::ImageLayout::PRESENT_SRC,
                .clear_value = {std::array<f32, 4>{1.0, 0.0, 0.0, 1.0}},
                .dst_image = runtime.get_image(clear_task.task_images.at("task_swapchain_image"))
            });
        },
        .debug_name = "clear swapchain",
    });

    clear_task.task.submit({});
    clear_task.task.present({});
    clear_task.task.complete();
}

void Renderer::draw() 
{
    images.swapchain_image = swapchain.acquire_next_image();
    if(images.swapchain_image.is_empty())
    {
        return;
    }
    tasks.at("clear_present").task.execute();
}

Renderer::~Renderer() {}