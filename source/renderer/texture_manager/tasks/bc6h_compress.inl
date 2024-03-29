#pragma once

#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

#include "../../shared/shared.inl"

struct BC6HCompressPC
{
    daxa_u32vec2 TextureSizeInBlocks;
    daxa_f32vec2 TextureSizeRcp;
    daxa_SamplerId point_sampler;
};

DAXA_DECL_TASK_USES_BEGIN(BC6HCompressTaskBase, DAXA_UNIFORM_BUFFER_SLOT0)
DAXA_TASK_USE_IMAGE(_src_texture, REGULAR_2D, COMPUTE_SHADER_SAMPLED)
DAXA_TASK_USE_IMAGE(_dst_texture, REGULAR_2D, COMPUTE_SHADER_STORAGE_WRITE_ONLY)
DAXA_DECL_TASK_USES_END()

#if __cplusplus
#include "../../context.hpp"
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
    std::shared_ptr<daxa::ComputePipeline> compress = {};
    daxa::Device device = {};
    daxa_SamplerId nearest_sampler = {};

	static constexpr daxa_u32 threadsX = 8;
	static constexpr daxa_u32 threadsY = 8;
    static constexpr daxa_u32 BC_BLOCK_SIZE = 4;
    static constexpr daxa_u32 divx = threadsX * BC_BLOCK_SIZE;
    static constexpr daxa_u32 divy = threadsY * BC_BLOCK_SIZE;

    void callback(daxa::TaskInterface ti)
    {
        auto & cmd_list = ti.get_recorder();

        auto image_dimensions = device.info_image(uses._src_texture.image()).value().size;
        cmd_list.set_uniform_buffer(ti.uses.get_uniform_buffer_info());
        cmd_list.push_constant(BC6HCompressPC{
            .TextureSizeInBlocks = daxa_u32vec2{
                (image_dimensions.x + BC_BLOCK_SIZE - 1) / BC_BLOCK_SIZE,
                (image_dimensions.y + BC_BLOCK_SIZE - 1) / BC_BLOCK_SIZE,
            },
            .TextureSizeRcp = daxa_f32vec2{1.0f / image_dimensions.x, 1.0f / image_dimensions.y},
            .point_sampler = nearest_sampler
        });
        cmd_list.set_pipeline(*(compress));

        cmd_list.dispatch(((image_dimensions.x + divx - 1)/divx), ((image_dimensions.y + divy - 1)/divy));
    }
};
#endif // __cplusplus