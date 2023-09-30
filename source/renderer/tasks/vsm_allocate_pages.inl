#pragma once

#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

#include "../shared/shared.inl"

DAXA_DECL_TASK_USES_BEGIN(VSMAllocatePagesTaskBase, DAXA_UNIFORM_BUFFER_SLOT0)
DAXA_TASK_USE_BUFFER(_vsm_allocation_buffer, daxa_BufferPtr(AllocationRequest), COMPUTE_SHADER_READ)
DAXA_TASK_USE_BUFFER(_vsm_allocate_indirect, daxa_BufferPtr(DispatchIndirectStruct), COMPUTE_SHADER_READ)
DAXA_TASK_USE_BUFFER(_vsm_free_pages_buffer, daxa_BufferPtr(PageCoordBuffer), COMPUTE_SHADER_READ)
DAXA_TASK_USE_BUFFER(_vsm_not_visited_pages_buffer, daxa_BufferPtr(PageCoordBuffer), COMPUTE_SHADER_READ)
DAXA_TASK_USE_BUFFER(_vsm_find_free_pages_header, daxa_BufferPtr(FindFreePagesHeader), COMPUTE_SHADER_READ)
DAXA_TASK_USE_IMAGE(_vsm_page_table, REGULAR_2D, COMPUTE_SHADER_STORAGE_READ_WRITE)
DAXA_TASK_USE_IMAGE(_vsm_meta_memory_table, REGULAR_2D, COMPUTE_SHADER_STORAGE_READ_WRITE)
DAXA_DECL_TASK_USES_END()

#if __cplusplus
#include "../context.hpp"

inline auto get_vsm_allocate_pages_pipeline() -> daxa::ComputePipelineCompileInfo {
    return {
        .shader_info = { .source = daxa::ShaderFile{"vsm_allocate_pages.glsl"}, }, 
        .name = "vsm_allocate_pages"
    };
}

struct VSMAllocatePagesTask : VSMAllocatePagesTaskBase
{
    Context * context = {};

    void callback(daxa::TaskInterface ti)
    {
        auto cmd_list = ti.get_command_list();
        cmd_list.set_uniform_buffer(ti.uses.get_uniform_buffer_info());
        cmd_list.set_pipeline(*(context->pipelines.vsm_allocate_pages));
        cmd_list.dispatch_indirect({
            .indirect_buffer = uses._vsm_allocate_indirect.buffer(),
            .offset = 0u
        });
    }
};
#endif //__cplusplus