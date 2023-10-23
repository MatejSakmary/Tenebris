#define DAXA_ENABLE_IMAGE_OVERLOADS_BASIC 1
#include <shared/shared.inl>
#include "tasks/prepare_shadow_matrices.inl"

#extension GL_EXT_debug_printf : enable

layout (local_size_x = NUM_CASCADES) in;

const daxa_i32vec2 offsets[8] = daxa_i32vec2[](
    daxa_i32vec2(-1.0,  1.0),
    daxa_i32vec2(-1.0, -1.0),
    daxa_i32vec2( 1.0, -1.0),
    daxa_i32vec2( 1.0,  1.0),
    daxa_i32vec2( 1.0,  1.0),
    daxa_i32vec2(-1.0,  1.0),
    daxa_i32vec2(-1.0, -1.0),
    daxa_i32vec2( 1.0, -1.0)
);

daxa_f32mat4x4 inverse_rotation_translation(daxa_f32mat3x3 r, daxa_f32vec3 t)
{
    daxa_f32mat4x4 inv = daxa_f32mat4x4(daxa_f32vec4(r[0][0], r[1][0], r[2][0], 0.0f),
                              daxa_f32vec4(r[0][1], r[1][1], r[2][1], 0.0f),
                              daxa_f32vec4(r[0][2], r[1][2], r[2][2], 0.0f),
                              daxa_f32vec4(  0.0f,    0.0f,   0.0f,   1.0f));
    inv[3][0] = -dot(t, r[0]);
    inv[3][1] = -dot(t, r[1]);
    inv[3][2] = -dot(t, r[2]);
    return inv;
}

daxa_f32mat4x4 orthographic_projection(daxa_f32 l, daxa_f32 b, daxa_f32 r, daxa_f32 t, daxa_f32 zn, daxa_f32 zf)
{
    return daxa_f32mat4x4(daxa_f32vec4(  2.0 / (r - l)     ,       0.0        ,        0.0        , 0.0),
                     daxa_f32vec4(       0.0          ,   2.0 / (t - b)  ,        0.0        , 0.0),
                     daxa_f32vec4(       0.0          ,       0.0        ,   1.0 / (zf - zn) , 0.0),
                     daxa_f32vec4((l + r) / (l - r)   , (t + b)/(b - t)  ,   zn / (zn - zf)  , 1.0));
}

