#pragma once

#include <daxa/daxa.hpp>
#include <daxa/utils/task_list.hpp>

#include "../context.hpp"
#include "../shared/shared.inl"

inline auto get_multiscattering_LUT_pipeline() -> daxa::ComputePipelineCompileInfo
{
    return {
        .shader_info = {
            .source = daxa::ShaderFile{"multiscattering.glsl"},
            .compile_options = {}
        },
        .push_constant_size = sizeof(MultiscatteringPC),
        .debug_name = "compute multiscattering LUT pipeline"
    };
}

inline void task_compute_multiscattering_LUT(Context & context)
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
                daxa::TaskImageAccess::COMPUTE_SHADER_READ_ONLY,
                daxa::ImageMipArraySlice{} 
            },
            {
                context.main_task_list.task_images.at(Images::MULTISCATTERING),
                daxa::TaskImageAccess::COMPUTE_SHADER_WRITE_ONLY,
                daxa::ImageMipArraySlice{}
            },
        },
        .task = [&](daxa::TaskRuntime const & runtime)
        {
            auto cmd_list = runtime.get_command_list();

            auto transmittance_image = runtime.get_images(context.main_task_list.task_images.at(Images::TRANSMITTANCE))[0];
            auto multiscattering_image = runtime.get_images(context.main_task_list.task_images.at(Images::MULTISCATTERING))[0];
            auto atmosphere_gpu_buffer = runtime.get_buffers(context.main_task_list.task_buffers.t_atmosphere_parameters)[0];

            auto multiscattering_dimensions = context.device.info_image(multiscattering_image).size;
            cmd_list.set_pipeline(*context.pipelines.multiscattering);
            cmd_list.push_constant(MultiscatteringPC{
                .transmittance_image = transmittance_image.default_view(),
                .multiscattering_image = multiscattering_image.default_view(),
                .multiscattering_dimensions = {multiscattering_dimensions.x, multiscattering_dimensions.y},
                .sampler_id = context.linear_sampler,
                .atmosphere_parameters = context.device.get_device_address(atmosphere_gpu_buffer)
            });
            cmd_list.dispatch(multiscattering_dimensions.x, multiscattering_dimensions.y);
        },
        .debug_name = "render_multiscattering"
    });
}