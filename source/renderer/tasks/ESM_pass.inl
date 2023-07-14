#pragma once

#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

#include "../shared/shared.inl"

struct ESMShadowPC
{
    u32vec2 shadowmap_dimensions;
};

DAXA_DECL_TASK_USES_BEGIN(ESMPassTaskBase, DAXA_UNIFORM_BUFFER_SLOT0)
DAXA_TASK_USE_IMAGE(_shadowmap, REGULAR_2D, COMPUTE_SHADER_READ)
DAXA_TASK_USE_IMAGE(_esm_map, REGULAR_2D, COMPUTE_SHADER_WRITE)
DAXA_DECL_TASK_USES_END()

#if __cplusplus
#include "../context.hpp"
inline auto get_esm_pass_pipeline() -> daxa::ComputePipelineCompileInfo {
    return {
        .shader_info = { .source = daxa::ShaderFile{"ESM_pass.glsl"}, }, 
        .push_constant_size = sizeof(ESMShadowPC),
        .name = "esm pass pipeline"
    };
}

struct ESMPassTask : ESMPassTaskBase
{
    static constexpr u32 threadsX = 8;
    static constexpr u32 threadsY = 4;

    Context * context = {};
    void callback(daxa::TaskInterface ti)
    {
        auto cmd_list = ti.get_command_list();

        auto image_dimensions = context->device.info_image(uses._shadowmap.image()).size;
        cmd_list.set_uniform_buffer(ti.uses.get_uniform_buffer_info());
        cmd_list.push_constant(ESMShadowPC{ .shadowmap_dimensions = {image_dimensions.x, image_dimensions.y} });

        cmd_list.set_pipeline(*(context->pipelines.esm_pass));
        
        cmd_list.dispatch((image_dimensions.x + threadsX - 1) / threadsX, (image_dimensions.y + threadsY - 1) / threadsY);
    }
};
#endif //__cplusplus