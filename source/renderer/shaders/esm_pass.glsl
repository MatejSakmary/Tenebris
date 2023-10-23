#define DAXA_ENABLE_IMAGE_OVERLOADS_BASIC 1
#include <shared/shared.inl>
#include "tasks/esm_pass.inl"

layout (local_size_x = WORKGROUP_SIZE) in;

DAXA_DECL_PUSH_CONSTANT(ESMShadowPC, pc)

// Because I do a 5 wide blur I need 4 extra values so I cover the edges
shared daxa_f32 depth_lds [WORKGROUP_SIZE + 4];

daxa_f32 get_depth(daxa_i32 local_thread_index, daxa_i32 offset)
{
    // add two because that is the left overhang of the 5 wide gaussian kernel
    const daxa_i32 index = local_thread_index + offset + 2;
    return depth_lds[index];
}

void main()
{
    const daxa_f32 weights[] = daxa_f32[3](0.375, 0.25, 0.0625);
    const daxa_f32 esm_factor = 80.0;

    daxa_u32vec2 wg_image_coords;
    daxa_u32vec2 image_coords;
#if defined(FIRST_PASS)
    wg_image_coords.x = gl_WorkGroupID.x * WORKGROUP_SIZE;
    wg_image_coords.y = gl_WorkGroupID.y;

    image_coords.x = wg_image_coords.x + gl_LocalInvocationIndex;
    image_coords.y = wg_image_coords.y;

    // const daxa_i32vec2 offset_image_coords = daxa_i32vec2(image_coords + pc.offset);
    for(daxa_i32 wg_offset = 0; wg_offset < 2 * WORKGROUP_SIZE; wg_offset += WORKGROUP_SIZE)
    {
        // I offset the local thread x coord by two which is the overhang I need for 5 wide gaussian
        const daxa_u32 thread_x_image_coord = wg_image_coords.x + wg_offset + gl_LocalInvocationIndex - 2;
        const daxa_u32 clamped_thread_x_image_coord = clamp(thread_x_image_coord, 0, SHADOWMAP_RESOLUTION);

        // offset the position in image by the cascade - The cascades are rendered into a single
        // depth image where each cascade has it's own viewport
        const daxa_u32 cascade_offset_clamped_x_image_coord = clamped_thread_x_image_coord + pc.offset.x;
        const daxa_u32 cascade_offset_y_image_coord = wg_image_coords.y + pc.offset.y;

        if(gl_LocalInvocationIndex + wg_offset < WORKGROUP_SIZE + 4)
        {
            const daxa_f32 d0 = texelFetch(
                daxa_texture2D(_shadowmap),
                daxa_i32vec2(cascade_offset_clamped_x_image_coord, cascade_offset_y_image_coord),
                0).r;
            depth_lds[gl_LocalInvocationIndex + wg_offset] = d0;
        }
    }
#elif defined(SECOND_PASS)
    wg_image_coords.x = gl_WorkGroupID.x;
    wg_image_coords.y = gl_WorkGroupID.y * WORKGROUP_SIZE;

    image_coords.x = wg_image_coords.x;
    image_coords.y = wg_image_coords.y + gl_LocalInvocationIndex;

    // const daxa_i32vec2 offset_image_coords = daxa_i32vec2(image_coords + pc.offset);
    for(daxa_i32 wg_offset = 0; wg_offset < 2 * WORKGROUP_SIZE; wg_offset += WORKGROUP_SIZE)
    {
        // I offset the local thread x coord by two which is the overhang I need for 5 wide gaussian
        const daxa_u32 thread_y_image_coord = wg_image_coords.y + wg_offset + gl_LocalInvocationIndex - 2;
        const daxa_u32 clamped_thread_y_image_coord = clamp(thread_y_image_coord, 0, SHADOWMAP_RESOLUTION);

        // offset the position in image by the cascade - The cascades are rendered into a single
        // depth image where each cascade has it's own viewport
        const daxa_u32 cascade_offset_x_image_coord = wg_image_coords.x + pc.offset.x;
        const daxa_u32 cascade_offset_clamped_y_image_coord = clamped_thread_y_image_coord + pc.offset.y;

        if(gl_LocalInvocationIndex + wg_offset < WORKGROUP_SIZE + 4)
        {
            const daxa_i32vec3 sample_coords = daxa_i32vec3(
                cascade_offset_x_image_coord,
                cascade_offset_clamped_y_image_coord,
                pc.cascade_index
            );
            const daxa_f32 d0 = imageLoad(daxa_image2DArray(_esm_tmp_map), sample_coords).r;
            depth_lds[gl_LocalInvocationIndex + wg_offset] = d0;
        }
    }
#endif
    memoryBarrierShared();
    barrier();

    // https://www.advances.realtimerendering.com/s2009/SIGGRAPH%202009%20-%20Lighting%20Research%20at%20Bungie.pdf
    // slide 54
    const daxa_f32 d0 = get_depth(daxa_i32(gl_LocalInvocationIndex), 0);
    const daxa_f32 sum = weights[2] * exp(esm_factor * (get_depth(daxa_i32(gl_LocalInvocationIndex), -2) - d0)) + 
                    weights[1] * exp(esm_factor * (get_depth(daxa_i32(gl_LocalInvocationIndex), -1) - d0)) +
                    weights[0] + 
                    weights[1] * exp(esm_factor * (get_depth(daxa_i32(gl_LocalInvocationIndex),  1) - d0)) +
                    weights[2] * exp(esm_factor * (get_depth(daxa_i32(gl_LocalInvocationIndex),  2) - d0)); 

    const daxa_f32 esm_result = d0 + log(sum) / esm_factor;

    imageStore(
#if defined(FIRST_PASS)
        daxa_image2DArray(_esm_map),
#elif defined(SECOND_PASS)
        daxa_image2DArray(_esm_target_map),
#endif // SECOND_PASS
        daxa_i32vec3(image_coords, pc.cascade_index),
        daxa_f32vec4(esm_result, 0, 0, 0)
    );
}