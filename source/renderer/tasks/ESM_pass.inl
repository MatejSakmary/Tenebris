#pragma once

#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

#include "../shared/shared.inl"

struct ESMShadowPC
{
    daxa_u32vec2 offset;
    daxa_u32 cascade_index;
};

#define WORKGROUP_SIZE 64

DAXA_DECL_TASK_USES_BEGIN(ESMFirstPassTaskBase, DAXA_UNIFORM_BUFFER_SLOT0)
DAXA_TASK_USE_IMAGE(_shadowmap, REGULAR_2D, COMPUTE_SHADER_SAMPLED)
DAXA_TASK_USE_IMAGE(_esm_map, REGULAR_2D_ARRAY, COMPUTE_SHADER_STORAGE_WRITE_ONLY)
DAXA_DECL_TASK_USES_END()

DAXA_DECL_TASK_USES_BEGIN(ESMSecondPassTaskBase, DAXA_UNIFORM_BUFFER_SLOT0)
DAXA_TASK_USE_IMAGE(_esm_tmp_map, REGULAR_2D_ARRAY, COMPUTE_SHADER_STORAGE_READ_ONLY)
DAXA_TASK_USE_IMAGE(_esm_target_map, REGULAR_2D_ARRAY, COMPUTE_SHADER_STORAGE_WRITE_ONLY)
DAXA_DECL_TASK_USES_END()

#if __cplusplus
#include "../context.hpp"
inline auto get_esm_pass_pipeline(bool first_pass) -> daxa::ComputePipelineCompileInfo {
    daxa::ShaderCompileOptions options;
    if(first_pass) { options.defines = {{"FIRST_PASS", "1" }}; }
    else           { options.defines = {{"SECOND_PASS", "1" }};}
    return {
        .shader_info = { 
            .source = daxa::ShaderFile{"ESM_pass.glsl"},
            .compile_options = options
        }, 
        .push_constant_size = sizeof(ESMShadowPC),
        .name = first_pass ? "esm first pass pipeline" : "esm second pass pipeline"
    };
}

struct ESMFirstPassTask : ESMFirstPassTaskBase
{
    Context * context = {};
    void callback(daxa::TaskInterface ti)
    {
        DBG_ASSERT_TRUE_M(SHADOWMAP_RESOLUTION % WORKGROUP_SIZE == 0,
            "[Renderer::ESMSecondPassTask()] SHADOWMAP_RESOLUTION must be a multiple of WORKGROUP_SIZE");
        auto & cmd_list = ti.get_recorder();

        cmd_list.set_uniform_buffer(ti.uses.get_uniform_buffer_info());
        cmd_list.set_pipeline(*(context->pipelines.first_esm_pass));

        const auto resolution_multiplier = TerrainShadowmapTask::resolution_table[NUM_CASCADES - 1];
        for(daxa_u32 i = 0; i < NUM_CASCADES; i++ )
        {
            daxa_u32vec2 offset;
            offset.x = i % resolution_multiplier.x;
            offset.y = i / resolution_multiplier.x;

            cmd_list.push_constant(ESMShadowPC{
                .offset = {
                    offset.x * SHADOWMAP_RESOLUTION,
                    offset.y * SHADOWMAP_RESOLUTION
                },
                .cascade_index = i
            });
            cmd_list.dispatch(SHADOWMAP_RESOLUTION / WORKGROUP_SIZE, SHADOWMAP_RESOLUTION);
        }
    }
};

struct ESMSecondPassTask : ESMSecondPassTaskBase
{
    Context * context = {};
    void callback(daxa::TaskInterface ti)
    {
        DBG_ASSERT_TRUE_M(SHADOWMAP_RESOLUTION % WORKGROUP_SIZE == 0,
            "[Renderer::ESMSecondPassTask()] SHADOWMAP_RESOLUTION must be a multiple of WORKGROUP_SIZE");
        auto & cmd_list = ti.get_recorder();

        cmd_list.set_uniform_buffer(ti.uses.get_uniform_buffer_info());
        cmd_list.set_pipeline(*(context->pipelines.second_esm_pass));

        for(daxa_u32 i = 0; i < NUM_CASCADES; i++ )
        {
            cmd_list.push_constant(ESMShadowPC{ .offset = {}, .cascade_index = i });
            cmd_list.dispatch(SHADOWMAP_RESOLUTION, SHADOWMAP_RESOLUTION / WORKGROUP_SIZE);
        }
    }
};
#endif //__cplusplus