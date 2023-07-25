#pragma once

#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

#include "../shared/shared.inl"

struct ESMShadowPC
{
    u32vec2 offset;
    u32 cascade_index;
};

DAXA_DECL_TASK_USES_BEGIN(ESMPassTaskBase, DAXA_UNIFORM_BUFFER_SLOT0)
DAXA_TASK_USE_IMAGE(_shadowmap, REGULAR_2D, COMPUTE_SHADER_SAMPLED)
DAXA_TASK_USE_IMAGE(_esm_map, REGULAR_2D_ARRAY, COMPUTE_SHADER_STORAGE_WRITE_ONLY)
DAXA_DECL_TASK_USES_END()

#if __cplusplus
#include "../context.hpp"
inline auto get_esm_pass_pipeline() -> daxa::ComputePipelineCompileInfo {
    return {
        .shader_info = { .source = daxa::ShaderFile{"ESM_pass.glsl"}, }, 
        .push_constant_size = sizeof(ESMShadowPC),
        .name = "esm pass pipeline"
    };
}

struct ESMPassTask : ESMPassTaskBase
{
    static constexpr u32 threadsX = 8;
    static constexpr u32 threadsY = 4;

    Context * context = {};
    void callback(daxa::TaskInterface ti)
    {
        auto cmd_list = ti.get_command_list();

        cmd_list.set_uniform_buffer(ti.uses.get_uniform_buffer_info());
        cmd_list.set_pipeline(*(context->pipelines.esm_pass));

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
#endif //__cplusplus