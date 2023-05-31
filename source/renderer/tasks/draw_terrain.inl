#pragma once

#include <daxa/daxa.inl>
#include <daxa/utils/task_list.inl>

#include "../shared/shared.inl"

struct DrawTerrainPC
{
    daxa_f32vec3 terrain_scale;
    daxa_f32 delta;
    daxa_f32 min_depth;
    daxa_f32 max_depth;
    daxa_i32 min_tess_level;
    daxa_i32 max_tess_level;
};

DAXA_INL_TASK_USE_BEGIN(DrawTerrainTaskBase, DAXA_CBUFFER_SLOT0)
DAXA_INL_TASK_USE_BUFFER(_vertices, daxa_BufferPtr(TerrainVertex), VERTEX_SHADER_READ)
DAXA_INL_TASK_USE_BUFFER(_indices, daxa_BufferPtr(TerrainIndex), VERTEX_SHADER_READ)
DAXA_INL_TASK_USE_BUFFER(_camera_parameters, daxa_BufferPtr(CameraParameters), SHADER_READ)
DAXA_INL_TASK_USE_IMAGE(_offscreen, daxa_Image2Df32, COLOR_ATTACHMENT)
DAXA_INL_TASK_USE_IMAGE(_depth, daxa_Image2Df32, DEPTH_ATTACHMENT)
DAXA_INL_TASK_USE_END()

#if __cplusplus
#include "../context.hpp"
inline auto get_draw_terrain_pipeline() -> daxa::RasterPipelineCompileInfo {
    return {
        .vertex_shader_info = daxa::ShaderCompileInfo{ .source = daxa::ShaderFile{"draw_terrain.glsl"}, },
        .tesselation_control_shader_info = daxa::ShaderCompileInfo{ .source = daxa::ShaderFile{"draw_terrain.glsl"}, },
        .tesselation_evaluation_shader_info = daxa::ShaderCompileInfo{ .source = daxa::ShaderFile{"draw_terrain.glsl"}, },
        .fragment_shader_info = daxa::ShaderCompileInfo{ .source = daxa::ShaderFile{"draw_terrain.glsl"}, },
        .color_attachments = {{.format = daxa::Format::R16G16B16A16_SFLOAT}},
        .depth_test = { 
            .depth_attachment_format = daxa::Format::D32_SFLOAT,
            .enable_depth_test = true,
            .enable_depth_write = true,
        },
        .raster = { 
            .primitive_topology = daxa::PrimitiveTopology::PATCH_LIST,
            .primitive_restart_enable = false,
            .polygon_mode = daxa::PolygonMode::LINE,
            .face_culling = daxa::FaceCullFlagBits::BACK_BIT,
        },
        .tesselation = { .control_points = 3 },
        .push_constant_size = sizeof(DrawTerrainPC),
        .name = "terrain pipeline"
    };
}

struct DrawTerrainTask : DrawTerrainTaskBase
{
    Context *context = {};
    void callback(daxa::TaskInterface ti)
    {
        auto cmd_list = ti.get_command_list();

        auto dimensions = context->swapchain.get_surface_extent();

        cmd_list.set_constant_buffer(ti.uses.constant_buffer_set_info());
        cmd_list.begin_renderpass({
            .color_attachments = 
            {{
                .image_view = {uses._offscreen.view()},
                .load_op = daxa::AttachmentLoadOp::CLEAR,
                .clear_value = std::array<f32, 4>{0.0, 0.0, 0.0, 1.0}
            }},
            .depth_attachment = 
            {{
                .image_view = {uses._depth.view()},
                .layout = daxa::ImageLayout::ATTACHMENT_OPTIMAL,
                .load_op = daxa::AttachmentLoadOp::CLEAR,
                .store_op = daxa::AttachmentStoreOp::STORE,
                .clear_value = daxa::ClearValue{daxa::DepthValue{1.0f, 0}},
            }},
            .render_area = {.x = 0, .y = 0, .width = dimensions.x , .height = dimensions.y}
        });

        cmd_list.set_pipeline(*(context->pipelines.draw_terrain));
        cmd_list.push_constant(DrawTerrainPC{
            .terrain_scale = context->terrain_params.scale,
            .delta = context->terrain_params.delta,
            .min_depth = context->terrain_params.min_depth,
            .max_depth = context->terrain_params.max_depth,
            .min_tess_level = context->terrain_params.min_tess_level,
            .max_tess_level = context->terrain_params.max_tess_level
        });
        cmd_list.set_index_buffer(uses._indices.buffer(), 0, sizeof(u32));
        cmd_list.draw_indexed({.index_count = u32(context->buffers.terrain_indices.cpu_buffer.size())});
        cmd_list.end_renderpass();
    }
};
#endif