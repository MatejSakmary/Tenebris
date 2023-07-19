#pragma once

#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

#include "../shared/shared.inl"

struct AnalyseDepthPC
{
    u32vec2 depth_dimensions;
    u32 subsequent_thread_count;
};

DAXA_DECL_TASK_USES_BEGIN(AnalyseDepthbufferTaskBase, DAXA_UNIFORM_BUFFER_SLOT0)
DAXA_TASK_USE_BUFFER(_globals, daxa_BufferPtr(Globals), COMPUTE_SHADER_READ)
DAXA_TASK_USE_BUFFER(_depth_limits, daxa_BufferPtr(DepthLimits), COMPUTE_SHADER_READ_WRITE)
DAXA_TASK_USE_IMAGE(_depth, REGULAR_2D, COMPUTE_SHADER_SAMPLED)
DAXA_DECL_TASK_USES_END()

#if __cplusplus
#include "../context.hpp"
inline auto get_analyse_depthbuffer_pipeline(bool first_pass) -> daxa::ComputePipelineCompileInfo {
    daxa::ShaderCompileOptions options;
    if(first_pass) { options.defines = {{"FIRST_PASS", "1" }}; }

    return {
        .shader_info = { 
            .source = daxa::ShaderFile{"analyse_depthbuffer.glsl"},
            .compile_options = options
        }, 
        .push_constant_size = sizeof(AnalyseDepthPC),
        .name = first_pass ? "analyze depth pipeline first pass" : "analyze depth pipeline subsequent pass"
    };
}

struct AnalyseDepthbufferTask : AnalyseDepthbufferTaskBase
{
    static constexpr u32 threadsX = 32;
    static constexpr u32 threadsY = 32;

    Context * context = {};
    void callback(daxa::TaskInterface ti)
    {
        auto cmd_list = ti.get_command_list();

        auto image_dimensions = context->device.info_image(uses._depth.image()).size;
        u32vec2 first_pass_dispatch_size = u32vec2{
            (image_dimensions.x + threadsX - 1) / threadsX,
            (image_dimensions.y + threadsY - 1) / threadsY
        };

        cmd_list.set_uniform_buffer(ti.uses.get_uniform_buffer_info());
        cmd_list.push_constant(AnalyseDepthPC{ .depth_dimensions = {image_dimensions.x, image_dimensions.y} });
        cmd_list.set_pipeline(*(context->pipelines.analyze_depthbuffer_first_pass));
        cmd_list.dispatch(first_pass_dispatch_size.x, first_pass_dispatch_size.y);

        u32 prev_pass_num_threads = first_pass_dispatch_size.x * first_pass_dispatch_size.y;
        u32 threads_in_block = threadsX * threadsY;
        u32 second_pass_dispatch_size = {};
        while(second_pass_dispatch_size != 1)
        {
            second_pass_dispatch_size = (prev_pass_num_threads + threads_in_block - 1) / threads_in_block;
            cmd_list.pipeline_barrier({
                .src_access = daxa::AccessConsts::COMPUTE_SHADER_WRITE,
                .dst_access = daxa::AccessConsts::COMPUTE_SHADER_READ_WRITE,
            });
            cmd_list.push_constant(AnalyseDepthPC{ .subsequent_thread_count = prev_pass_num_threads });
            cmd_list.set_uniform_buffer(ti.uses.get_uniform_buffer_info());
            cmd_list.set_pipeline(*(context->pipelines.analyze_depthbuffer_subsequent_pass));
            cmd_list.dispatch(second_pass_dispatch_size);

            prev_pass_num_threads = second_pass_dispatch_size;
        }
    }
};
#endif //__cplusplus