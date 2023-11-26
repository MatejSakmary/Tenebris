#pragma once

#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

#include "../shared/shared.inl"

DAXA_DECL_TASK_USES_BEGIN(PrepareShadowmapMatricesTaskBase, DAXA_UNIFORM_BUFFER_SLOT0)
DAXA_TASK_USE_BUFFER(_globals, daxa_BufferPtr(Globals), COMPUTE_SHADER_READ)
DAXA_TASK_USE_BUFFER(_depth_limits, daxa_BufferPtr(DepthLimits), COMPUTE_SHADER_READ)
DAXA_TASK_USE_BUFFER(_cascade_data, daxa_BufferPtr(ShadowmapCascadeData), COMPUTE_SHADER_WRITE)
DAXA_TASK_USE_BUFFER(_frustum_vertices, daxa_BufferPtr(FrustumVertex), COMPUTE_SHADER_WRITE)
DAXA_TASK_USE_BUFFER(_frustum_colors, daxa_BufferPtr(FrustumColor), COMPUTE_SHADER_WRITE)
DAXA_TASK_USE_BUFFER(_frustum_indirect, daxa_BufferPtr(DrawIndexedIndirectStruct), COMPUTE_SHADER_READ_WRITE)
DAXA_DECL_TASK_USES_END()

#if __cplusplus
#include "../context.hpp"
inline auto get_prepare_shadow_matrices_pipeline() -> daxa::ComputePipelineCompileInfo {
    return {
        .shader_info = { .source = daxa::ShaderFile{"prepare_shadow_matrices.glsl"}, },
        .name = "prepare shadowmap matrices pipeline"
    };
}

struct PrepareShadowmapMatricesTask : PrepareShadowmapMatricesTaskBase
{
    Context * context = {};
    void callback(daxa::TaskInterface ti)
    {
        auto & cmd_list = ti.get_recorder();

        cmd_list.set_uniform_buffer(ti.uses.get_uniform_buffer_info());
        cmd_list.set_pipeline(*(context->pipelines.prepare_shadow_matrices));
        cmd_list.dispatch(1, 1, 1);
    }
};
#endif // __cplusplus