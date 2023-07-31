#pragma once

#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

#include "../shared/shared.inl"

struct LuminanceHistogramPC
{
    u32vec2 resolution;
};

DAXA_DECL_TASK_USES_BEGIN(LuminanceHistogramTaskBase, DAXA_UNIFORM_BUFFER_SLOT0)
DAXA_TASK_USE_BUFFER(_globals, daxa_BufferPtr(Globals), COMPUTE_SHADER_READ)
DAXA_TASK_USE_BUFFER(_histogram, daxa_BufferPtr(Histogram), COMPUTE_SHADER_WRITE)
DAXA_TASK_USE_IMAGE(_offscreen, REGULAR_2D, COMPUTE_SHADER_STORAGE_READ_ONLY)
DAXA_DECL_TASK_USES_END()

#if __cplusplus
#include "../context.hpp"
inline auto get_luminance_histogram_pipeline() -> daxa::ComputePipelineCompileInfo {
    return {
        .shader_info = { .source = daxa::ShaderFile{"luminance_histogram.glsl"}},
        .push_constant_size = sizeof(LuminanceHistogramPC),
        .name = "construct luminance histogram"
    };
}

struct LuminanceHistogramTask : LuminanceHistogramTaskBase
{
    static constexpr u32vec2 workgroup_size = {32u, 32u};
    Context * context = {};
    void callback(daxa::TaskInterface ti)
    {
        auto cmd_list = ti.get_command_list();

        auto resolution = context->device.info_image(uses._offscreen.image()).size;
        u32vec2 dispatch_size = u32vec2{
            (resolution.x + workgroup_size.x - 1) / workgroup_size.x,
            (resolution.y + workgroup_size.y - 1) / workgroup_size.y
        };

        cmd_list.set_uniform_buffer(ti.uses.get_uniform_buffer_info());
        cmd_list.set_pipeline(*(context->pipelines.luminance_histogram));
        cmd_list.push_constant(LuminanceHistogramPC{.resolution = {resolution.x, resolution.y}});
        cmd_list.dispatch(dispatch_size.x, dispatch_size.y);
    }
};
#endif //__cplusplus