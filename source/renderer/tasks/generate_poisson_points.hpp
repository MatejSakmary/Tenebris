#pragma once

#include <daxa/daxa.hpp>
#include <daxa/utils/task_list.hpp>

#include "../context.hpp"
#include "../shared/shared.inl"

inline auto get_generate_poisson_points_pipeline(const Context & context) -> daxa::RasterPipelineCompileInfo
{
    return {
        .vertex_shader_info = { 
            .source = daxa::ShaderFile{"poisson_points.glsl"},
            .compile_options = {
                .defines = {{"_VERTEX", ""}},
            },
        },
        .fragment_shader_info = {
            .source = daxa::ShaderFile{"poisson_points.glsl"},
            .compile_options = {
                .defines = {{"_FRAGMENT", ""}},
            },
        },
        .color_attachments = {{.format = daxa::Format::R32_SFLOAT}},
        .depth_test = { 
            .depth_attachment_format = daxa::Format::D32_SFLOAT,
            .enable_depth_test = true,
            .enable_depth_write = true,
        },
        .raster = { 
            .primitive_topology = daxa::PrimitiveTopology::POINT_LIST,
            .primitive_restart_enable = false,
            .polygon_mode = daxa::PolygonMode::FILL,
            .face_culling = daxa::FaceCullFlagBits::BACK_BIT,
        },
        .push_constant_size = sizeof(GeneratePoissonPC),
        .debug_name = "poisson points pipeline"
    };
}

inline void task_generate_poisson_points(Context & context)
{
    context.main_task_list.task_list.add_task({
        .used_buffers =
        { 
            {
                context.main_task_list.task_buffers.t_poisson_points,
                daxa::TaskBufferAccess::VERTEX_SHADER_READ_WRITE 
            },
            {
                context.main_task_list.task_buffers.t_poisson_header,
                daxa::TaskBufferAccess::VERTEX_SHADER_READ_WRITE 
            },
        },
        .used_images = 
        { 
            { 
                context.main_task_list.task_images.at(Images::POISSON_RESOLVE),
                daxa::TaskImageAccess::COLOR_ATTACHMENT,
                daxa::ImageMipArraySlice{}
            },
            { 
                context.main_task_list.task_images.at(Images::DEPTH),
                daxa::TaskImageAccess::DEPTH_ATTACHMENT,
                daxa::ImageMipArraySlice{.image_aspect = daxa::ImageAspectFlagBits::DEPTH} 
            },
        },
        .task = [&](daxa::TaskRuntimeInterface const & runtime)
        {
            auto cmd_list = runtime.get_command_list();

            auto poisson_resolve = runtime.get_images(context.main_task_list.task_images.at(Images::POISSON_RESOLVE))[0];
            auto depth_image = runtime.get_images(context.main_task_list.task_images.at(Images::DEPTH))[0];

            auto resolve_dimensions = context.device.info_image(poisson_resolve).size;
            auto poisson_points_gpu_buffer = runtime.get_buffers(context.main_task_list.task_buffers.t_poisson_points)[0];
            auto poisson_header_gpu_buffer = runtime.get_buffers(context.main_task_list.task_buffers.t_poisson_header)[0];

            cmd_list.begin_renderpass({
                .color_attachments = 
                {{
                    .image_view = poisson_resolve.default_view(),
                    .load_op = daxa::AttachmentLoadOp::CLEAR,
                    .clear_value = std::array<f32, 4>{0.0, 0.0, 0.0, 1.0} 
                }},
                .depth_attachment = 
                {{
                    .image_view = depth_image.default_view(),
                    .layout = daxa::ImageLayout::ATTACHMENT_OPTIMAL,
                    .load_op = daxa::AttachmentLoadOp::CLEAR,
                    .store_op = daxa::AttachmentStoreOp::STORE,
                    .clear_value = daxa::ClearValue{daxa::DepthValue{1.0f, 0}},
                }},
                .render_area = {.x = 0, .y = 0, .width = resolve_dimensions.x , .height = resolve_dimensions.y}
            });

            cmd_list.set_pipeline(*context.pipelines.generate_poisson_points);
            cmd_list.push_constant(GeneratePoissonPC{
                .poisson_points = context.device.get_device_address(poisson_points_gpu_buffer),
                .poisson_header = context.device.get_device_address(poisson_header_gpu_buffer),
                .dispatch_size = context.poisson_info.dispatch_size
            });
            cmd_list.draw({.vertex_count = u32(context.poisson_info.dispatch_size)});
            cmd_list.end_renderpass();
        },
        .debug_name = "generate poisson points",
    });
}