#pragma once

#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

#include "../shared/shared.inl"

struct DrawTerrainShadowmapPC
{
    daxa_SamplerId linear_sampler_id;
};

DAXA_DECL_TASK_USES_BEGIN(TerrainShadowmapTaskBase, DAXA_UNIFORM_BUFFER_SLOT0)
DAXA_TASK_USE_BUFFER(_vertices, daxa_BufferPtr(TerrainVertex), VERTEX_SHADER_READ)
DAXA_TASK_USE_BUFFER(_indices, daxa_BufferPtr(TerrainIndex), VERTEX_SHADER_READ)
DAXA_TASK_USE_BUFFER(_globals, daxa_BufferPtr(Globals), SHADER_READ)
DAXA_TASK_USE_IMAGE(_shadowmap, REGULAR_2D, DEPTH_ATTACHMENT)
DAXA_TASK_USE_IMAGE(_height_map, REGULAR_2D, SHADER_READ)
DAXA_DECL_TASK_USES_END()

#if __cplusplus
#include "../context.hpp"
inline auto get_terrain_shadowmap_pipeline() -> daxa::RasterPipelineCompileInfo {
    daxa::ShaderCompileOptions options = { .defines = {{"SHADOWMAP_DRAW","1"}} };
    return {
        .vertex_shader_info = daxa::ShaderCompileInfo{ .source = daxa::ShaderFile{"draw_terrain.glsl"}, .compile_options = options },
        .tesselation_control_shader_info = daxa::ShaderCompileInfo{ .source = daxa::ShaderFile{"draw_terrain.glsl"}, .compile_options = options },
        .tesselation_evaluation_shader_info = daxa::ShaderCompileInfo{ .source = daxa::ShaderFile{"draw_terrain.glsl"}, .compile_options = options },
        .fragment_shader_info = daxa::ShaderCompileInfo{ .source = daxa::ShaderFile{"draw_terrain.glsl"}, .compile_options = options },
        .depth_test = {
            .depth_attachment_format = daxa::Format::D32_SFLOAT,
            .enable_depth_test = true,
            .enable_depth_write = true
        },
        .raster = { 
            .primitive_topology = daxa::PrimitiveTopology::PATCH_LIST,
            .primitive_restart_enable = false,
            .polygon_mode = daxa::PolygonMode::FILL,
            .face_culling = daxa::FaceCullFlagBits::FRONT_BIT,
        },
        .tesselation = { .control_points = 4 },
        .push_constant_size = sizeof(DrawTerrainShadowmapPC),
        .name = "terrain shadowmap pipeline"
    };
}

struct TerrainShadowmapTask : TerrainShadowmapTaskBase
{
    Context *context = {};

    void callback(daxa::TaskInterface ti)
    {
        auto cmd_list = ti.get_command_list();

        auto dimensions = context->device.info_image(uses._shadowmap.image()).size;

        cmd_list.set_uniform_buffer(ti.uses.get_uniform_buffer_info());
        cmd_list.begin_renderpass({
            .depth_attachment = 
            {{
                .image_view = {uses._shadowmap.view()},
                .layout = daxa::ImageLayout::ATTACHMENT_OPTIMAL,
                .load_op = daxa::AttachmentLoadOp::CLEAR,
                .store_op = daxa::AttachmentStoreOp::STORE,
                .clear_value = daxa::ClearValue{daxa::DepthValue{1.0f, 0}},
            }},
            .render_area = {.x = 0, .y = 0, .width = dimensions.x, .height = dimensions.y}
        });
        cmd_list.set_pipeline(*(context->pipelines.draw_terrain_shadowmap));
        cmd_list.set_index_buffer(uses._indices.buffer(), 0, sizeof(u32));
        cmd_list.push_constant(DrawTerrainShadowmapPC{ .linear_sampler_id = context->linear_sampler});
        cmd_list.draw_indexed({.index_count = static_cast<u32>(context->terrain_index_size)});
        cmd_list.end_renderpass();
    }
};
#endif //__cplusplus