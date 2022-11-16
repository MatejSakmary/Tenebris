#include "renderer.hpp"
#include <string.h>

Renderer::Renderer(daxa::NativeWindowHandle window) :
    daxa_context{daxa::create_context({.enable_validation = true})},
    daxa_device{daxa_context.create_device({.debug_name = "Daxa device"})}
{
    daxa_swapchain = daxa_device.create_swapchain({ 
        .native_window = window,
        .native_window_platform = daxa::NativeWindowPlatform::UNKNOWN,
        .present_mode = daxa::PresentMode::DOUBLE_BUFFER_WAIT_FOR_VBLANK,
        .image_usage = daxa::ImageUsageFlagBits::TRANSFER_DST | daxa::ImageUsageFlagBits::COLOR_ATTACHMENT,
        .debug_name = "Swapchain",
    });

    daxa_pipeline_compiler = daxa_device.create_pipeline_compiler({
        .shader_compile_options = {
            .root_paths = { DAXA_SHADER_INCLUDE_DIR, "source", "shaders",},
            .language = daxa::ShaderLanguage::GLSL,
        },
        .debug_name = "Pipeline Compiler",
    });

    default_sampler = daxa_device.create_sampler({});

    pipelines.transmittance_pipeline = daxa_pipeline_compiler.create_compute_pipeline({
        .shader_info = { .source = daxa::ShaderFile{"transmittance.glsl"} },
        .push_constant_size = sizeof(TransmittancePush),
        .debug_name = "transmittance"}).value();

    pipelines.finalpass_pipeline = daxa_pipeline_compiler.create_raster_pipeline({
        .vertex_shader_info = { .source = daxa::ShaderFile{"screen_triangle.glsl"} },
        .fragment_shader_info = { .source = daxa::ShaderFile{"final_pass.glsl"} },
        .color_attachments = {{.format = daxa_swapchain.get_format()}},
        .depth_test = { .enable_depth_test = false },
        .raster = {
            .polygon_mode = daxa::PolygonMode::FILL,
            .face_culling = daxa::FaceCullFlagBits::BACK_BIT,
        },
        .push_constant_size = sizeof(FinalPassPush),
        .debug_name = "finalpass"
    }).value();

    create_resources();
    record_tasks();
}

void Renderer::resize()
{
    daxa_swapchain.resize();
}

void Renderer::draw() 
{
    auto reload_raster_pipeline = [this](daxa::RasterPipeline & pipeline) -> bool
    {
        if (daxa_pipeline_compiler.check_if_sources_changed(pipeline))
        {
            auto new_pipeline = daxa_pipeline_compiler.recreate_raster_pipeline(pipeline);
            std::cout << new_pipeline.to_string() << std::endl;
            if (new_pipeline.is_ok())
            {
                pipeline = new_pipeline.value();
                return true;
            }
        }
        return false;
    };
    auto reload_compute_pipeline = [this](daxa::ComputePipeline & pipeline) -> bool
    {
        if (daxa_pipeline_compiler.check_if_sources_changed(pipeline))
        {
            auto new_pipeline = daxa_pipeline_compiler.recreate_compute_pipeline(pipeline);
            std::cout << new_pipeline.to_string() << std::endl;
            if (new_pipeline.is_ok())
            {
                pipeline = new_pipeline.value();
                return true;
            }
        }
        return false;
    };

    daxa_tasks.clear_present.remove_runtime_image(daxa_task_images.swapchain_image, daxa_images.swapchain_image);
    daxa_images.swapchain_image = daxa_swapchain.acquire_next_image();
    daxa_tasks.clear_present.add_runtime_image(daxa_task_images.swapchain_image, daxa_images.swapchain_image);
    if(daxa_images.swapchain_image.is_empty())
    {
        return;
    }
    daxa_tasks.render_sky.execute();
    daxa_tasks.clear_present.execute();
    reload_raster_pipeline(pipelines.finalpass_pipeline);
    reload_compute_pipeline(pipelines.transmittance_pipeline);
}

