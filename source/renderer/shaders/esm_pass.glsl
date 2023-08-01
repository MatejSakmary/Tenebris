#define DAXA_ENABLE_SHADER_NO_NAMESPACE 1
#define DAXA_ENABLE_IMAGE_OVERLOADS_BASIC 1
#include <shared/shared.inl>
#include "tasks/esm_pass.inl"

layout (local_size_x = 8, local_size_y = 4) in;

DAXA_DECL_PUSH_CONSTANT(ESMShadowPC, pc)
#if defined(FIRST_PASS)
#define offset_coord(coord, offset) i32vec2(coord.x + offset, coord.y)
#define get_depth(center, offset) texelFetch(daxa_texture2D(_shadowmap), offset_coord(center, offset), 0).r
#elif defined(SECOND_PASS)
#define offset_coord(coord, offset) i32vec3(coord.x, coord.y + offset, coord.z)
#define get_depth(center, offset) imageLoad(daxa_image2DArray(_esm_tmp_map), offset_coord(center, offset)).r
#endif // VERTICAL_PASS

void main()
{
    if(!all(lessThan(gl_GlobalInvocationID.xy, u32vec2(SHADOWMAP_RESOLUTION))))
    { return; }

    const f32 weights[] = f32[3](0.375, 0.25, 0.0625);
    const f32 esm_factor = 80.0;

#if defined(FIRST_PASS)
    const i32vec2 center = i32vec2(gl_GlobalInvocationID.xy + pc.offset);
#elif defined(SECOND_PASS)
    const i32vec3 center = i32vec3(gl_GlobalInvocationID.xy, pc.cascade_index);
#endif

    const f32 d0 = get_depth(center, 0);
    // https://www.advances.realtimerendering.com/s2009/SIGGRAPH%202009%20-%20Lighting%20Research%20at%20Bungie.pdf
    // slide 54
    const f32 sum = weights[2] * exp(esm_factor * (get_depth(center, -2) - d0)) + 
                    weights[1] * exp(esm_factor * (get_depth(center, -1) - d0)) +
                    weights[0] + 
                    weights[1] * exp(esm_factor * (get_depth(center,  1) - d0)) +
                    weights[2] * exp(esm_factor * (get_depth(center,  2) - d0)); 
    // const f32 sum = exp(esm_factor * (get_depth(center, -2) - d0) + log(weights[2])) + 
    //                 exp(esm_factor * (get_depth(center, -1) - d0) + log(weights[1])) +
    //                 exp(esm_factor * (get_depth(center,  1) - d0) + log(weights[1])) +
    //                 exp(esm_factor * (get_depth(center,  2) - d0) + log(weights[2]));

    const f32 esm_result = d0 + log(sum) / esm_factor;

    imageStore(
#if defined(FIRST_PASS)
        daxa_image2DArray(_esm_map),
#elif defined(SECOND_PASS)
        daxa_image2DArray(_esm_target_map),
#endif // SECOND_PASS
        i32vec3(gl_GlobalInvocationID.xy, pc.cascade_index),
        f32vec4(esm_result, 0, 0, 0)
    );
}