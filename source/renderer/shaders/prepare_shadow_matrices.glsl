#define DAXA_ENABLE_SHADER_NO_NAMESPACE 1
#define DAXA_ENABLE_IMAGE_OVERLOADS_BASIC 1
#include <shared/shared.inl>
#include "tasks/prepare_shadow_matrices.inl"

#extension GL_EXT_debug_printf : enable

layout (local_size_x = NUM_CASCADES) in;

const i32vec2 offsets[8] = i32vec2[](
    i32vec2(-1.0,  1.0),
    i32vec2(-1.0, -1.0),
    i32vec2( 1.0, -1.0),
    i32vec2( 1.0,  1.0),
    i32vec2( 1.0,  1.0),
    i32vec2(-1.0,  1.0),
    i32vec2(-1.0, -1.0),
    i32vec2( 1.0, -1.0)
);

void main()
{
    f32vec2 min_max_depth = deref(_depth_limits[0]).limits;

    const f32mat4x4 inverse_projection = inverse(deref(_globals).secondary_projection);

    // Reverse depth so min_depth is actually max dist and other way around
    const f32vec4 cam_space_min_unproj = inverse_projection * f32vec4(0.0, 0.0, min_max_depth.y, 1.0);
    const f32 cam_space_min_dist = - cam_space_min_unproj.z / cam_space_min_unproj.w;

    const f32vec4 cam_space_max_unproj = inverse_projection * f32vec4(0.0, 0.0, min_max_depth.x, 1.0);
    const f32 cam_space_max_dist = - cam_space_max_unproj.z / cam_space_max_unproj.w;
    
    const f32vec3 camera_position_world_space = deref(_globals).secondary_camera_position - deref(_globals).secondary_offset;


    f32 range = cam_space_max_dist - cam_space_min_dist;
    f32 ratio = cam_space_max_dist / cam_space_min_dist;

    f32 cascade_splits[4] = f32[](0.0, 0.0, 0.0, 0.0);
    for(i32 i = 0; i < NUM_CASCADES; i++)
    {
        f32 p = f32(i + 1) / f32(NUM_CASCADES);
        f32 log_scale = cam_space_min_dist * pow(abs(ratio), p);
        f32 uniform_scale = cam_space_min_dist + range * p;
        f32 d = deref(_globals).lambda * (log_scale - uniform_scale) + uniform_scale;
        cascade_splits[i] = d / range;
    }

    u32 thread_idx = gl_LocalInvocationID.x;
    // Debug draw tightened camera frustum
    if(deref(_globals).use_debug_camera)
    {
        u32 idx = atomicAdd(deref(_frustum_indirect).instance_count, 1);
        u32 offset = 8 * idx;

        f32 near_dist = thread_idx == 0 ? cam_space_min_dist : cascade_splits[thread_idx - 1] * range;
        f32 far_dist = cascade_splits[thread_idx] * range;

        // debugPrintfEXT("near dist %f, far dist %f\n", near_dist, far_dist);

        const f32vec3[4] colors = f32vec3[](
            f32vec3(1.0, 0.0, 0.0),
            f32vec3(0.0, 1.0, 0.0),
            f32vec3(0.0, 0.0, 1.0),
            f32vec3(1.0, 1.0, 0.0)
        );

        deref(_frustum_colors[idx]).color = colors[thread_idx];
        for(int i = 0; i < 8; i++)
        {
            f32vec3 dir_vec = 
                deref(_globals).camera_front +
                offsets[i].x * (-deref(_globals).camera_frust_right_offset) +
                offsets[i].y * deref(_globals).camera_frust_top_offset;

            f32 multiplier = i < 4 ? near_dist : far_dist;
            deref(_frustum_vertices[i + offset]).vertex = camera_position_world_space + dir_vec * multiplier;
        }
    }
}