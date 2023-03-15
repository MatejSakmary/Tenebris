#pragma once

#include <daxa/daxa.hpp>
#include <daxa/utils/task_list.hpp>

#include "../context.hpp"
#include "../shared/shared.inl"

inline auto get_post_process_pipeline(const Context & context) -> daxa::RasterPipelineCompileInfo
{
    return {
        .vertex_shader_info = { .source = daxa::ShaderFile{"screen_triangle.glsl"} },
        .fragment_shader_info = { .source = daxa::ShaderFile{"post_process.glsl"} },
        .color_attachments = {{.format = context.swapchain.get_format()}},
        .depth_test = { .enable_depth_test = false },
        .raster = { 
            .polygon_mode = daxa::PolygonMode::FILL,
            .face_culling = daxa::FaceCullFlagBits::BACK_BIT,
        },
        .push_constant_size = sizeof(PostProcessPC),
        .debug_name = "post process pipeline"
    };
}

inline void task_post_process(Context & context)
{
    context.main_task_list.task_list.add_task({
        .used_images = 
        { 
            { 
                context.main_task_list.task_images.at(Images::SWAPCHAIN),
                daxa::TaskImageAccess::SHADER_WRITE_ONLY,
                daxa::ImageMipArraySlice{}
            },
            { 
                context.main_task_list.task_images.at(Images::OFFSCREEN),
                daxa::TaskImageAccess::SHADER_READ_ONLY,
                daxa::ImageMipArraySlice{} 
            },
        },
        .task = [&](daxa::TaskRuntimeInterface const & runtime)
        {
            auto cmd_list = runtime.get_command_list();
            auto dimensions = context.swapchain.get_surface_extent();

            auto swapchain_image = runtime.get_images(context.main_task_list.task_images.at(Images::SWAPCHAIN))[0];
            auto offscreen_image = runtime.get_images(context.main_task_list.task_images.at(Images::OFFSCREEN))[0];

            cmd_list.begin_renderpass({
                .color_attachments = {{
                    .image_view = swapchain_image.default_view(),
                    .load_op = daxa::AttachmentLoadOp::CLEAR,
                    .clear_value = std::array<f32, 4>{1.0, 1.0, 0.0, 1.0}
                }},
                .depth_attachment = {},
                .render_area = {.x = 0, .y = 0, .width = dimensions.x , .height = dimensions.y}
            });

            cmd_list.set_pipeline(*context.pipelines.post_process);
            cmd_list.push_constant(PostProcessPC{
                .offscreen_id = offscreen_image.default_view(),
                .sampler_id = context.linear_sampler
            });
            cmd_list.draw({.vertex_count = 3});
            cmd_list.end_renderpass();
        },
        .debug_name = "post process",
    });
}