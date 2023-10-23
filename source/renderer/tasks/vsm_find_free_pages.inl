#pragma once

#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

#include "../shared/shared.inl"

DAXA_DECL_TASK_USES_BEGIN(VSMFindFreePagesTaskBase, DAXA_UNIFORM_BUFFER_SLOT0)
DAXA_TASK_USE_BUFFER(_vsm_allocation_buffer, daxa_BufferPtr(AllocationRequest), COMPUTE_SHADER_READ)
DAXA_TASK_USE_BUFFER(_vsm_allocate_indirect, daxa_BufferPtr(DispatchIndirectStruct), COMPUTE_SHADER_READ)
DAXA_TASK_USE_BUFFER(_vsm_free_pages_buffer, daxa_BufferPtr(PageCoordBuffer), COMPUTE_SHADER_WRITE)
DAXA_TASK_USE_BUFFER(_vsm_not_visited_pages_buffer, daxa_BufferPtr(PageCoordBuffer), COMPUTE_SHADER_WRITE)
DAXA_TASK_USE_BUFFER(_vsm_find_free_pages_header, daxa_BufferPtr(FindFreePagesHeader), COMPUTE_SHADER_READ_WRITE)
DAXA_TASK_USE_IMAGE(_vsm_page_table, REGULAR_2D_ARRAY, COMPUTE_SHADER_STORAGE_READ_WRITE)
DAXA_TASK_USE_IMAGE(_vsm_meta_memory_table, REGULAR_2D, COMPUTE_SHADER_STORAGE_READ_WRITE)
DAXA_DECL_TASK_USES_END()

#if __cplusplus
#include "../context.hpp"

inline auto get_vsm_find_free_pages_pipeline() -> daxa::ComputePipelineCompileInfo {
    return {
        .shader_info = { .source = daxa::ShaderFile{"vsm_find_free_pages.glsl"}, }, 
        .name = "vsm find free pages"
    };
}

struct VSMFindFreePagesTask : VSMFindFreePagesTaskBase
{
    Context * context = {};
    static constexpr daxa_i32 meta_memory_pix_count = VSM_META_MEMORY_RESOLUTION * VSM_META_MEMORY_RESOLUTION;
    static constexpr daxa_i32 dispatch_x_size = 
        (meta_memory_pix_count + VSM_FIND_FREE_PAGES_LOCAL_SIZE_X - 1) / VSM_FIND_FREE_PAGES_LOCAL_SIZE_X;

    void callback(daxa::TaskInterface ti)
    {
        auto cmd_list = ti.get_command_list();
        cmd_list.set_uniform_buffer(ti.uses.get_uniform_buffer_info());
        cmd_list.set_pipeline(*(context->pipelines.vsm_find_free_pages));
        cmd_list.dispatch(dispatch_x_size, 1, 1);
    }
};
#endif //__cplusplus