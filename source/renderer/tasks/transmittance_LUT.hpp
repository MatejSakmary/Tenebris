#pragma once

#include <daxa/daxa.hpp>
#include <daxa/utils/task_list.hpp>

#include "../context.hpp"
#include "../shared/shared.inl"

inline auto get_transmittance_LUT_pipeline() -> daxa::ComputePipelineCompileInfo
{
    return {
        .shader_info = { .source = daxa::ShaderFile{"transmittance.glsl"}, },
        .push_constant_size = sizeof(TransmittancePC),
        .name = "compute transmittance LUT pipeline"
    };
}

inline void task_compute_transmittance_LUT(Context & context)
{
    context.main_task_list.task_list.add_task({
        .used_buffers = 
        {
            {
                context.main_task_list.task_buffers.t_atmosphere_parameters,
                daxa::TaskBufferAccess::COMPUTE_SHADER_READ_ONLY 
            }
        },
        .used_images = 
        {
            {
                context.main_task_list.task_images.at(Images::TRANSMITTANCE),
                daxa::TaskImageAccess::COMPUTE_SHADER_WRITE_ONLY,
                daxa::ImageMipArraySlice{}
            }
        },
        .task = [&](daxa::TaskRuntimeInterface const & runtime)
        {
            auto cmd_list = runtime.get_command_list();
            auto transmittance_image = runtime.get_images(context.main_task_list.task_images.at(Images::TRANSMITTANCE))[0];
            auto atmosphere_gpu_buffer = runtime.get_buffers(context.main_task_list.task_buffers.t_atmosphere_parameters)[0];

            auto image_dimensions = context.device.info_image(transmittance_image).size;
            cmd_list.set_pipeline(*context.pipelines.transmittance);
            cmd_list.push_constant(TransmittancePC{
                .transmittance_image = {transmittance_image.default_view()},
                .dimensions = {image_dimensions.x, image_dimensions.y},
                .atmosphere_parameters = context.device.get_device_address(atmosphere_gpu_buffer)
            });
            cmd_list.dispatch(((image_dimensions.x + 7)/8), ((image_dimensions.y + 3)/4));
        },
        .name = "render transmittance"
    });
}

