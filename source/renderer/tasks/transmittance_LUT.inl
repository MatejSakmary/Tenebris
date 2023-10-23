#pragma once

#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

#include "../shared/shared.inl"

DAXA_DECL_TASK_USES_BEGIN(ComputeTransmittanceTaskBase, DAXA_UNIFORM_BUFFER_SLOT0)
DAXA_TASK_USE_BUFFER(_globals, daxa_BufferPtr(Globals), COMPUTE_SHADER_READ)
DAXA_TASK_USE_IMAGE(_transmittance_LUT, REGULAR_2D, COMPUTE_SHADER_STORAGE_WRITE_ONLY)
DAXA_DECL_TASK_USES_END()

#if __cplusplus
#include "../context.hpp"

inline auto get_transmittance_LUT_pipeline() -> daxa::ComputePipelineCompileInfo
{
    return {
        .shader_info = { .source = daxa::ShaderFile{"transmittance.glsl"}, },
        .name = "compute transmittance LUT pipeline"
    };
}

struct ComputeTransmittanceTask : ComputeTransmittanceTaskBase
{
    Context *context = {};
    void callback(daxa::TaskInterface ti)
    {
        auto cmd_list = ti.get_command_list();

        auto image_dimensions = context->device.info_image(uses._transmittance_LUT.image()).value().size;
        cmd_list.set_uniform_buffer(ti.uses.get_uniform_buffer_info());
        cmd_list.set_pipeline(*(context->pipelines.transmittance));
        cmd_list.dispatch(((image_dimensions.x + 7)/8), ((image_dimensions.y + 3)/4));
    }
};
#endif