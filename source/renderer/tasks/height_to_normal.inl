#pragma once

#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

#include "../shared/shared.inl"

struct HeightToNormalPC
{
    u32vec2 texture_size;
    SamplerId point_sampler;
};

DAXA_DECL_TASK_USES_BEGIN(HeightToNormalTaskBase, DAXA_UNIFORM_BUFFER_SLOT0)
DAXA_TASK_USE_IMAGE(_height_texture, REGULAR_2D, COMPUTE_SHADER_READ)
DAXA_TASK_USE_IMAGE(_normal_texture, REGULAR_2D, COMPUTE_SHADER_WRITE)
DAXA_DECL_TASK_USES_END()

#if __cplusplus
#include "../context.hpp"
struct TextureManagerInfo;

inline auto get_height_to_normal_pipeline() -> daxa::ComputePipelineCompileInfo
{
    return {
        .shader_info = { .source = daxa::ShaderFile{"height_to_normal.glsl"}},
        .push_constant_size = sizeof(HeightToNormalPC),
        .name = "height to normal pipeline"
    };
}

struct HeightToNormalTask : HeightToNormalTaskBase
{
    TextureManagerInfo * info = {};
    SamplerId nearest_sampler = {};

    static constexpr u32 threadsX = 8;
    static constexpr u32 threadsY = 4;

    void callback(daxa::TaskInterface ti)
    {
        auto cmd_list = ti.get_command_list();

        auto image_dimensions = info->device.info_image(uses._height_texture.image()).size;
        cmd_list.set_uniform_buffer(ti.uses.get_uniform_buffer_info());
        cmd_list.push_constant(HeightToNormalPC{
            .texture_size = u32vec2{image_dimensions.x, image_dimensions.y},
            .point_sampler = nearest_sampler
        });
        cmd_list.set_pipeline(*(info->height_to_normal_pipeline));
        cmd_list.dispatch((image_dimensions.x + threadsX - 1)/ threadsX, (image_dimensions.y + threadsY - 1)/ threadsY);
    }
};
#endif // __cplusplus