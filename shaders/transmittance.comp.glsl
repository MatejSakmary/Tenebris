#define DAXA_SHADER_NO_NAMESPACE
#include <shared/shared.inl>

DAXA_USE_PUSH_CONSTANT(TransmittancePush)

layout (local_size_x = 8, local_size_y = 4) in;
void main()
{
    u32vec3 thread_id = gl_GlobalInvocationID.xyz;

    if(thread_id.x >= push_constant.dimensions.x || thread_id.y >= push_constant.dimensions.y)
        return;
    
    imageStore(
        daxa_access_Image(image2D, push_constant.transmittance_image),
        i32vec2(thread_id.xy),
        // f32vec4(1.0, 1.0, 0.0, 1.0));
        f32vec4(thread_id.xy / f32vec2(push_constant.dimensions.xy), 0.0, 1.0));
}