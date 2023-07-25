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

f32mat4x4 inverse_rotation_translation(f32mat3x3 r, f32vec3 t)
{
    f32mat4x4 inv = f32mat4x4(f32vec4(r[0][0], r[1][0], r[2][0], 0.0f),
                              f32vec4(r[0][1], r[1][1], r[2][1], 0.0f),
                              f32vec4(r[0][2], r[1][2], r[2][2], 0.0f),
                              f32vec4(  0.0f,    0.0f,   0.0f,   1.0f));
    inv[3][0] = -dot(t, r[0]);
    inv[3][1] = -dot(t, r[1]);
    inv[3][2] = -dot(t, r[2]);
    return inv;
}

f32mat4x4 orthographic_projection(f32 l, f32 b, f32 r, f32 t, f32 zn, f32 zf)
{
    return f32mat4x4(f32vec4(  2.0 / (r - l)     ,       0.0        ,        0.0        , 0.0),
                     f32vec4(       0.0          ,   2.0 / (t - b)  ,        0.0        , 0.0),
                     f32vec4(       0.0          ,       0.0        ,   1.0 / (zf - zn) , 0.0),
                     f32vec4((l + r) / (l - r)   , (t + b)/(b - t)  ,   zn / (zn - zf)  , 1.0));
}

void main()
{
    f32vec2 min_max_depth = deref(_depth_limits[0]).limits;

    f32mat4x4 inverse_projection;
    f32vec3 camera_position_world_space;
    if(deref(_globals).use_debug_camera)
    {
        inverse_projection = inverse(deref(_globals).secondary_projection);
        camera_position_world_space = deref(_globals).secondary_camera_position - deref(_globals).secondary_offset;
    } else {
        inverse_projection = inverse(deref(_globals).projection);
        camera_position_world_space = deref(_globals).camera_position - deref(_globals).offset;
    }

    // Reverse depth so min_depth is actually max dist and other way around
    const f32vec4 cam_space_min_unproj = inverse_projection * f32vec4(0.0, 0.0, min_max_depth.y, 1.0);
    const f32 cam_space_min_dist = - cam_space_min_unproj.z / cam_space_min_unproj.w;

    const f32vec4 cam_space_max_unproj = inverse_projection * f32vec4(0.0, 0.0, min_max_depth.x, 1.0);
    const f32 cam_space_max_dist = - cam_space_max_unproj.z / cam_space_max_unproj.w;

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

    f32 near_dist = thread_idx == 0 ? cam_space_min_dist : cascade_splits[thread_idx - 1] * range;
    f32 far_dist = cascade_splits[thread_idx] * range;

    f32vec3 frustum_vertices[8];
    f32vec3 vertices_sum = f32vec3(0.0, 0.0, 0.0);
    for(int i = 0; i < 8; i++)
    {
        f32vec3 dir_vec = 
            deref(_globals).camera_front +
            offsets[i].x * (-deref(_globals).camera_frust_right_offset) +
            offsets[i].y * deref(_globals).camera_frust_top_offset;

        f32 multiplier = i < 4 ? near_dist : far_dist;
        frustum_vertices[i] = camera_position_world_space + dir_vec * multiplier;
        vertices_sum += frustum_vertices[i];
    }
    f32vec3 average_vertex = vertices_sum / 8;

    f32mat3x3 light_camera_rotation;
    f32vec3 camera_right = normalize(deref(_globals).camera_frust_right_offset);
    f32vec3 world_up = f32vec3(0.0, 0.0, 1.0);

    f32vec3 sun_direction = deref(_globals).sun_direction;
    f32vec3 front = -sun_direction;
    f32vec3 right = normalize(cross(front, world_up));
    f32vec3 up = normalize(cross(front, right));
    light_camera_rotation = f32mat3x3(right, up, front);

    f32mat4x4 light_view = inverse_rotation_translation(light_camera_rotation, average_vertex);

    // Calculate an AABB around the frustum corners
    const f32 max_float = 3.402823466e+38F;
    f32vec3 min_extends = f32vec3(max_float, max_float, max_float);
    f32vec3 max_extends = f32vec3(-max_float, -max_float, -max_float);

    for(i32 i = 0; i < 8; i++)
    {
        f32vec3 proj_corner = (light_view * f32vec4(frustum_vertices[i], 1.0)).xyz;
        min_extends = min(min_extends, proj_corner);
        max_extends = max(max_extends, proj_corner);
    }
    // debugPrintfEXT("min extends %f, %f, %f %d\n", min_extends.x, min_extends.y, min_extends.z, thread_idx);
    // debugPrintfEXT("max extends %f, %f, %f %d\n", max_extends.x, max_extends.y, max_extends.z, thread_idx);


    f32vec3 cascade_extends = max_extends - min_extends;
    f32vec3 shadow_camera_pos = average_vertex + sun_direction * -min_extends.z;
    // debugPrintfEXT("pos %f, %f, %f %d\n", shadow_camera_pos.x, shadow_camera_pos.y, shadow_camera_pos.z, thread_idx);

    f32mat4x4 shadow_view = inverse_rotation_translation(light_camera_rotation, shadow_camera_pos);
    f32mat4x4 shadow_proj = orthographic_projection(
        min_extends.x, min_extends.y,
        max_extends.x, max_extends.y,
        0.1, cascade_extends.z
    );

    // f32mat4x4 shadow_proj_view = shadow_proj * shadow_view;
    deref(_shadowmap_matrices[thread_idx]) = ShadowmapMatrix(shadow_view, shadow_proj, 0.0, cascade_extends.z);

    // Debug draw tightened camera frustum split into cascades
    if(deref(_globals).use_debug_camera)
    {
        const f32vec3[4] colors = f32vec3[](
            f32vec3(1.0, 0.0, 0.0),
            f32vec3(0.0, 1.0, 0.0),
            f32vec3(0.0, 0.0, 1.0),
            f32vec3(1.0, 1.0, 0.0)
        );

        if(thread_idx == 0 || thread_idx == 1)// || thread_idx == 2 || thread_idx == 3) 
        {
            // Visualize frustum cascade splits
            u32 idx = atomicAdd(deref(_frustum_indirect).instance_count, 1);
            u32 offset = 8 * idx;
            deref(_frustum_colors[idx]).color = colors[thread_idx];
            for(i32 i = 0; i < 8; i++)
            {
                deref(_frustum_vertices[i + offset]).vertex = frustum_vertices[i];
            }
        
            // Visualize orthographic camera
            f32mat4x4 inverse_shadow_proj_view = inverse(shadow_proj * shadow_view);

            idx = atomicAdd(deref(_frustum_indirect).instance_count, 1);
            offset = 8 * idx;
            deref(_frustum_colors[idx]).color = f32vec3(1.0, 1.0, 1.0);

            for(i32 i = 0; i < 8; i++)
            {
                f32vec4 ndc_pos = f32vec4(offsets[i], i < 4 ? 0.0 : 1.0, 1.0);
                f32vec4 unproj_world_space = inverse_shadow_proj_view * f32vec4(ndc_pos);
                f32vec3 world_space = unproj_world_space.xyz / unproj_world_space.w;
                deref(_frustum_vertices[i + offset]).vertex = world_space;
            }
        }
    }
}