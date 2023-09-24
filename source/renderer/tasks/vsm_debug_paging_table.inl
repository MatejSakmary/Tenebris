#pragma once

#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

#include "../shared/shared.inl"

DAXA_DECL_TASK_USES_BEGIN(VSMDebugPageTableTaskBase, DAXA_UNIFORM_BUFFER_SLOT0)
DAXA_TASK_USE_IMAGE(_vsm_page_table, REGULAR_2D, COMPUTE_SHADER_STORAGE_READ_ONLY)
DAXA_TASK_USE_IMAGE(_vsm_debug_paging_table, REGULAR_2D, COMPUTE_SHADER_STORAGE_WRITE_ONLY)
DAXA_DECL_TASK_USES_END()

#if __cplusplus
#include "../context.hpp"
inline auto get_vsm_debug_paging_table_pipeline() -> daxa::ComputePipelineCompileInfo {
    return {
        .shader_info = { .source = daxa::ShaderFile{"vsm_debug_paging_table.glsl"}, },
        .name = "vsm_debug_paging_table"
    };
}

struct VSMDebugPageTableTask : VSMDebugPageTableTaskBase
{
    Context * context = {};
    void callback(daxa::TaskInterface ti)
    {
        auto cmd_list = ti.get_command_list();

        cmd_list.set_uniform_buffer(ti.uses.get_uniform_buffer_info());
        cmd_list.set_pipeline(*(context->pipelines.vsm_debug_paging_table));
        cmd_list.dispatch(VSM_PAGE_TABLE_RESOLUTION, VSM_PAGE_TABLE_RESOLUTION);
    }
};

#endif //__cplusplus