#pragma once

#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

#include "../shared/shared.inl"

DAXA_DECL_TASK_USES_BEGIN(VSMClearDirtyBitTaskBase, DAXA_UNIFORM_BUFFER_SLOT0)
DAXA_TASK_USE_BUFFER(_vsm_allocation_count, daxa_BufferPtr(AllocationCount), COMPUTE_SHADER_READ)
DAXA_TASK_USE_BUFFER(_vsm_allocation_buffer, daxa_BufferPtr(AllocationRequest), COMPUTE_SHADER_READ)
DAXA_TASK_USE_BUFFER(_vsm_clear_dirty_bit_indirect, daxa_BufferPtr(DispatchIndirectStruct), COMPUTE_SHADER_READ)
DAXA_TASK_USE_IMAGE(_vsm_page_table, REGULAR_2D_ARRAY, COMPUTE_SHADER_STORAGE_READ_WRITE)
DAXA_DECL_TASK_USES_END()

#if __cplusplus
inline auto get_vsm_clear_dirty_bit_pipeline() -> daxa::ComputePipelineCompileInfo{
    return {
        .shader_info = { .source = daxa::ShaderFile{"vsm_clear_dirty_bit.glsl"}, },
        .name = "vsm_clear_dirty_bit"
    };
}

struct VSMClearDirtyBitTask : VSMClearDirtyBitTaskBase
{
    Context * context = {};

    void callback(daxa::TaskInterface ti)
    {
        auto cmd_list = ti.get_command_list();
        cmd_list.set_uniform_buffer(ti.uses.get_uniform_buffer_info());
        cmd_list.set_pipeline(*(context->pipelines.vsm_clear_dirty_bit));
        cmd_list.dispatch_indirect({
            .indirect_buffer = uses._vsm_clear_dirty_bit_indirect.buffer(),
            .offset = 0u
        });
    }
};
#endif //cplusplus