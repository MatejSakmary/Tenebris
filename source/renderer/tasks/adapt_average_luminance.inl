#pragma once

#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

#include "../shared/shared.inl"

struct AdaptAverageLuminancePC
{
    daxa_u32vec2 resolution;
};

DAXA_DECL_TASK_USES_BEGIN(AdaptAverageLuminanceTaskBase, DAXA_UNIFORM_BUFFER_SLOT0)
DAXA_TASK_USE_BUFFER(_globals, daxa_BufferPtr(Globals), COMPUTE_SHADER_READ)
DAXA_TASK_USE_BUFFER(_histogram, daxa_BufferPtr(Histogram), COMPUTE_SHADER_READ)
DAXA_TASK_USE_BUFFER(_average_luminance, daxa_BufferPtr(AverageLuminance), COMPUTE_SHADER_READ_WRITE)
DAXA_DECL_TASK_USES_END()

#if __cplusplus
#include "../context.hpp"
inline auto get_adapt_average_luminance_pipeline() -> daxa::ComputePipelineCompileInfo {
    return {
        .shader_info = {.source = daxa::ShaderFile{"adapt_average_luminance.glsl"}},
        .push_constant_size = sizeof(AdaptAverageLuminancePC),
        .name = "adapt average luminance"
    };
}

struct AdaptAverageLuminanceTask : AdaptAverageLuminanceTaskBase
{
    static constexpr daxa_u32vec2 workgroup_size = {16u, 16u};
    Context * context;

    void callback(daxa::TaskInterface ti)
    {
        DBG_ASSERT_TRUE_M(HISTOGRAM_BIN_COUNT == 256, 
            "[Renderer::AdaptAverageLuminanceTask()] Wrong workgroup size for bin count != 256");

        auto & cmd_list = ti.get_recorder();

        auto resolution = context->swapchain.get_surface_extent();
        cmd_list.set_uniform_buffer(ti.uses.get_uniform_buffer_info());
        cmd_list.set_pipeline(*(context->pipelines.adapt_average_luminance));
        cmd_list.push_constant(LuminanceHistogramPC{.resolution = {resolution.x, resolution.y}});
        cmd_list.dispatch(1);
    }
};
#endif //_cplusplus