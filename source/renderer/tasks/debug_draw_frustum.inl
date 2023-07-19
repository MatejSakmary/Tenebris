#pragma once

#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

#include "../shared/shared.inl"

DAXA_DECL_TASK_USES_BEGIN(DebugDrawFrustumTaskBase, DAXA_UNIFORM_BUFFER_SLOT0)
DAXA_TASK_USE_BUFFER(_globals, daxa_BufferPtr(Globals), VERTEX_SHADER_READ)
DAXA_TASK_USE_BUFFER(_frustum_vertices, daxa_BufferPtr(FrustumVertex), VERTEX_SHADER_READ)
DAXA_TASK_USE_BUFFER(_frustum_indices, daxa_BufferPtr(FrustumIndex), VERTEX_SHADER_READ)
DAXA_TASK_USE_IMAGE(_g_albedo, REGULAR_2D, COLOR_ATTACHMENT)
DAXA_TASK_USE_IMAGE(_g_normals, REGULAR_2D, COLOR_ATTACHMENT)
DAXA_TASK_USE_IMAGE(_depth, REGULAR_2D, DEPTH_ATTACHMENT)
DAXA_DECL_TASK_USES_END()

#if __cplusplus
#include "../context.hpp"

inline auto get_debug_draw_frustum_pipeline() -> daxa::RasterPipelineCompileInfo{
    return {
        .vertex_shader_info = daxa::ShaderCompileInfo{ .source = daxa::ShaderFile{"debug_draw_frustum.glsl"}, },
        .fragment_shader_info = daxa::ShaderCompileInfo{ .source = daxa::ShaderFile{"debug_draw_frustum.glsl"}, },
        .color_attachments = {
            {.format = daxa::Format::R16G16B16A16_SFLOAT}, // g_albedo
            {.format = daxa::Format::R16G16B16A16_SFLOAT}, // g_normals
        },
        .depth_test = { 
            .depth_attachment_format = daxa::Format::D32_SFLOAT,
            .enable_depth_test = false,
            .enable_depth_write = true,
            .depth_test_compare_op = daxa::CompareOp::GREATER_OR_EQUAL,
        },
        .raster = {
            .primitive_topology = daxa::PrimitiveTopology::LINE_STRIP,
            .primitive_restart_enable = true,
            .polygon_mode = daxa::PolygonMode::LINE
        },
        .name = "debug draw frustum"
    };
}

struct DebugDrawFrustumTask : DebugDrawFrustumTaskBase
{
    static constexpr u32 index_count = 18u;
    
    Context * context = {};
    void callback(daxa::TaskInterface ti)
    {
        auto cmd_list = ti.get_command_list();

        auto dimensions = context->swapchain.get_surface_extent();

        cmd_list.set_uniform_buffer(ti.uses.get_uniform_buffer_info());
        cmd_list.begin_renderpass({
            .color_attachments = 
            {
                {
                    .image_view = {uses._g_albedo.view()},
                    .load_op = daxa::AttachmentLoadOp::LOAD,
                },
                {
                    .image_view = {uses._g_normals.view()},
                    .load_op = daxa::AttachmentLoadOp::LOAD,
                },
            },
            .depth_attachment = 
            {{
                .image_view = {uses._depth.view()},
                .layout = daxa::ImageLayout::ATTACHMENT_OPTIMAL,
                .load_op = daxa::AttachmentLoadOp::LOAD,
                .store_op = daxa::AttachmentStoreOp::STORE,
            }},
            .render_area = {.x = 0, .y = 0, .width = dimensions.x , .height = dimensions.y}
        });

        cmd_list.set_pipeline(*(context->pipelines.debug_draw_frustum)); 

        cmd_list.set_index_buffer(uses._frustum_indices.buffer(), 0, sizeof(u32));
        cmd_list.draw_indexed({.index_count = 18});
        cmd_list.end_renderpass();
    }
};
#endif // __cplusplus