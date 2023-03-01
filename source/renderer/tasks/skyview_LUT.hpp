#pragma once

#include <daxa/daxa.hpp>
#include <daxa/utils/task_list.hpp>

#include "../context.hpp"
#include "../shared/shared.inl"

inline auto get_skyview_LUT_pipeline() -> daxa::ComputePipelineCompileInfo
{
    return {
        .shader_info = {
            .source = daxa::ShaderFile{"skyview.glsl"},
            .compile_options = {}
        },
        .push_constant_size = sizeof(SkyviewPC),
        .debug_name = "compute skyview LUT pipeline"
    };
}

inline void task_compute_skyview_LUT(Context & context)
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
                daxa::TaskImageAccess::COMPUTE_SHADER_READ_ONLY,
                daxa::ImageMipArraySlice{}
            },
            {
                context.main_task_list.task_images.at(Images::SKYVIEW),
                daxa::TaskImageAccess::COMPUTE_SHADER_WRITE_ONLY,
                daxa::ImageMipArraySlice{}
            },
        },
        .task = [&](daxa::TaskRuntime const & runtime)
        {
            auto cmd_list = runtime.get_command_list();

            auto transmittance_image = runtime.get_images(context.main_task_list.task_images.at(Images::TRANSMITTANCE))[0];
            auto multiscattering_image = runtime.get_images(context.main_task_list.task_images.at(Images::MULTISCATTERING))[0];
            auto skyview_image = runtime.get_images(context.main_task_list.task_images.at(Images::SKYVIEW))[0];
            auto atmosphere_gpu_buffer = runtime.get_buffers(context.main_task_list.task_buffers.t_atmosphere_parameters)[0];

            auto skyview_dimensions = context.device.info_image(skyview_image).size;
            auto multiscattering_dimensions = context.device.info_image(multiscattering_image).size;

            cmd_list.set_pipeline(*context.pipelines.skyview);
            cmd_list.push_constant(SkyviewPC{
                .transmittance_image = transmittance_image.default_view(),
                .multiscattering_image = multiscattering_image.default_view(),
                .skyview_image = skyview_image.default_view(),
                .skyview_dimensions = {skyview_dimensions.x, skyview_dimensions.y},
                .multiscattering_dimensions = {multiscattering_dimensions.x, multiscattering_dimensions.y},
                .sampler_id = context.linear_sampler,
                .atmosphere_parameters = context.device.get_device_address(atmosphere_gpu_buffer)
            });
            cmd_list.dispatch((skyview_dimensions.x + 7)/8, ((skyview_dimensions.y + 3)/4));
        },
        .debug_name = "render_skyview"
    });
}