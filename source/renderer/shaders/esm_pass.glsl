#define DAXA_ENABLE_SHADER_NO_NAMESPACE 1
#define DAXA_ENABLE_IMAGE_OVERLOADS_BASIC 1
#include <shared/shared.inl>
#include "tasks/esm_pass.inl"

layout (local_size_x = 8, local_size_y = 4) in;

DAXA_DECL_PUSH_CONSTANT(ESMShadowPC, pc)

void main()
{
    if(!all(lessThan(gl_GlobalInvocationID.xy, pc.shadowmap_dimensions)))
    { return; }

    f32 depth = texelFetch(daxa_texture2D(_shadowmap), i32vec2(gl_GlobalInvocationID.xy), 0).r;

    const f32 c = 80.0;
    imageStore(daxa_image2D(_esm_map), i32vec2(gl_GlobalInvocationID.xy), f32vec4(exp(depth * c), 0, 0, 0));
    // imageStore(daxa_image2D(_esm_map), i32vec2(gl_GlobalInvocationID.xy), f32vec4(depth, 0, 0, 0));
}