#pragma once

#include <daxa/daxa.inl>
#include <daxa/utils/task_list.inl>

#include "../shared/shared.inl"

struct TransmittancePC
{
    daxa_u32vec2 dimensions;
};

DAXA_INL_TASK_USE_BEGIN(ComputeTransmittanceTaskBase, DAXA_CBUFFER_SLOT0)
DAXA_INL_TASK_USE_BUFFER(_atmosphere_parameters, daxa_BufferPtr(AtmosphereParameters), COMPUTE_SHADER_READ)
DAXA_INL_TASK_USE_IMAGE(_transmittance_LUT, daxa_RWImage2Df32, COMPUTE_SHADER_WRITE)
DAXA_INL_TASK_USE_END()

#if __cplusplus
#include "../context.hpp"

inline auto get_transmittance_LUT_pipeline() -> daxa::ComputePipelineCompileInfo
{
    return {
        .shader_info = { .source = daxa::ShaderFile{"transmittance.glsl"}, },
        .push_constant_size = sizeof(TransmittancePC),
        .name = "compute transmittance LUT pipeline"
    };
}

struct ComputeTransmittanceTask : ComputeTransmittanceTaskBase
{
    Context *context = {};
    void callback(daxa::TaskInterface ti)
    {
        auto cmd_list = ti.get_command_list();

        auto image_dimensions = context->device.info_image(uses._transmittance_LUT.image()).size;
        cmd_list.set_constant_buffer(ti.uses.constant_buffer_set_info());
        cmd_list.set_pipeline(*(context->pipelines.transmittance));
        cmd_list.push_constant(TransmittancePC{
            .dimensions = {image_dimensions.x, image_dimensions.y},
        });
        cmd_list.dispatch(((image_dimensions.x + 7)/8), ((image_dimensions.y + 3)/4));
    }
};
#endif