#pragma once

#include <daxa/daxa.inl>
#include <daxa/utils/task_list.inl>

#include "../shared/shared.inl"

struct MultiscatteringPC
{
    daxa_u32vec2 multiscattering_dimensions;
    daxa_SamplerId sampler_id;
};

DAXA_INL_TASK_USE_BEGIN(ComputeMultiscatteringTaskBase, DAXA_CBUFFER_SLOT0)
DAXA_INL_TASK_USE_BUFFER(_atmosphere_parameters, daxa_BufferPtr(AtmosphereParameters), COMPUTE_SHADER_READ)
DAXA_INL_TASK_USE_IMAGE(_transmittance_LUT, daxa_Image2Df32, COMPUTE_SHADER_READ)
DAXA_INL_TASK_USE_IMAGE(_multiscattering_LUT, daxa_RWImage2Df32, COMPUTE_SHADER_WRITE)
DAXA_INL_TASK_USE_END()

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

        auto multiscattering_dimensions = context->device.info_image(uses._multiscattering_LUT.image()).size;
        cmd_list.set_constant_buffer(ti.uses.constant_buffer_set_info());
        cmd_list.set_pipeline(*(context->pipelines.multiscattering));
        cmd_list.push_constant(MultiscatteringPC{
            .multiscattering_dimensions = {multiscattering_dimensions.x, multiscattering_dimensions.y},
            .sampler_id = context->linear_sampler,
        });
        cmd_list.dispatch(multiscattering_dimensions.x, multiscattering_dimensions.y);
    }
};
#endif