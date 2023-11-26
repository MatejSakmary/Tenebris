#pragma once

#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

#include "../shared/shared.inl"

struct VSMDrawPagesPC
{
    daxa_SamplerId llb_sampler;
    daxa_ImageViewId daxa_u32_vsm_memory_view;
};

DAXA_DECL_TASK_USES_BEGIN(VSMDrawPagesTaskBase, DAXA_UNIFORM_BUFFER_SLOT0)
DAXA_TASK_USE_BUFFER(_globals, daxa_BufferPtr(Globals), GRAPHICS_SHADER_READ)
DAXA_TASK_USE_BUFFER(_vertices, daxa_BufferPtr(TerrainVertex), VERTEX_SHADER_READ)
DAXA_TASK_USE_BUFFER(_indices, daxa_BufferPtr(TerrainIndex), VERTEX_SHADER_READ)
DAXA_TASK_USE_BUFFER(_vsm_sun_projections, daxa_BufferPtr(VSMClipProjection), GRAPHICS_SHADER_READ)
DAXA_TASK_USE_IMAGE(_height_map, REGULAR_2D, GRAPHICS_SHADER_SAMPLED)
DAXA_TASK_USE_IMAGE(_debug, REGULAR_2D, COLOR_ATTACHMENT)
DAXA_TASK_USE_IMAGE(_vsm_page_table, REGULAR_2D_ARRAY, GRAPHICS_SHADER_STORAGE_READ_ONLY)
DAXA_TASK_USE_IMAGE(_vsm_memory, REGULAR_2D, FRAGMENT_SHADER_STORAGE_READ_WRITE)
DAXA_DECL_TASK_USES_END()

#if __cplusplus
#include "../context.hpp"
inline auto get_vsm_draw_pages_pipeline() -> daxa::RasterPipelineCompileInfo {
    const auto shader_compile_info = daxa::ShaderCompileInfo{ .source = daxa::ShaderFile{"vsm_draw_pages.glsl"}, };
    return {
        .vertex_shader_info = shader_compile_info,
        .tesselation_control_shader_info = shader_compile_info,
        .tesselation_evaluation_shader_info = shader_compile_info,
        .fragment_shader_info = shader_compile_info,
        .color_attachments = {{.format = daxa::Format::R16G16B16A16_SFLOAT}}, // g_albedo
        .raster = {
            .primitive_topology = daxa::PrimitiveTopology::PATCH_LIST,
            .primitive_restart_enable = false,
            .polygon_mode = daxa::PolygonMode::FILL,
            .face_culling = daxa::FaceCullFlagBits::BACK_BIT,
            // .face_culling = daxa::FaceCullFlagBits::FRONT_BIT,
            .depth_clamp_enable = true,
        },
        .tesselation = { .control_points = 4 },
        .push_constant_size = static_cast<daxa_u32>(sizeof(VSMDrawPagesPC)),
        .name = "vsm draw pages pipeline"
    };
}

struct VSMDrawPagesTask : VSMDrawPagesTaskBase
{
    Context * context = {};
    void callback(daxa::TaskInterface ti)
    {
        const auto resolution = daxa_u32vec2{VSM_TEXTURE_RESOLUTION, VSM_TEXTURE_RESOLUTION};

        daxa::ImageViewId daxa_u32_image_view = context->device.create_image_view({
            .type = daxa::ImageViewType::REGULAR_2D,
            .format = daxa::Format::R32_UINT,
            .image = uses._vsm_memory.image(),
            .name = "vsm memory daxa_u32 view"
        });

        auto & cmd_list = ti.get_recorder();
        cmd_list.destroy_image_view_deferred(daxa_u32_image_view);
        cmd_list.set_uniform_buffer(ti.uses.get_uniform_buffer_info());
        auto render_cmd_list = std::move(cmd_list).begin_renderpass({
            .color_attachments = 
            {
                {
                    .image_view = {uses._debug.view()},
                    .load_op = daxa::AttachmentLoadOp::CLEAR,
                    .clear_value = std::array<daxa_f32, 4>{0.0, 0.0, 0.0, 1.0}
                },
            },
            .render_area = {.x = 0, .y = 0, .width = resolution.x, .height = resolution.y}
        });
        render_cmd_list.set_pipeline(*(context->pipelines.vsm_draw_pages));
        render_cmd_list.push_constant(VSMDrawPagesPC{
            .llb_sampler = context->linear_sampler,
            .daxa_u32_vsm_memory_view = daxa_u32_image_view
        });
        render_cmd_list.set_index_buffer({
            .id = uses._indices.buffer(),
            .offset = 0,
            .index_type = daxa::IndexType::uint32 
        });
        render_cmd_list.draw_indexed({
            .index_count = static_cast<daxa_u32>(context->terrain_index_size),
            .instance_count = VSM_CLIP_LEVELS
        });
        cmd_list = std::move(render_cmd_list).end_renderpass();
    }
};
#endif //__cplusplus

