#pragma once

#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

#include "../shared/shared.inl"

DAXA_DECL_TASK_USES_BEGIN(DebugDrawFrustumTaskBase, DAXA_UNIFORM_BUFFER_SLOT0)
DAXA_TASK_USE_BUFFER(_globals, daxa_BufferPtr(Globals), VERTEX_SHADER_READ)
DAXA_TASK_USE_BUFFER(_frustum_indices, daxa_BufferPtr(FrustumIndex), VERTEX_SHADER_READ)
DAXA_TASK_USE_BUFFER(_frustum_vertices, daxa_BufferPtr(FrustumVertex), VERTEX_SHADER_READ)
DAXA_TASK_USE_BUFFER(_frustum_colors, daxa_BufferPtr(FrustumColor), VERTEX_SHADER_READ)
DAXA_TASK_USE_BUFFER(_frustum_indirect, daxa_BufferPtr(DrawIndexedIndirectStruct), DRAW_INDIRECT_INFO_READ)
DAXA_TASK_USE_IMAGE(_swapchain, REGULAR_2D, COLOR_ATTACHMENT)
DAXA_TASK_USE_IMAGE(_depth, REGULAR_2D, DEPTH_ATTACHMENT)
DAXA_DECL_TASK_USES_END()

#if __cplusplus
#include "../context.hpp"

inline auto get_debug_draw_frustum_pipeline(Context const & context) -> daxa::RasterPipelineCompileInfo{
    return {
        .vertex_shader_info = daxa::ShaderCompileInfo{ .source = daxa::ShaderFile{"debug_draw_frustum.glsl"}, },
        .fragment_shader_info = daxa::ShaderCompileInfo{ .source = daxa::ShaderFile{"debug_draw_frustum.glsl"}, },
        .color_attachments = { {.format = context.swapchain.get_format()},},
        .depth_test = daxa::DepthTestInfo{ 
            .depth_attachment_format = daxa::Format::D32_SFLOAT,
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
    static constexpr daxa_u32 index_count = 18u;
    
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
                    .image_view = {uses._swapchain.view()},
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

        cmd_list.set_index_buffer({
            .id = uses._frustum_indices.buffer(),
            .offset = 0u,
            .index_type = daxa::IndexType::uint32
        });
        cmd_list.draw_indirect({
            .draw_command_buffer = uses._frustum_indirect.buffer(),
            .is_indexed = true
        });
        cmd_list.end_renderpass();
    }
};
#endif // __cplusplus