void Renderer::create_resources()
{
    daxa_images.transmittance = daxa_device.create_image({
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

    daxa_buffers.atmosphere_parameters.gpu_buffer = daxa_device.create_buffer(daxa::BufferInfo{
        .size = sizeof(AtmosphereParameters),
        .debug_name = "atmosphere_parameters",
    }); 

    f32 mie_scale_height = 1.2f;
    f32 rayleigh_scale_height = 8.0f;
    daxa_buffers.atmosphere_parameters.cpu_buffer = {
        .atmosphere_bottom = 6360.0f,
        .atmosphere_top = 6460.0f,
        .mie_scattering = { 0.003996f, 0.003996f, 0.003996f },
        .mie_extinction = { 0.004440f, 0.004440f, 0.004440f },
        .mie_scale_height = mie_scale_height,
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

void Renderer::record_tasks()
{
    record_render_sky_task();
    record_clear_present_task();
}

void Renderer::record_render_sky_task()  
{
    daxa_tasks.render_sky = daxa::TaskList({
        .device = daxa_device,
        .dont_reorder_tasks = false,
        .dont_use_split_barriers = false,
        .swapchain = daxa_swapchain,
        .debug_name = "render_sky"
    });

    daxa_task_images.transmittance = daxa_tasks.render_sky.create_task_image({
        .initial_access = daxa::AccessConsts::NONE,
        .initial_layout = daxa::ImageLayout::UNDEFINED,
        .swapchain_image = false,
        .debug_name = "transmittance"
    });

    daxa_task_buffers.atmosphere_parameters = daxa_tasks.render_sky.create_task_buffer({
        .initial_access = daxa::AccessConsts::NONE,
        .debug_name = "atmosphere_parameters"
    });

    daxa_tasks.render_sky.add_runtime_image(daxa_task_images.transmittance, daxa_images.transmittance);

    daxa_tasks.render_sky.add_task({
        .used_buffers = { {daxa_task_buffers.atmosphere_parameters, daxa::TaskBufferAccess::HOST_TRANSFER_WRITE } },
        .task = [this](daxa::TaskRuntime const & runtime)
        {
            auto cmd_list = runtime.get_command_list();
            // create staging buffer
            auto staging_atmosphere_gpu_buffer = daxa_device.create_buffer({
                .memory_flags = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                .size = sizeof(AtmosphereParameters),
                .debug_name = "staging_atmosphere_gpu_buffer"
            });
            // copy data into staging buffer
            auto buffer_ptr = daxa_device.map_memory_as<AtmosphereParameters>(staging_atmosphere_gpu_buffer);
            memcpy(buffer_ptr, &daxa_buffers.atmosphere_parameters.cpu_buffer, sizeof(AtmosphereParameters));
            daxa_device.unmap_memory(staging_atmosphere_gpu_buffer);
            // copy staging buffer into gpu_buffer
            cmd_list.copy_buffer_to_buffer({
                .src_buffer = staging_atmosphere_gpu_buffer,
                .dst_buffer = daxa_buffers.atmosphere_parameters.gpu_buffer,
                .size = sizeof(AtmosphereParameters),
            });
            // destroy the stagin buffer after the copy is done
            cmd_list.destroy_buffer_deferred(staging_atmosphere_gpu_buffer);
        },
        .debug_name = "upload input"
    }); 

    daxa_tasks.render_sky.add_task({
        .used_buffers = { {daxa_task_buffers.atmosphere_parameters, daxa::TaskBufferAccess::COMPUTE_SHADER_READ_ONLY } },
        .used_images = { {daxa_task_images.transmittance, daxa::TaskImageAccess::COMPUTE_SHADER_WRITE_ONLY, daxa::ImageMipArraySlice{} }},
        .task = [this](daxa::TaskRuntime const & runtime)
        {
            auto cmd_list = runtime.get_command_list();
            auto image_dimensions = daxa_device.info_image(daxa_images.transmittance).size;
            cmd_list.set_pipeline(pipelines.transmittance_pipeline);
            cmd_list.push_constant(TransmittancePush{
                .transmittance_image = daxa_images.transmittance.default_view(),
                .dimensions = {image_dimensions.x, image_dimensions.y},
                .atmosphere_parameters = this->daxa_device.get_device_address(daxa_buffers.atmosphere_parameters.gpu_buffer)
            });
            cmd_list.dispatch(((image_dimensions.x + 7)/8), ((image_dimensions.y + 3)/4));
        },
        .debug_name = "render_transmittance"
    });
    daxa_tasks.render_sky.submit({});
}

void Renderer::record_clear_present_task()
{
    // Create tasklist
    daxa_tasks.clear_present = daxa::TaskList({
        .device = daxa_device,
        .dont_reorder_tasks = false,
        .dont_use_split_barriers = false,
        .swapchain = daxa_swapchain,
        .debug_name = "clear_present"
    });
    
    // Create tasklist swapchain image
    daxa_task_images.swapchain_image = daxa_tasks.clear_present.create_task_image({
        .initial_access = daxa::AccessConsts::NONE,
        .initial_layout = daxa::ImageLayout::UNDEFINED,
        .swapchain_image = true,
        .debug_name = "task_swapchain_image"
    });

    daxa_task_images.transmittance_sampled = daxa_tasks.clear_present.create_task_image({
        .initial_access = daxa::AccessConsts::COMPUTE_SHADER_WRITE,
        .initial_layout = daxa::ImageLayout::GENERAL,
        .swapchain_image = false,
        .debug_name = "transmittance sampled image"
    });

    daxa_tasks.clear_present.add_runtime_image(daxa_task_images.transmittance_sampled, daxa_images.transmittance);

    daxa_tasks.clear_present.add_task({
        .used_images = { { daxa_task_images.swapchain_image, daxa::TaskImageAccess::SHADER_WRITE_ONLY, daxa::ImageMipArraySlice{} },
                         { daxa_task_images.transmittance_sampled, daxa::TaskImageAccess::SHADER_READ_ONLY, daxa::ImageMipArraySlice{} },},
        .task = [this](daxa::TaskRuntime const & runtime)
        {
            auto cmd_list = runtime.get_command_list();
            auto dimensions = daxa_swapchain.get_surface_extent();
            cmd_list.begin_renderpass({
                .color_attachments = {{
                    .image_view = daxa_images.swapchain_image.default_view(),
                    .load_op = daxa::AttachmentLoadOp::CLEAR,
                    .clear_value = std::array<f32, 4>{1.0, 1.0, 0.0, 1.0}
                }},
                .depth_attachment = {},
                .render_area = {.x = 0, .y = 0, .width = dimensions.x , .height = dimensions.y}
            });

            cmd_list.set_pipeline(pipelines.finalpass_pipeline);
            cmd_list.push_constant(FinalPassPush{
                .image_id = daxa_images.transmittance.default_view(),
                .sampler_id = default_sampler
            });
            cmd_list.draw({.vertex_count = 6});
            cmd_list.end_renderpass();
        },
        .debug_name = "display texture",
    });

    daxa_tasks.clear_present.submit({});
    daxa_tasks.clear_present.present({});
    daxa_tasks.clear_present.complete();
}

Renderer::~Renderer()
{
    daxa_device.wait_idle();
    daxa_device.destroy_buffer(daxa_buffers.atmosphere_parameters.gpu_buffer);
    daxa_device.destroy_image(daxa_images.transmittance);
    daxa_device.destroy_sampler(default_sampler);
    daxa_device.collect_garbage();
}