#pragma once

#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

#include "../shared/shared.inl"

DAXA_DECL_TASK_USES_BEGIN(VSMDebugVirtualPageTableTaskBase, DAXA_UNIFORM_BUFFER_SLOT0)
DAXA_TASK_USE_BUFFER(_globals, daxa_BufferPtr(Globals), COMPUTE_SHADER_READ)
DAXA_TASK_USE_IMAGE(_vsm_page_table, REGULAR_2D_ARRAY, COMPUTE_SHADER_STORAGE_READ_ONLY)
DAXA_TASK_USE_IMAGE(_vsm_debug_page_table, REGULAR_2D, COMPUTE_SHADER_STORAGE_WRITE_ONLY)
DAXA_DECL_TASK_USES_END()

DAXA_DECL_TASK_USES_BEGIN(VSMDebugMetaTableTaskBase, DAXA_UNIFORM_BUFFER_SLOT0)
DAXA_TASK_USE_IMAGE(_vsm_page_table_mem_pass, REGULAR_2D_ARRAY, COMPUTE_SHADER_STORAGE_READ_WRITE)
DAXA_TASK_USE_IMAGE(_vsm_meta_memory_table, REGULAR_2D, COMPUTE_SHADER_STORAGE_READ_WRITE)
DAXA_TASK_USE_IMAGE(_vsm_debug_meta_memory_table, REGULAR_2D, COMPUTE_SHADER_STORAGE_WRITE_ONLY)
DAXA_DECL_TASK_USES_END()

#if __cplusplus
#include "../context.hpp"
inline auto get_vsm_debug_meta_memory_table_pipeline() -> daxa::ComputePipelineCompileInfo {
    return {
        .shader_info = { 
            .source = daxa::ShaderFile{"vsm_debug_pass.glsl"},
            .compile_options = daxa::ShaderCompileOptions{ .defines = {{"VSM_DEBUG_META_MEMORY_TABLE", "1"}}},
        },
        .name = "vsm debug meta memory table"
    };
}

inline auto get_vsm_debug_page_table_pipeline() -> daxa::ComputePipelineCompileInfo {
    return {
        .shader_info = {
            .source = daxa::ShaderFile{"vsm_debug_pass.glsl"},
            .compile_options = daxa::ShaderCompileOptions{ .defines = {{"VSM_DEBUG_PAGE_TABLE", "1"}}},
        },
        .name = "vsm debug page table"
    };
}

struct VSMDebugVirtualPageTableTask : VSMDebugVirtualPageTableTaskBase
{
    Context * context = {};
    void callback(daxa::TaskInterface ti)
    {
        auto cmd_list = ti.get_command_list();

        cmd_list.set_uniform_buffer(ti.uses.get_uniform_buffer_info());
        cmd_list.set_pipeline(*(context->pipelines.vsm_debug_page_table));
        cmd_list.dispatch(VSM_PAGE_TABLE_RESOLUTION, VSM_PAGE_TABLE_RESOLUTION);
        cmd_list.pipeline_barrier({
            .src_access = daxa::Access{
                .stages = daxa::PipelineStageFlagBits::COMPUTE_SHADER,
                .type = daxa::AccessTypeFlagBits::READ_WRITE
            }
        });
    }
};

struct VSMDebugMetaTableTask : VSMDebugMetaTableTaskBase
{
    Context * context = {};
    void callback(daxa::TaskInterface ti)
    {
        auto cmd_list = ti.get_command_list();

        cmd_list.set_uniform_buffer(ti.uses.get_uniform_buffer_info());
        cmd_list.set_pipeline(*(context->pipelines.vsm_debug_meta_memory_table));
        cmd_list.dispatch(VSM_DEBUG_META_MEMORY_RESOLUTION, VSM_DEBUG_META_MEMORY_RESOLUTION);
    }
};

#endif //__cplusplus