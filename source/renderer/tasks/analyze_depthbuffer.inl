#pragma once

#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

#include "../shared/shared.inl"

struct AnalyzeDepthPC
{
    u32vec2 depth_dimensions;
    daxa_SamplerId linear_sampler;
    u32 prev_thread_count;
};

DAXA_DECL_TASK_USES_BEGIN(AnalyzeDepthbufferTaskBase, DAXA_UNIFORM_BUFFER_SLOT0)
DAXA_TASK_USE_BUFFER(_depth_limits, daxa_BufferPtr(DepthLimits), COMPUTE_SHADER_READ_WRITE)
DAXA_TASK_USE_IMAGE(_depth, REGULAR_2D, COMPUTE_SHADER_SAMPLED)
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
    static constexpr u32vec2 workgroup_size = {32u, 32u};
    // Each thread will read a 2x2 block
    static constexpr u32vec2 thread_read_count = {2u, 2u};
    // How many pixels each workgroup will read on each axis
    static constexpr u32vec2 wg_total_reads_per_axis = workgroup_size * thread_read_count;

    Context * context = {};
    void callback(daxa::TaskInterface ti)
    {
        auto cmd_list = ti.get_command_list();

        // Each thread will read a 2x2 block
        auto image_dimensions = context->device.info_image(uses._depth.image()).size;
        u32vec2 first_pass_dispatch_size = u32vec2{
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

        u32 const threads_in_block = workgroup_size.x * workgroup_size.y;
        u32 const wg_total_reads = threads_in_block * thread_read_count.x * thread_read_count.y;
        u32 prev_pass_num_threads = first_pass_dispatch_size.x * first_pass_dispatch_size.y;
        u32 second_pass_dispatch_size = 0;
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