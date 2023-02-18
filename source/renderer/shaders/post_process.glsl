#define DAXA_ENABLE_SHADER_NO_NAMESPACE 1
#define DAXA_ENABLE_IMAGE_OVERLOADS_BASIC 1
#include <shared/shared.inl>

DAXA_USE_PUSH_CONSTANT(PostProcessPC)

layout (location = 0) in f32vec2 in_uv;
layout (location = 0) out f32vec4 out_color;

void main()
{
    out_color = texture( daxa_push_constant.offscreen_id, daxa_push_constant.sampler_id, in_uv);
}