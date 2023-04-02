#pragma once

#include <daxa/daxa.hpp>
#include <daxa/utils/task_list.hpp>

#include "../context.hpp"
#include "../shared/shared.inl"

inline auto get_draw_far_sky_pipeline(const Context & context) -> daxa::RasterPipelineCompileInfo
{
    return {
        .vertex_shader_info = daxa::ShaderCompileInfo{ .source = daxa::ShaderFile{"screen_triangle.glsl"} },
        .fragment_shader_info = daxa::ShaderCompileInfo{ .source = daxa::ShaderFile{"draw_far_sky.glsl"} },
        .color_attachments = {{.format = daxa::Format::R16G16B16A16_SFLOAT}},
        .depth_test = 
        { 
            .depth_attachment_format = daxa::Format::D32_SFLOAT,
            .enable_depth_test = true,
            .enable_depth_write = false,
            .depth_test_compare_op = daxa::CompareOp::EQUAL
        },
        .raster = 
        { 
            .polygon_mode = daxa::PolygonMode::FILL,
            .face_culling = daxa::FaceCullFlagBits::BACK_BIT,
        },
        .push_constant_size = sizeof(DrawSkyPC),
        .debug_name = "far sky pipeline"
    };
}

inline void task_draw_far_sky(Context & context)
{
    context.main_task_list.task_list.add_task({
        .used_buffers =
        { 
            {
                context.main_task_list.task_buffers.t_atmosphere_parameters,
                daxa::TaskBufferAccess::FRAGMENT_SHADER_READ_ONLY 
            },
            {
                context.main_task_list.task_buffers.t_camera_parameters,
                daxa::TaskBufferAccess::FRAGMENT_SHADER_READ_ONLY 
            } 
        },
        .used_images = 
        { 
            { 
                context.main_task_list.task_images.at(Images::OFFSCREEN),
                daxa::TaskImageAccess::COLOR_ATTACHMENT,
                daxa::ImageMipArraySlice{}
            },
            { 
                context.main_task_list.task_images.at(Images::DEPTH),
                daxa::TaskImageAccess::DEPTH_ATTACHMENT,
                daxa::ImageMipArraySlice{.image_aspect = daxa::ImageAspectFlagBits::DEPTH} 
            },
            { 
                context.main_task_list.task_images.at(Images::SKYVIEW),
                daxa::TaskImageAccess::SHADER_READ_ONLY,
                daxa::ImageMipArraySlice{} 
            }
        },
        .task = [&](daxa::TaskRuntimeInterface const & runtime)
        {
            auto cmd_list = runtime.get_command_list();
            auto dimensions = context.swapchain.get_surface_extent();

            auto skyview_image = runtime.get_images(context.main_task_list.task_images.at(Images::SKYVIEW))[0];
            auto offscreen_image = runtime.get_images(context.main_task_list.task_images.at(Images::OFFSCREEN))[0];
            auto depth_image = runtime.get_images(context.main_task_list.task_images.at(Images::DEPTH))[0];

            auto atmosphere_gpu_buffer = runtime.get_buffers(context.main_task_list.task_buffers.t_atmosphere_parameters)[0];
            auto camera_gpu_buffer = runtime.get_buffers(context.main_task_list.task_buffers.t_camera_parameters)[0];

            auto skyview_dimensions = context.device.info_image(skyview_image).size;

            cmd_list.begin_renderpass({
                .color_attachments = 
                {{
                    .image_view = offscreen_image.default_view(),
                    .load_op = daxa::AttachmentLoadOp::LOAD
                }},
                .depth_attachment = 
                {{
                    .image_view = depth_image.default_view(),
                    .layout = daxa::ImageLayout::ATTACHMENT_OPTIMAL,
                    .load_op = daxa::AttachmentLoadOp::LOAD,
                    .store_op = daxa::AttachmentStoreOp::STORE,
                }},
                .render_area = {.x = 0, .y = 0, .width = dimensions.x , .height = dimensions.y}
            });

            cmd_list.set_pipeline(*context.pipelines.draw_far_sky);
            cmd_list.push_constant(DrawSkyPC{
                .skyview_image = skyview_image.default_view(),
                .sampler_id = context.linear_sampler,
                .skyview_dimensions = {skyview_dimensions.x, skyview_dimensions.y},
                .atmosphere_parameters = context.device.get_device_address(atmosphere_gpu_buffer),
                .camera_parameters = context.device.get_device_address(camera_gpu_buffer)
            });
            cmd_list.draw({.vertex_count = 3});
            cmd_list.end_renderpass();
        },
        .debug_name = "draw far sky",
    });
}