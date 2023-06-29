#define DAXA_ENABLE_SHADER_NO_NAMESPACE 1
#define DAXA_ENABLE_IMAGE_OVERLOADS_BASIC 1
#include <shared/shared.inl>
#include "tasks/post_process.inl"

DAXA_DECL_PUSH_CONSTANT(PostProcessPC, pc)

layout (location = 0) in f32vec2 in_uv;
layout (location = 0) out f32vec4 out_color;

void main()
{
    out_color = texture(daxa_sampler2D(_offscreen, pc.sampler_id), in_uv);
}