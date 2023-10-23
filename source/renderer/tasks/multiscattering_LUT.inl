#pragma once

#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

#include "../shared/shared.inl"

struct MultiscatteringPC
{
    daxa_SamplerId sampler_id;
};

DAXA_DECL_TASK_USES_BEGIN(ComputeMultiscatteringTaskBase, DAXA_UNIFORM_BUFFER_SLOT0)
DAXA_TASK_USE_BUFFER(_globals, daxa_BufferPtr(Globals), COMPUTE_SHADER_READ)
DAXA_TASK_USE_IMAGE(_transmittance_LUT, REGULAR_2D, COMPUTE_SHADER_SAMPLED)
DAXA_TASK_USE_IMAGE(_multiscattering_LUT, REGULAR_2D, COMPUTE_SHADER_STORAGE_WRITE_ONLY)
DAXA_DECL_TASK_USES_END()

#if __cplusplus
#include "../context.hpp"

inline auto get_multiscattering_LUT_pipeline() -> daxa::ComputePipelineCompileInfo
{
    return {
        .shader_info = {
            .source = daxa::ShaderFile{"multiscattering.glsl"},
            .compile_options = {}
        },
        .push_constant_size = sizeof(MultiscatteringPC),
        .name = "compute multiscattering LUT pipeline"
    };
}

struct ComputeMultiscatteringTask : ComputeMultiscatteringTaskBase
{
    Context * context = {};
    void callback(daxa::TaskInterface ti)
    {
        auto cmd_list = ti.get_command_list();

        auto multiscattering_dimensions = context->device.info_image(uses._multiscattering_LUT.image()).value().size;
        cmd_list.set_uniform_buffer(ti.uses.get_uniform_buffer_info());
        cmd_list.set_pipeline(*(context->pipelines.multiscattering));
        cmd_list.push_constant(MultiscatteringPC{
            .sampler_id = context->linear_sampler,
        });
        cmd_list.dispatch(multiscattering_dimensions.x, multiscattering_dimensions.y);
    }
};
#endif