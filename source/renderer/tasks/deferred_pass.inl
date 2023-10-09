#pragma once

#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

#include "../shared/shared.inl"

struct DeferredPassPC
{
    bool debug_active;
    daxa_SamplerId linear_sampler_id;
    daxa_SamplerId nearest_sampler_id;
    daxa_u32vec2 esm_resolution;
    daxa_u32vec2 offscreen_resolution;
};

DAXA_DECL_TASK_USES_BEGIN(DeferredPassTaskBase, DAXA_UNIFORM_BUFFER_SLOT0)
DAXA_TASK_USE_BUFFER(_globals, daxa_BufferPtr(Globals), FRAGMENT_SHADER_READ)
DAXA_TASK_USE_BUFFER(_cascade_data, daxa_BufferPtr(ShadowmapCascadeData), FRAGMENT_SHADER_READ)
DAXA_TASK_USE_BUFFER(_vsm_sun_projections, daxa_BufferPtr(VSMClipProjection), FRAGMENT_SHADER_READ)
DAXA_TASK_USE_IMAGE(_offscreen, REGULAR_2D, COLOR_ATTACHMENT)
DAXA_TASK_USE_IMAGE(_g_albedo, REGULAR_2D, FRAGMENT_SHADER_SAMPLED)
DAXA_TASK_USE_IMAGE(_g_normals, REGULAR_2D, FRAGMENT_SHADER_SAMPLED)
DAXA_TASK_USE_IMAGE(_transmittance, REGULAR_2D, FRAGMENT_SHADER_SAMPLED)
DAXA_TASK_USE_IMAGE(_esm, REGULAR_2D_ARRAY, FRAGMENT_SHADER_SAMPLED)
DAXA_TASK_USE_IMAGE(_skyview, REGULAR_2D, FRAGMENT_SHADER_SAMPLED)
DAXA_TASK_USE_IMAGE(_depth, REGULAR_2D, FRAGMENT_SHADER_SAMPLED)
DAXA_TASK_USE_IMAGE(_vsm_page_table, REGULAR_2D_ARRAY, FRAGMENT_SHADER_SAMPLED)
DAXA_TASK_USE_IMAGE(_vsm_physical_memory, REGULAR_2D, FRAGMENT_SHADER_SAMPLED)
DAXA_DECL_TASK_USES_END()

#if __cplusplus
#include "../context.hpp"

inline auto get_deferred_pass_pipeline() -> daxa::RasterPipelineCompileInfo {
    return {
        .vertex_shader_info = daxa::ShaderCompileInfo{ .source = daxa::ShaderFile{"screen_triangle.glsl"} },
        .fragment_shader_info = daxa::ShaderCompileInfo{ .source = daxa::ShaderFile{"deferred_pass.glsl"} },
        .color_attachments = {{.format = daxa::Format::R16G16B16A16_SFLOAT}},
        .depth_test = 
        { 
            .enable_depth_test = false,
            .enable_depth_write = false,
        },
        .raster = 
        { 
            .polygon_mode = daxa::PolygonMode::FILL,
            .face_culling = daxa::FaceCullFlagBits::BACK_BIT,
        },
        .push_constant_size = sizeof(DeferredPassPC),
        .name = "deferred pass pipeline"
    };
}
struct DeferredPassTask : DeferredPassTaskBase
{
    Context *context = {}; 
    void callback(daxa::TaskInterface ti)
    {
        auto cmd_list = ti.get_command_list();

        auto swapchain_resolution = context->swapchain.get_surface_extent();
        auto esm_resolution = context->device.info_image(uses._esm.image()).size;

        cmd_list.set_uniform_buffer(ti.uses.get_uniform_buffer_info());
        cmd_list.begin_renderpass({
            .color_attachments = 
            {{
                .image_view = {uses._offscreen.view()},
                .load_op = daxa::AttachmentLoadOp::LOAD
            }},
            .render_area = {.x = 0, .y = 0, .width = swapchain_resolution.x , .height = swapchain_resolution.y}
        });

        cmd_list.set_pipeline(*context->pipelines.deferred_pass);
        cmd_list.push_constant(DeferredPassPC{ 
            .debug_active = context->main_task_list.conditionals.at(MainConditionals::USE_DEBUG_CAMERA),
            .linear_sampler_id = context->linear_sampler,
            .nearest_sampler_id = context->nearest_sampler,
            .esm_resolution = {esm_resolution.x, esm_resolution.y},
            .offscreen_resolution = u32vec2{swapchain_resolution.x, swapchain_resolution.y},
        });
        cmd_list.draw({.vertex_count = 3});
        cmd_list.end_renderpass();
    }
};
#endif