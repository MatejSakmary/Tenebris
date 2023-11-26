#pragma once

#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

#include "../shared/shared.inl"

DAXA_DECL_TASK_USES_BEGIN(VSMFreeWrappedPagesTaskBase, DAXA_UNIFORM_BUFFER_SLOT0)
DAXA_TASK_USE_BUFFER(_free_wrapped_pages_info, daxa_BufferPtr(FreeWrappedPagesInfo), COMPUTE_SHADER_READ)
DAXA_TASK_USE_BUFFER(_vsm_sun_projections, daxa_BufferPtr(VSMClipProjection), COMPUTE_SHADER_READ)
DAXA_TASK_USE_IMAGE(_vsm_page_table, REGULAR_2D_ARRAY, COMPUTE_SHADER_STORAGE_READ_WRITE)
DAXA_TASK_USE_IMAGE(_vsm_meta_memory_table, REGULAR_2D, COMPUTE_SHADER_STORAGE_READ_WRITE)
DAXA_DECL_TASK_USES_END()

#if __cplusplus
#include "../context.hpp"

inline auto get_vsm_free_wrapped_pages_pipeline() -> daxa::ComputePipelineCompileInfo {
    return {
        .shader_info = { .source = daxa::ShaderFile{"vsm_free_wrapped_pages.glsl"}, }, 
        .name = "vsm free wrapped pages"
    };
}

struct VSMFreeWrappedPagesTask : VSMFreeWrappedPagesTaskBase
{
    Context * context = {};

    void callback(daxa::TaskInterface ti)
    {
        auto & cmd_list = ti.get_recorder();
        cmd_list.set_uniform_buffer(ti.uses.get_uniform_buffer_info());
        cmd_list.set_pipeline(*(context->pipelines.vsm_free_wrapped_pages));
        cmd_list.dispatch(1, VSM_PAGE_TABLE_RESOLUTION, VSM_CLIP_LEVELS);
    }
};
#endif //__cplusplus