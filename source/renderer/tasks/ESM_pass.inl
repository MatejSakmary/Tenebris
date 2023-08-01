#pragma once

#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

#include "../shared/shared.inl"

struct ESMShadowPC
{
    u32vec2 offset;
    u32 cascade_index;
};

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
    static constexpr u32 threadsX = 8;
    static constexpr u32 threadsY = 4;

    Context * context = {};
    void callback(daxa::TaskInterface ti)
    {
        auto cmd_list = ti.get_command_list();

        cmd_list.set_uniform_buffer(ti.uses.get_uniform_buffer_info());
        cmd_list.set_pipeline(*(context->pipelines.first_esm_pass));

        const auto resolution_multiplier = TerrainShadowmapTask::resolution_table[NUM_CASCADES - 1];
        for(u32 i = 0; i < NUM_CASCADES; i++ )
        {
            u32vec2 offset;
            offset.x = i % resolution_multiplier.x;
            offset.y = i / resolution_multiplier.x;

            cmd_list.push_constant(ESMShadowPC{
                .offset = {
                    offset.x * SHADOWMAP_RESOLUTION,
                    offset.y * SHADOWMAP_RESOLUTION
                },
                .cascade_index = i
            });
            cmd_list.dispatch((SHADOWMAP_RESOLUTION + threadsX - 1) / threadsX, (SHADOWMAP_RESOLUTION + threadsY - 1) / threadsY);
        }
    }
};

struct ESMSecondPassTask : ESMSecondPassTaskBase
{
    static constexpr u32 threadsX = 8;
    static constexpr u32 threadsY = 4;

    Context * context = {};
    void callback(daxa::TaskInterface ti)
    {
        auto cmd_list = ti.get_command_list();

        cmd_list.set_uniform_buffer(ti.uses.get_uniform_buffer_info());
        cmd_list.set_pipeline(*(context->pipelines.second_esm_pass));

        for(u32 i = 0; i < NUM_CASCADES; i++ )
        {
            cmd_list.push_constant(ESMShadowPC{ .offset = {}, .cascade_index = i });
            cmd_list.dispatch((SHADOWMAP_RESOLUTION + threadsX - 1) / threadsX, (SHADOWMAP_RESOLUTION + threadsY - 1) / threadsY);
        }
    }
};
#endif //__cplusplus