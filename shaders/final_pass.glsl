#define DAXA_SHADER_NO_NAMESPACE

#include <shared/shared.inl>

DAXA_USE_PUSH_CONSTANT(FinalPassPush)

layout (location = 0) in f32vec2 in_uv;
layout (location = 0) out f32vec4 out_color;

void main()
{
    out_color = texture(
        sampler2D
        (
            daxa_get_Texture(texture2D, push_constant.image_id),
            daxa_get_Sampler(push_constant.sampler_id)
        ),
        in_uv
    );
}