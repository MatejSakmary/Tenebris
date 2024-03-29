#pragma once

#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

#include "../shared/shared.inl"

struct DrawTerrainShadowmapPC
{
    daxa_SamplerId linear_sampler_id;
    daxa_u32 cascade_level;
};

DAXA_DECL_TASK_USES_BEGIN(TerrainShadowmapTaskBase, DAXA_UNIFORM_BUFFER_SLOT0)
DAXA_TASK_USE_BUFFER(_vertices, daxa_BufferPtr(TerrainVertex), VERTEX_SHADER_READ)
DAXA_TASK_USE_BUFFER(_indices, daxa_BufferPtr(TerrainIndex), VERTEX_SHADER_READ)
DAXA_TASK_USE_BUFFER(_globals, daxa_BufferPtr(Globals), GRAPHICS_SHADER_READ)
DAXA_TASK_USE_BUFFER(_cascade_data, daxa_BufferPtr(ShadowmapCascadeData), GRAPHICS_SHADER_READ)
DAXA_TASK_USE_IMAGE(_shadowmap_cascades, REGULAR_2D, DEPTH_ATTACHMENT)
DAXA_TASK_USE_IMAGE(_height_map, REGULAR_2D, GRAPHICS_SHADER_SAMPLED)
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
        .depth_test = daxa::DepthTestInfo{
            .depth_attachment_format = daxa::Format::D32_SFLOAT,
            .enable_depth_write = true,
        },
        .raster = { 
            .primitive_topology = daxa::PrimitiveTopology::PATCH_LIST,
            .primitive_restart_enable = false,
            .polygon_mode = daxa::PolygonMode::FILL,
            .face_culling = daxa::FaceCullFlagBits::BACK_BIT,
            .depth_clamp_enable = true,
        },
        .tesselation = { .control_points = 4 },
        .push_constant_size = sizeof(DrawTerrainShadowmapPC),
        .name = "terrain shadowmap pipeline"
    };
}

struct TerrainShadowmapTask : TerrainShadowmapTaskBase
{
    Context *context = {};

    // TODO(msakmary) Make this a function - iteratively increase the dimensions of the smaller dimension
    // untill the image fits
    static constexpr std::array<daxa_u32vec2, 8> resolution_table{
        daxa_u32vec2{1u,1u}, daxa_u32vec2{2u,1u}, daxa_u32vec2{2u,2u}, daxa_u32vec2{2u,2u},
        daxa_u32vec2{3u,2u}, daxa_u32vec2{3u,2u}, daxa_u32vec2{3u,3u}, daxa_u32vec2{3u,3u},
    };

    void callback(daxa::TaskInterface ti)
    {
        DBG_ASSERT_TRUE_M(NUM_CASCADES <= 8, "[TerrainShadowmapTask::callback()]More than 8 cascades are not supported");
        auto & cmd_list = ti.get_recorder();

        cmd_list.set_uniform_buffer(ti.uses.get_uniform_buffer_info());
        auto dimensions = context->device.info_image(uses._shadowmap_cascades.image()).value().size;

        const auto resolution_multiplier = resolution_table[NUM_CASCADES - 1];
        auto render_cmd_list = std::move(cmd_list).begin_renderpass({
            .depth_attachment = 
            {{
                .image_view = {uses._shadowmap_cascades.view()},
                .layout = daxa::ImageLayout::ATTACHMENT_OPTIMAL,
                .load_op = daxa::AttachmentLoadOp::CLEAR,
                .store_op = daxa::AttachmentStoreOp::STORE,
                .clear_value = daxa::ClearValue{daxa::DepthValue{1.0f, 0}},
            }},
            .render_area = {
                .x = 0,
                .y = 0,
                .width = SHADOWMAP_RESOLUTION * resolution_multiplier.x,
                .height = SHADOWMAP_RESOLUTION * resolution_multiplier.y
            },
        });

        render_cmd_list.set_pipeline(*(context->pipelines.draw_terrain_shadowmap));
        render_cmd_list.set_index_buffer({
            .id = uses._indices.buffer(),
            .offset = 0u,
            .index_type = daxa::IndexType::uint32
        });

        for(daxa_u32 i = 0; i < NUM_CASCADES; i++ )
        {
            daxa_u32vec2 offset;
            offset.x = i % resolution_multiplier.x;
            offset.y = i / resolution_multiplier.x;
            render_cmd_list.set_viewport({
                .x = static_cast<daxa_f32>(offset.x * SHADOWMAP_RESOLUTION),
                .y = static_cast<daxa_f32>(offset.y * SHADOWMAP_RESOLUTION),
                .width = SHADOWMAP_RESOLUTION,
                .height = SHADOWMAP_RESOLUTION,
                .min_depth = 0.0f,
                .max_depth = 1.0f
            });
            render_cmd_list.push_constant(DrawTerrainShadowmapPC{ 
                .linear_sampler_id = context->linear_sampler,
                .cascade_level = i
            });
            render_cmd_list.draw_indexed({ .index_count = static_cast<daxa_u32>(context->terrain_index_size)});
        }

        cmd_list = std::move(render_cmd_list).end_renderpass();
    }
};
#endif //__cplusplus