#pragma once

#include <daxa/daxa.inl>
#include <daxa/utils/task_list.inl>

#include "../shared/shared.inl"

struct BC6HCompressPC
{
    u32vec2 TextureSizeInBlocks;
    f32vec2 TextureSizeRcp;
    SamplerId point_sampler;
};

DAXA_DECL_TASK_USES_BEGIN(BC6HCompressTaskBase, DAXA_UNIFORM_BUFFER_SLOT0)
DAXA_TASK_USE_IMAGE(_src_texture, REGULAR_2D, COMPUTE_SHADER_READ)
DAXA_TASK_USE_IMAGE(_dst_texture, REGULAR_2D, COMPUTE_SHADER_WRITE)
DAXA_DECL_TASK_USES_END()

#if __cplusplus
#include "../context.hpp"
struct TextureManagerInfo;

inline auto get_BC6H_pipeline() -> daxa::ComputePipelineCompileInfo
{
    return {
        .shader_info = { .source = daxa::ShaderFile{"bc6h_compress.glsl"}, },
        .push_constant_size = sizeof(BC6HCompressPC),
        .name = "compress bc6h texture pipeline"
    };
}

struct BC6HCompressTask : BC6HCompressTaskBase
{
    TextureManagerInfo *info = {};
    SamplerId nearest_sampler = {};

	static constexpr u32 threadsX = 8;
	static constexpr u32 threadsY = 8;
    static constexpr u32 BC_BLOCK_SIZE = 4;
    static constexpr u32 divx = threadsX * BC_BLOCK_SIZE;
    static constexpr u32 divy = threadsY * BC_BLOCK_SIZE;

    void callback(daxa::TaskInterface ti)
    {
        auto cmd_list = ti.get_command_list();

        auto image_dimensions = info->device.info_image(uses._src_texture.image()).size;
        cmd_list.set_uniform_buffer(ti.uses.get_uniform_buffer_info());
        cmd_list.push_constant(BC6HCompressPC{
            .TextureSizeInBlocks = u32vec2{
                (image_dimensions.x + BC_BLOCK_SIZE - 1) / BC_BLOCK_SIZE,
                (image_dimensions.y + BC_BLOCK_SIZE - 1) / BC_BLOCK_SIZE,
            },
            .TextureSizeRcp = f32vec2{1.0f / image_dimensions.x, 1.0f / image_dimensions.y},
            .point_sampler = nearest_sampler
        });
        cmd_list.set_pipeline(*(info->compress_pipeline));

        cmd_list.dispatch(((image_dimensions.x + divx - 1)/divx), ((image_dimensions.y + divy - 1)/divy));
    }
};
#endif //cplusplus