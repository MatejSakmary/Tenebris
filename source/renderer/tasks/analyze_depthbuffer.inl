#pragma once

#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

#include "../shared/shared.inl"

struct AnalyzeDepthPC
{
    daxa_u32vec2 depth_dimensions;
    daxa_SamplerId linear_sampler;
    daxa_u32 prev_thread_count;
};

DAXA_DECL_TASK_USES_BEGIN(AnalyzeDepthbufferTaskBase, DAXA_UNIFORM_BUFFER_SLOT0)
DAXA_TASK_USE_BUFFER(_globals, daxa_BufferPtr(Globals), COMPUTE_SHADER_READ)
DAXA_TASK_USE_BUFFER(_depth_limits, daxa_BufferPtr(DepthLimits), COMPUTE_SHADER_READ_WRITE)
DAXA_TASK_USE_BUFFER(_vsm_allocation_count, daxa_BufferPtr(AllocationCount), COMPUTE_SHADER_READ_WRITE)
DAXA_TASK_USE_BUFFER(_vsm_allocation_buffer, daxa_BufferPtr(AllocationRequest), COMPUTE_SHADER_WRITE)
DAXA_TASK_USE_BUFFER(_vsm_allocate_indirect, daxa_BufferPtr(DispatchIndirectStruct), COMPUTE_SHADER_WRITE)
DAXA_TASK_USE_BUFFER(_vsm_clear_indirect, daxa_BufferPtr(DispatchIndirectStruct), COMPUTE_SHADER_WRITE)
DAXA_TASK_USE_BUFFER(_vsm_clear_dirty_bit_indirect, daxa_BufferPtr(DispatchIndirectStruct), COMPUTE_SHADER_WRITE)
DAXA_TASK_USE_BUFFER(_vsm_sun_projections, daxa_BufferPtr(VSMClipProjection), COMPUTE_SHADER_READ)
DAXA_TASK_USE_IMAGE(_depth, REGULAR_2D, COMPUTE_SHADER_SAMPLED)
DAXA_TASK_USE_IMAGE(_vsm_page_table, REGULAR_2D_ARRAY, COMPUTE_SHADER_STORAGE_READ_WRITE)
DAXA_TASK_USE_IMAGE(_vsm_meta_memory_table, REGULAR_2D, COMPUTE_SHADER_STORAGE_READ_WRITE)
DAXA_DECL_TASK_USES_END()

#if __cplusplus
#include "../context.hpp"
inline auto get_analyze_depthbuffer_pipeline(bool first_pass) -> daxa::ComputePipelineCompileInfo {
    daxa::ShaderCompileOptions options;
    if(first_pass) { options.defines = {{"FIRST_PASS", "1" }}; }

    return {
        .shader_info = { 
            .source = daxa::ShaderFile{"analyze_depthbuffer.glsl"},
            .compile_options = options
        }, 
        .push_constant_size = sizeof(AnalyzeDepthPC),
        .name = first_pass ? "analyze depth pipeline first pass" : "analyze depth pipeline subsequent pass"
    };
}

struct AnalyzeDepthbufferTask : AnalyzeDepthbufferTaskBase
{
    static constexpr daxa_u32vec2 workgroup_size = {32u, 32u};
    // Each thread will read a 2x2 block
    static constexpr daxa_u32vec2 thread_read_count = {2u, 2u};
    // How many pixels each workgroup will read on each axis
    static constexpr daxa_u32vec2 wg_total_reads_per_axis = daxa_u32vec2(
        workgroup_size.x * thread_read_count.x,
        workgroup_size.y * thread_read_count.y
    );

    Context * context = {};
    void callback(daxa::TaskInterface ti)
    {
        auto & cmd_list = ti.get_recorder();

        // Each thread will read a 2x2 block
        auto image_dimensions = context->device.info_image(uses._depth.image()).value().size;
        daxa_u32vec2 first_pass_dispatch_size = daxa_u32vec2{
            (image_dimensions.x + wg_total_reads_per_axis.x - 1) / wg_total_reads_per_axis.x,
            (image_dimensions.y + wg_total_reads_per_axis.y - 1) / wg_total_reads_per_axis.y
        };

        cmd_list.set_uniform_buffer(ti.uses.get_uniform_buffer_info());
        cmd_list.push_constant(AnalyzeDepthPC{ 
            .depth_dimensions = {image_dimensions.x, image_dimensions.y},
            .linear_sampler = context->linear_sampler
        });
        cmd_list.set_pipeline(*(context->pipelines.analyze_depthbuffer_first_pass));
        cmd_list.dispatch(first_pass_dispatch_size.x, first_pass_dispatch_size.y);

        daxa_u32 const threads_in_block = workgroup_size.x * workgroup_size.y;
        daxa_u32 const wg_total_reads = threads_in_block * thread_read_count.x * thread_read_count.y;
        daxa_u32 prev_pass_num_threads = first_pass_dispatch_size.x * first_pass_dispatch_size.y;
        daxa_u32 second_pass_dispatch_size = 0;
        while(second_pass_dispatch_size != 1)
        {
            second_pass_dispatch_size = (prev_pass_num_threads + wg_total_reads - 1) / wg_total_reads;
            cmd_list.pipeline_barrier({
                .src_access = daxa::AccessConsts::COMPUTE_SHADER_WRITE,
                .dst_access = daxa::AccessConsts::COMPUTE_SHADER_READ_WRITE,
            });
            cmd_list.push_constant(AnalyzeDepthPC{ .prev_thread_count = prev_pass_num_threads });
            cmd_list.set_uniform_buffer(ti.uses.get_uniform_buffer_info());
            cmd_list.set_pipeline(*(context->pipelines.analyze_depthbuffer_subsequent_pass));
            cmd_list.dispatch(second_pass_dispatch_size);

            prev_pass_num_threads = second_pass_dispatch_size;
        }
    }
};
#endif //__cplusplus