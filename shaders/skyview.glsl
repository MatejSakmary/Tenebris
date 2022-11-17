#define DAXA_SHADER_NO_NAMESPACE
#include <shared/shared.inl>
#include "common_func.glsl"

DAXA_USE_PUSH_CONSTANT(SkyviewPush)
BufferRef(AtmosphereParameters) params = push_constant.atmosphere_parameters;

layout (local_size_x = 8, local_size_y = 4) in;
void main()
{
    imageStore(
        daxa_get_image(image2D, push_constant.skyview_image),
        i32vec2(gl_GlobalInvocationID.xy),
        f32vec4(0.0, 0.0, 0.5, 1.0));
}