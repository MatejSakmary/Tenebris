#pragma once

#include <daxa/daxa.inl>
#include <daxa/utils/task_list.inl>

#include "../shared/shared.inl"

struct PostProcessPC
{
    daxa_SamplerId sampler_id;
};

DAXA_INL_TASK_USE_BEGIN(PostProcessTaskBase, DAXA_CBUFFER_SLOT0)
DAXA_INL_TASK_USE_IMAGE(_swapchain, daxa_Image2Du32, FRAGMENT_SHADER_WRITE)
DAXA_INL_TASK_USE_IMAGE(_offscreen, daxa_Image2Df32, FRAGMENT_SHADER_READ)
DAXA_INL_TASK_USE_END()

#if __cplusplus
#include "../context.hpp"

inline auto get_post_process_pipeline(const Context & context) -> daxa::RasterPipelineCompileInfo
{
    return {
        .vertex_shader_info = daxa::ShaderCompileInfo{ .source = daxa::ShaderFile{"screen_triangle.glsl"} },
        .fragment_shader_info = daxa::ShaderCompileInfo{ .source = daxa::ShaderFile{"post_process.glsl"} },
        .color_attachments = {{.format = context.swapchain.get_format()}},
        .depth_test = { .enable_depth_test = false },
        .raster = { 
            .polygon_mode = daxa::PolygonMode::FILL,
            .face_culling = daxa::FaceCullFlagBits::BACK_BIT,
        },
        .push_constant_size = sizeof(PostProcessPC),
        .name = "post process pipeline"
    };
}

struct PostProcessTask : PostProcessTaskBase
{
    Context * context = {};
    void callback(daxa::TaskInterface ti)
    {
        auto cmd_list = ti.get_command_list();
        auto dimensions = context->swapchain.get_surface_extent();

        cmd_list.set_constant_buffer(ti.uses.constant_buffer_set_info());
        cmd_list.begin_renderpass({
            .color_attachments = {{
                .image_view = {uses._swapchain.view()},
                .load_op = daxa::AttachmentLoadOp::CLEAR,
                .clear_value = std::array<f32, 4>{1.0, 1.0, 0.0, 1.0}
            }},
            .depth_attachment = {},
            .render_area = {.x = 0, .y = 0, .width = dimensions.x , .height = dimensions.y}
        });

        cmd_list.set_pipeline(*(context->pipelines.post_process));
        cmd_list.push_constant(PostProcessPC{ .sampler_id = context->linear_sampler });
        cmd_list.draw({.vertex_count = 3});
        cmd_list.end_renderpass();
    }
};
#endif