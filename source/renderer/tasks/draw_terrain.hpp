#pragma once

#include <daxa/daxa.hpp>
#include <daxa/utils/task_list.hpp>

#include "../context.hpp"
#include "../shared/shared.inl"

inline auto get_draw_terrain_pipeline(const Context & context) -> daxa::RasterPipelineCompileInfo
{
    return {
        .vertex_shader_info = daxa::ShaderCompileInfo{ .source = daxa::ShaderFile{"draw_terrain.glsl"}, },
        .tesselation_control_shader_info = daxa::ShaderCompileInfo{ .source = daxa::ShaderFile{"draw_terrain.glsl"}, },
        .tesselation_evaluation_shader_info = daxa::ShaderCompileInfo{ .source = daxa::ShaderFile{"draw_terrain.glsl"}, },
        .fragment_shader_info = daxa::ShaderCompileInfo{ .source = daxa::ShaderFile{"draw_terrain.glsl"}, },
        .color_attachments = {{.format = daxa::Format::R16G16B16A16_SFLOAT}},
        .depth_test = { 
            .depth_attachment_format = daxa::Format::D32_SFLOAT,
            .enable_depth_test = true,
            .enable_depth_write = true,
        },
        .raster = { 
            .primitive_topology = daxa::PrimitiveTopology::PATCH_LIST,
            .primitive_restart_enable = false,
            .polygon_mode = daxa::PolygonMode::LINE,
            .face_culling = daxa::FaceCullFlagBits::BACK_BIT,
        },
        .tesselation = { .control_points = 3 },
        .push_constant_size = sizeof(DrawTerrainPC),
        .debug_name = "terrain pipeline"
    };
}

inline void task_draw_terrain(Context & context)
{
    context.main_task_list.task_list.add_task({
        .used_buffers =
        { 
            {
                context.main_task_list.task_buffers.t_terrain_vertices,
                daxa::TaskBufferAccess::VERTEX_SHADER_READ_ONLY,
            },
            {
                context.main_task_list.task_buffers.t_terrain_indices,
                daxa::TaskBufferAccess::VERTEX_SHADER_READ_ONLY,
            },
            {
                context.main_task_list.task_buffers.t_camera_parameters,
                daxa::TaskBufferAccess::SHADER_READ_ONLY
            } 
        },
        .used_images = 
        { 
            { 
                context.main_task_list.task_images.at(Images::OFFSCREEN),
                daxa::TaskImageAccess::SHADER_WRITE_ONLY,
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

            auto dimensions = context.swapchain.get_surface_extent();
            auto offscreen_image = runtime.get_images(context.main_task_list.task_images.at(Images::OFFSCREEN))[0];
            auto depth_image = runtime.get_images(context.main_task_list.task_images.at(Images::DEPTH))[0];

            auto camera_gpu_buffer = runtime.get_buffers(context.main_task_list.task_buffers.t_camera_parameters)[0];

            auto index_buffer = runtime.get_buffers(context.main_task_list.task_buffers.t_terrain_indices)[0];
            auto vertex_buffer = runtime.get_buffers(context.main_task_list.task_buffers.t_terrain_vertices)[0];
            cmd_list.begin_renderpass({
                .color_attachments = 
                {{
                    .image_view = offscreen_image.default_view(),
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
                .render_area = {.x = 0, .y = 0, .width = dimensions.x , .height = dimensions.y}
            });

            cmd_list.set_pipeline(*context.pipelines.draw_terrain);
            cmd_list.push_constant(DrawTerrainPC{
                .vertices = context.device.get_device_address(vertex_buffer),
                .camera_parameters = context.device.get_device_address(camera_gpu_buffer),
                .terrain_scale = context.terrain_params.scale,
                .delta = context.terrain_params.delta,
                .min_depth = context.terrain_params.min_depth,
                .max_depth = context.terrain_params.max_depth,
                .min_tess_level = context.terrain_params.min_tess_level,
                .max_tess_level = context.terrain_params.max_tess_level
            });
            cmd_list.set_index_buffer(index_buffer, 0, sizeof(u32));
            cmd_list.draw_indexed({.index_count = u32(context.buffers.terrain_indices.cpu_buffer.size())});
            // cmd_list.draw_indexed({.index_count = u32(3)});
            cmd_list.end_renderpass();
        },
        .debug_name = "draw terrain",
    });
}