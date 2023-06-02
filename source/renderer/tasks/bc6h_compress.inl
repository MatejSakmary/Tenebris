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

DAXA_INL_TASK_USE_BEGIN(BC6HCompressTaskBase, DAXA_CBUFFER_SLOT0)
DAXA_INL_TASK_USE_IMAGE(_src_texture, daxa_Image2Df32, COMPUTE_SHADER_READ)
DAXA_INL_TASK_USE_IMAGE(_dst_texture, daxa_RWImage2Du32, COMPUTE_SHADER_WRITE)
DAXA_INL_TASK_USE_END()

#if __cplusplus
#include "../context.hpp"

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
    Context *context = {};

	static constexpr u32 threadsX = 8;
	static constexpr u32 threadsY = 8;
    static constexpr u32 BC_BLOCK_SIZE = 4;
    static constexpr u32 divx = threadsX * BC_BLOCK_SIZE;
    static constexpr u32 divy = threadsY * BC_BLOCK_SIZE;

    void callback(daxa::TaskInterface ti)
    {
        auto cmd_list = ti.get_command_list();

        auto image_dimensions = context->device.info_image(uses._src_texture.image()).size;
        cmd_list.set_constant_buffer(ti.uses.constant_buffer_set_info());
        cmd_list.push_constant(BC6HCompressPC{
            .TextureSizeInBlocks = u32vec2{
                (image_dimensions.x + BC_BLOCK_SIZE - 1) / BC_BLOCK_SIZE,
                (image_dimensions.y + BC_BLOCK_SIZE - 1) / BC_BLOCK_SIZE,
            },
            .TextureSizeRcp = f32vec2{1.0f / image_dimensions.x, 1.0f / image_dimensions.y},
            .point_sampler = context->nearest_sampler
        });
        cmd_list.set_pipeline(*(context->pipelines.BC6H_compress));

        cmd_list.dispatch(((image_dimensions.x + divx - 1)/divx), ((image_dimensions.y + divy - 1)/divy));
    }
};
#endif //cplusplus