void main()
{
    daxa_f32vec2 min_max_depth = deref(_depth_limits[0]).limits;

    daxa_f32mat4x4 inverse_projection;
    daxa_f32vec3 camera_position_world_space;
    if(deref(_globals).use_debug_camera)
    {
        inverse_projection = inverse(deref(_globals).secondary_projection);
        camera_position_world_space = deref(_globals).secondary_camera_position - deref(_globals).secondary_offset;
    } else {
        inverse_projection = inverse(deref(_globals).projection);
        camera_position_world_space = deref(_globals).camera_position - deref(_globals).offset;
    }

    // Reverse depth so min_depth is actually max dist and other way around
    const daxa_f32vec4 cam_space_min_unproj = inverse_projection * daxa_f32vec4(0.0, 0.0, min_max_depth.y, 1.0);
    const daxa_f32 cam_space_min_dist = - cam_space_min_unproj.z / cam_space_min_unproj.w;

    const daxa_f32vec4 cam_space_max_unproj = inverse_projection * daxa_f32vec4(0.0, 0.0, min_max_depth.x, 1.0);
    const daxa_f32 cam_space_max_dist = - cam_space_max_unproj.z / cam_space_max_unproj.w;

    daxa_f32 range = cam_space_max_dist - cam_space_min_dist;
    daxa_f32 ratio = cam_space_max_dist / cam_space_min_dist;

    daxa_f32 cascade_splits[4] = daxa_f32[](0.0, 0.0, 0.0, 0.0);
    for(daxa_i32 i = 0; i < NUM_CASCADES; i++)
    {
        daxa_f32 p = daxa_f32(i + 1) / daxa_f32(NUM_CASCADES);
        daxa_f32 log_scale = cam_space_min_dist * pow(abs(ratio), p);
        daxa_f32 uniform_scale = cam_space_min_dist + range * p;
        daxa_f32 d = deref(_globals).lambda * (log_scale - uniform_scale) + uniform_scale;
        cascade_splits[i] = d / range;
    }

    daxa_u32 thread_idx = gl_LocalInvocationID.x;

    daxa_f32 near_dist = thread_idx == 0 ? cam_space_min_dist : cascade_splits[thread_idx - 1] * range;
    daxa_f32 far_dist = cascade_splits[thread_idx] * range;

    daxa_f32vec3 frustum_vertices[8];
    daxa_f32vec3 vertices_sum = daxa_f32vec3(0.0, 0.0, 0.0);
    for(int i = 0; i < 8; i++)
    {
        daxa_f32vec3 dir_vec = 
            deref(_globals).camera_front +
            offsets[i].x * (-deref(_globals).camera_frust_right_offset) +
            offsets[i].y * deref(_globals).camera_frust_top_offset;

        daxa_f32 multiplier = i < 4 ? near_dist : far_dist;
        frustum_vertices[i] = camera_position_world_space + dir_vec * multiplier;
        vertices_sum += frustum_vertices[i];
    }
    daxa_f32vec3 average_vertex = vertices_sum / 8;

    daxa_f32mat3x3 light_camera_rotation;
    daxa_f32vec3 camera_right = normalize(deref(_globals).camera_frust_right_offset);
    daxa_f32vec3 world_up = daxa_f32vec3(0.0, 0.0, 1.0);

    daxa_f32vec3 sun_direction = deref(_globals).sun_direction;
    daxa_f32vec3 front = -sun_direction;
    daxa_f32vec3 right = normalize(cross(front, world_up));
    daxa_f32vec3 up = normalize(cross(front, right));
    light_camera_rotation = daxa_f32mat3x3(right, up, front);

    daxa_f32mat4x4 light_view = inverse_rotation_translation(light_camera_rotation, average_vertex);

    // Calculate an AABB around the frustum corners
    const daxa_f32 max_float = 3.402823466e+38F;
    daxa_f32vec3 min_extends = daxa_f32vec3(max_float, max_float, max_float);
    daxa_f32vec3 max_extends = daxa_f32vec3(-max_float, -max_float, -max_float);

    for(daxa_i32 i = 0; i < 8; i++)
    {
        daxa_f32vec3 proj_corner = (light_view * daxa_f32vec4(frustum_vertices[i], 1.0)).xyz;
        min_extends = min(min_extends, proj_corner);
        max_extends = max(max_extends, proj_corner);
    }

    daxa_f32vec3 cascade_extends = max_extends - min_extends;
    daxa_f32vec3 shadow_camera_pos = average_vertex + sun_direction * -min_extends.z;

    daxa_f32mat4x4 shadow_view = inverse_rotation_translation(light_camera_rotation, shadow_camera_pos);
    daxa_f32mat4x4 shadow_proj = orthographic_projection(
        min_extends.x, min_extends.y,
        max_extends.x, max_extends.y,
        0.0, cascade_extends.z
    );

    deref(_cascade_data[thread_idx]) = ShadowmapCascadeData(shadow_view, shadow_proj, far_dist, cascade_extends.z);

    // Debug draw tightened camera frustum split into cascades
    if(deref(_globals).use_debug_camera)
    {
        const daxa_f32vec3[4] colors = daxa_f32vec3[](
            daxa_f32vec3(1.0, 0.0, 0.0),
            daxa_f32vec3(0.0, 1.0, 0.0),
            daxa_f32vec3(0.0, 0.0, 1.0),
            daxa_f32vec3(1.0, 1.0, 0.0)
        );

        if(false)//thread_idx == 0 || thread_idx == 1)// || thread_idx == 2 || thread_idx == 3) 
        {
            // Visualize frustum cascade splits
            daxa_u32 idx = atomicAdd(deref(_frustum_indirect).instance_count, 1);
            daxa_u32 offset = 8 * idx;
            deref(_frustum_colors[idx]).color = colors[thread_idx];
            for(daxa_i32 i = 0; i < 8; i++)
            {
                deref(_frustum_vertices[i + offset]).vertex = frustum_vertices[i];
            }
        
            // Visualize orthographic camera
            daxa_f32mat4x4 inverse_shadow_proj_view = inverse(shadow_proj * shadow_view);

            idx = atomicAdd(deref(_frustum_indirect).instance_count, 1);
            offset = 8 * idx;
            deref(_frustum_colors[idx]).color = daxa_f32vec3(1.0, 1.0, 1.0);

            for(daxa_i32 i = 0; i < 8; i++)
            {
                daxa_f32vec4 ndc_pos = daxa_f32vec4(offsets[i], i < 4 ? 0.0 : 1.0, 1.0);
                daxa_f32vec4 unproj_world_space = inverse_shadow_proj_view * daxa_f32vec4(ndc_pos);
                daxa_f32vec3 world_space = unproj_world_space.xyz / unproj_world_space.w;
                deref(_frustum_vertices[i + offset]).vertex = world_space;
            }
        }
    }
}