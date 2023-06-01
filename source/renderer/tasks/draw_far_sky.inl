#pragma once

#include <daxa/daxa.inl>
#include <daxa/utils/task_list.inl>

#include "../shared/shared.inl"

struct DrawSkyPC
{
    daxa_SamplerId sampler_id;
    daxa_u32vec2 skyview_dimensions;
};

DAXA_INL_TASK_USE_BEGIN(DrawFarSkyTaskBase, DAXA_CBUFFER_SLOT0)
DAXA_INL_TASK_USE_BUFFER(_globals, daxa_BufferPtr(Globals), FRAGMENT_SHADER_READ)
DAXA_INL_TASK_USE_IMAGE(_offscreen, daxa_Image2Df32, COLOR_ATTACHMENT)
DAXA_INL_TASK_USE_IMAGE(_depth, daxa_Image2Df32, DEPTH_ATTACHMENT)
DAXA_INL_TASK_USE_IMAGE(_skyview, daxa_Image2Df32, FRAGMENT_SHADER_READ)
DAXA_INL_TASK_USE_END()

#if __cplusplus
#include "../context.hpp"

inline auto get_draw_far_sky_pipeline() -> daxa::RasterPipelineCompileInfo {
    return {
        .vertex_shader_info = daxa::ShaderCompileInfo{ .source = daxa::ShaderFile{"screen_triangle.glsl"} },
        .fragment_shader_info = daxa::ShaderCompileInfo{ .source = daxa::ShaderFile{"draw_far_sky.glsl"} },
        .color_attachments = {{.format = daxa::Format::R16G16B16A16_SFLOAT}},
        .depth_test = 
        { 
            .depth_attachment_format = daxa::Format::D32_SFLOAT,
            .enable_depth_test = true,
            .enable_depth_write = false,
            .depth_test_compare_op = daxa::CompareOp::EQUAL
        },
        .raster = 
        { 
            .polygon_mode = daxa::PolygonMode::FILL,
            .face_culling = daxa::FaceCullFlagBits::BACK_BIT,
        },
        .push_constant_size = sizeof(DrawSkyPC),
        .name = "far sky pipeline"
    };
}
struct DrawFarSkyTask : DrawFarSkyTaskBase
{
    Context *context = {}; 
    void callback(daxa::TaskInterface ti)
    {
        auto cmd_list = ti.get_command_list();

        auto swapchain_dimensions = context->swapchain.get_surface_extent();
        auto skyview_dimensions = context->device.info_image(uses._skyview.image()).size;

        cmd_list.set_constant_buffer(ti.uses.constant_buffer_set_info());
        cmd_list.begin_renderpass({
            .color_attachments = 
            {{
                .image_view = {uses._offscreen.view()},
                .load_op = daxa::AttachmentLoadOp::LOAD
            }},
            .depth_attachment = 
            {{
                .image_view = {uses._depth.view()},
                .layout = daxa::ImageLayout::ATTACHMENT_OPTIMAL,
                .load_op = daxa::AttachmentLoadOp::LOAD,
                .store_op = daxa::AttachmentStoreOp::STORE,
            }},
            .render_area = {.x = 0, .y = 0, .width = swapchain_dimensions.x , .height = swapchain_dimensions.y}
        });

        cmd_list.set_pipeline(*context->pipelines.draw_far_sky);
        cmd_list.push_constant(DrawSkyPC{
            .sampler_id = context->linear_sampler,
            .skyview_dimensions = {skyview_dimensions.x, skyview_dimensions.y},
        });
        cmd_list.draw({.vertex_count = 3});
        cmd_list.end_renderpass();
    }
};
#endif