#pragma once

#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

#include "../shared/shared.inl"

struct DrawTerrainPC
{
    daxa_SamplerId linear_sampler_id;
    daxa_SamplerId nearest_sampler_id;
    daxa_u32 use_secondary_camera;
};

DAXA_DECL_TASK_USES_BEGIN(DrawTerrainTaskBase, DAXA_UNIFORM_BUFFER_SLOT0)
DAXA_TASK_USE_BUFFER(_vertices, daxa_BufferPtr(TerrainVertex), VERTEX_SHADER_READ)
DAXA_TASK_USE_BUFFER(_indices, daxa_BufferPtr(TerrainIndex), VERTEX_SHADER_READ)
DAXA_TASK_USE_BUFFER(_globals, daxa_BufferPtr(Globals), SHADER_READ)
DAXA_TASK_USE_IMAGE(_g_albedo, REGULAR_2D, COLOR_ATTACHMENT)
DAXA_TASK_USE_IMAGE(_g_normals, REGULAR_2D, COLOR_ATTACHMENT)
DAXA_TASK_USE_IMAGE(_depth, REGULAR_2D, DEPTH_ATTACHMENT)
DAXA_TASK_USE_IMAGE(_height_map, REGULAR_2D, SHADER_SAMPLED)
DAXA_TASK_USE_IMAGE(_diffuse_map, REGULAR_2D, FRAGMENT_SHADER_SAMPLED)
DAXA_TASK_USE_IMAGE(_normal_map, REGULAR_2D, FRAGMENT_SHADER_SAMPLED)
DAXA_DECL_TASK_USES_END()

#if __cplusplus
#include "../context.hpp"
inline auto get_draw_terrain_pipeline(bool wireframe) -> daxa::RasterPipelineCompileInfo {
    return {
        .vertex_shader_info = daxa::ShaderCompileInfo{ .source = daxa::ShaderFile{"draw_terrain.glsl"}, },
        .tesselation_control_shader_info = daxa::ShaderCompileInfo{ .source = daxa::ShaderFile{"draw_terrain.glsl"}, },
        .tesselation_evaluation_shader_info = daxa::ShaderCompileInfo{ .source = daxa::ShaderFile{"draw_terrain.glsl"}, },
        .fragment_shader_info = daxa::ShaderCompileInfo{ .source = daxa::ShaderFile{"draw_terrain.glsl"}, },
        .color_attachments = {
            {.format = daxa::Format::R16G16B16A16_SFLOAT}, // g_albedo
            {.format = daxa::Format::R16G16B16A16_SFLOAT}, // g_normals
        },
        .depth_test = { 
            .depth_attachment_format = daxa::Format::D32_SFLOAT,
            .enable_depth_test = true,
            .enable_depth_write = true,
            .depth_test_compare_op = daxa::CompareOp::GREATER_OR_EQUAL,
        },
        .raster = { 
            .primitive_topology = daxa::PrimitiveTopology::PATCH_LIST,
            .primitive_restart_enable = false,
            .polygon_mode = wireframe ? daxa::PolygonMode::LINE : daxa::PolygonMode::FILL,
            .face_culling = daxa::FaceCullFlagBits::BACK_BIT,
        },
        .tesselation = { .control_points = 4 },
        .push_constant_size = sizeof(DrawTerrainPC),
        .name = "terrain pipeline"
    };
}

struct DrawTerrainTask : DrawTerrainTaskBase
{
    Context *context = {};
    bool * wireframe = {};
    bool use_secondary_camera = {};

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
                    .load_op = daxa::AttachmentLoadOp::CLEAR,
                    .clear_value = std::array<f32, 4>{0.0, 0.0, 0.0, 1.0}
                },
                {
                    .image_view = {uses._g_normals.view()},
                    .load_op = daxa::AttachmentLoadOp::CLEAR,
                    .clear_value = std::array<f32, 4>{0.0, 0.0, 0.0, 1.0}
                },
            },
            .depth_attachment = 
            {{
                .image_view = {uses._depth.view()},
                .layout = daxa::ImageLayout::ATTACHMENT_OPTIMAL,
                .load_op = daxa::AttachmentLoadOp::CLEAR,
                .store_op = daxa::AttachmentStoreOp::STORE,
                .clear_value = daxa::ClearValue{daxa::DepthValue{0.0f, 0}},
            }},
            .render_area = {.x = 0, .y = 0, .width = dimensions.x , .height = dimensions.y}
        });
        if(*wireframe) { cmd_list.set_pipeline(*(context->pipelines.draw_terrain_wireframe)); }
        else           { cmd_list.set_pipeline(*(context->pipelines.draw_terrain_solid));     }

        cmd_list.set_index_buffer(uses._indices.buffer(), 0, sizeof(u32));
        cmd_list.push_constant(DrawTerrainPC{ 
            .linear_sampler_id = context->linear_sampler,
            .nearest_sampler_id = context->nearest_sampler,
            .use_secondary_camera = use_secondary_camera ? 1u : 0u,
        });
        cmd_list.draw_indexed({.index_count = static_cast<u32>(context->terrain_index_size)});
        cmd_list.end_renderpass();
    }
};
#endif