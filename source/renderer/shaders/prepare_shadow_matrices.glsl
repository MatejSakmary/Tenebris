#define DAXA_ENABLE_SHADER_NO_NAMESPACE 1
#define DAXA_ENABLE_IMAGE_OVERLOADS_BASIC 1
#include <shared/shared.inl>
#include "tasks/prepare_shadow_matrices.inl"

#extension GL_EXT_debug_printf : enable

#define _num_cascades 4

layout (local_size_x = _num_cascades) in;

const f32vec3 vertices[8] = f32vec3[](
    f32vec3( 0.0, 0.0, 1.0),
    f32vec3( 0.0, 0.0, 0.0),
    f32vec3( 1.0, 0.0, 0.0),
    f32vec3( 1.0, 0.0, 1.0),
    f32vec3( 1.0, 1.0, 1.0),
    f32vec3( 0.0, 1.0, 1.0),
    f32vec3( 0.0, 1.0, 0.0),
    f32vec3( 1.0, 1.0, 0.0)
);

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

    const f32mat4x4 inverse_projection = inverse(deref(_globals).projection);

    const f32vec4 cam_space_min_unproj = inverse_projection * f32vec4(0.0, 0.0, min_max_depth.x, 1.0);
    const f32 cam_space_min_dist = - cam_space_min_unproj.z / cam_space_min_unproj.w;

    const f32vec4 cam_space_max_unproj = inverse_projection * f32vec4(0.0, 0.0, min_max_depth.y, 1.0);
    const f32 cam_space_max_dist = - cam_space_max_unproj.z / cam_space_max_unproj.w;
    
    // TODO(msakmary) hardcoded near plane - pass it as a param
    const f32vec3 camera_position_world_space = deref(_globals).camera_position - deref(_globals).offset;
    const f32vec3 min_world_pos = camera_position_world_space + (10.0f + cam_space_min_dist) * deref(_globals).camera_front;
    const f32vec3 max_world_pos = camera_position_world_space + (10.0f + cam_space_max_dist) * deref(_globals).camera_front;

    if(gl_LocalInvocationID.x == 0)
    {
        debugPrintfEXT("min dist, max dist %f, %f\n", cam_space_min_dist, cam_space_max_dist);
        for(int i = 0; i < 8; i++)
        {
            f32vec3 dir_vec = normalize(
                deref(_globals).camera_front +
                offsets[i].x * (-deref(_globals).camera_frust_right_offset) +
                offsets[i].y * deref(_globals).camera_frust_top_offset
            );
            // f32 multiplier = i < 4 ? 100.0f : 100.0f + 1000.0f;
            f32 multiplier = i < 4 ? 10.0f + cam_space_min_dist : 10.0f + cam_space_max_dist;


            deref(_frustum_vertices[i]).vertex = camera_position_world_space + dir_vec * multiplier;
        }
    }
    deref(_shadowmap_matrices[gl_LocalInvocationID.x]).cascade_matrix[0] = f32vec4(min_world_pos, -42.0);
    deref(_shadowmap_matrices[gl_LocalInvocationID.x]).cascade_matrix[1] = f32vec4(max_world_pos, -42.0);
}