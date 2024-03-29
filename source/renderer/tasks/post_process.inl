#pragma once

#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

#include "../shared/shared.inl"

struct PostProcessPC
{
    daxa_SamplerId sampler_id;
    daxa_SamplerId llce_sampler;
};

DAXA_DECL_TASK_USES_BEGIN(PostProcessTaskBase, DAXA_UNIFORM_BUFFER_SLOT0)
DAXA_TASK_USE_BUFFER(_average_luminance, daxa_BufferPtr(AverageLuminance), FRAGMENT_SHADER_READ)
DAXA_TASK_USE_IMAGE(_swapchain, REGULAR_2D, COLOR_ATTACHMENT)
DAXA_TASK_USE_IMAGE(_offscreen, REGULAR_2D, FRAGMENT_SHADER_SAMPLED)
DAXA_TASK_USE_IMAGE(_tonemapping_lut, REGULAR_3D, FRAGMENT_SHADER_SAMPLED)
DAXA_DECL_TASK_USES_END()

#if __cplusplus
#include "../context.hpp"

inline auto get_post_process_pipeline(const Context & context) -> daxa::RasterPipelineCompileInfo
{
    return {
        .vertex_shader_info = daxa::ShaderCompileInfo{ .source = daxa::ShaderFile{"screen_triangle.glsl"} },
        .fragment_shader_info = daxa::ShaderCompileInfo{ .source = daxa::ShaderFile{"post_process.glsl"} },
        .color_attachments = {{.format = context.swapchain.get_format()}},
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
        auto & cmd_list = ti.get_recorder();
        auto dimensions = context->swapchain.get_surface_extent();

        cmd_list.set_uniform_buffer(ti.uses.get_uniform_buffer_info());
        auto render_cmd_list = std::move(cmd_list).begin_renderpass({
            .color_attachments = {{
                .image_view = {uses._swapchain.view()},
                .load_op = daxa::AttachmentLoadOp::CLEAR,
                .clear_value = std::array<daxa_f32, 4>{1.0, 1.0, 0.0, 1.0}
            }},
            .depth_attachment = {},
            .render_area = {.x = 0, .y = 0, .width = dimensions.x , .height = dimensions.y}
        });

        render_cmd_list.set_pipeline(*(context->pipelines.post_process));
        render_cmd_list.push_constant(PostProcessPC{
            .sampler_id = context->linear_sampler,
            .llce_sampler = context->llce_sampler
        });
        render_cmd_list.draw({.vertex_count = 3});
        cmd_list = std::move(render_cmd_list).end_renderpass();
    }
};
#endif