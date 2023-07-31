#define DAXA_ENABLE_SHADER_NO_NAMESPACE 1
#define DAXA_ENABLE_IMAGE_OVERLOADS_BASIC 1
#include <shared/shared.inl>
#extension GL_EXT_debug_printf : enable

#if defined(SHADOWMAP_DRAW)
#include "tasks/shadowmap.inl"
DAXA_DECL_PUSH_CONSTANT(DrawTerrainShadowmapPC, pc)
#else
#include "tasks/draw_terrain.inl"
DAXA_DECL_PUSH_CONSTANT(DrawTerrainPC, pc)
#endif // SHADOWMAP_DRAW

// #define SHADOWMAP_DRAW
#if DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_VERTEX
void main()
{
    f32vec2 pos = deref(_vertices[gl_VertexIndex]).position;

    f32vec4 pre_scale_pos = f32vec4(deref(_vertices[gl_VertexIndex]).position, 0.0, 1.0);
    const f32 sampled_height = texture(daxa_sampler2D(_height_map, pc.linear_sampler_id), f32vec2(pre_scale_pos.xy)).r;
    const f32 adjusted_height = (sampled_height - deref(_globals).terrain_midpoint) * deref(_globals).terrain_height_scale;

    pre_scale_pos.z += adjusted_height;

    gl_Position = pre_scale_pos;
}
#elif DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_TESSELATION_CONTROL
layout (vertices = 4) out;

void main()
{
    if(gl_InvocationID == 0)
    {
        f32vec4 scaled_pos_00 = f32vec4(gl_in[0].gl_Position.xy * deref(_globals).terrain_scale, gl_in[0].gl_Position.z, 1.0);
        f32vec4 scaled_pos_01 = f32vec4(gl_in[1].gl_Position.xy * deref(_globals).terrain_scale, gl_in[1].gl_Position.z, 1.0);
        f32vec4 scaled_pos_10 = f32vec4(gl_in[2].gl_Position.xy * deref(_globals).terrain_scale, gl_in[2].gl_Position.z, 1.0);
        f32vec4 scaled_pos_11 = f32vec4(gl_in[3].gl_Position.xy * deref(_globals).terrain_scale, gl_in[3].gl_Position.z, 1.0);

#if defined(SHADOWMAP_DRAW)
        i32vec3 offset = deref(_globals).offset;
        f32mat4x4 view = deref(_globals).view;
#else
        i32vec3 offset;
        f32mat4x4 view;
        if(pc.use_secondary_camera == 1)
        {
            offset = deref(_globals).secondary_offset;
            view = deref(_globals).secondary_view;
        } else {
            offset = deref(_globals).offset;
            view = deref(_globals).view;
        }
#endif
        scaled_pos_00.xyz += offset; 
        scaled_pos_01.xyz += offset; 
        scaled_pos_10.xyz += offset; 
        scaled_pos_11.xyz += offset; 

        f32 depth_00 = (view * scaled_pos_00).z;
        f32 depth_01 = (view * scaled_pos_01).z;
        f32 depth_10 = (view * scaled_pos_10).z;
        f32 depth_11 = (view * scaled_pos_11).z;
        f32 delta =  deref(_globals).terrain_max_depth - deref(_globals).terrain_min_depth;

        f32 dist_00 = clamp(log(abs(depth_00) - deref(_globals).terrain_min_depth) / deref(_globals).terrain_delta, 0.0, 1.0);
        f32 dist_01 = clamp(log(abs(depth_01) - deref(_globals).terrain_min_depth) / deref(_globals).terrain_delta, 0.0, 1.0);
        f32 dist_10 = clamp(log(abs(depth_10) - deref(_globals).terrain_min_depth) / deref(_globals).terrain_delta, 0.0, 1.0);
        f32 dist_11 = clamp(log(abs(depth_11) - deref(_globals).terrain_min_depth) / deref(_globals).terrain_delta, 0.0, 1.0);

        // f32 dist_00 = clamp((abs(depth_00) - deref(_globals).terrain_min_depth) / delta, 0.0, 1.0);
        // f32 dist_01 = clamp((abs(depth_01) - deref(_globals).terrain_min_depth) / delta, 0.0, 1.0);
        // f32 dist_10 = clamp((abs(depth_10) - deref(_globals).terrain_min_depth) / delta, 0.0, 1.0);
        // f32 dist_11 = clamp((abs(depth_11) - deref(_globals).terrain_min_depth) / delta, 0.0, 1.0);

        gl_TessLevelOuter[0] = mix(deref(_globals).terrain_max_tess_level, deref(_globals).terrain_min_tess_level, min(dist_10, dist_00));
        gl_TessLevelOuter[1] = mix(deref(_globals).terrain_max_tess_level, deref(_globals).terrain_min_tess_level, min(dist_00, dist_01));
        gl_TessLevelOuter[2] = mix(deref(_globals).terrain_max_tess_level, deref(_globals).terrain_min_tess_level, min(dist_01, dist_11));
        gl_TessLevelOuter[3] = mix(deref(_globals).terrain_max_tess_level, deref(_globals).terrain_min_tess_level, min(dist_11, dist_10));

        gl_TessLevelInner[0] = max(gl_TessLevelOuter[0], gl_TessLevelOuter[2]);
        gl_TessLevelInner[1] = max(gl_TessLevelOuter[1], gl_TessLevelOuter[3]);
    }
    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
}
#elif DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_TESSELATION_EVALUATION
layout (quads, fractional_odd_spacing , cw) in;
// layout (quads, equal_spacing , cw) in;
layout (location = 0) out f32vec2 out_uv;
layout (location = 1) out f32vec3 world_space_pos;
layout (location = 2) out f32vec3 view_space_pos;

void main()
{
    f32 u = gl_TessCoord.x;
    f32 v = gl_TessCoord.y;

    f32vec4 p00 = gl_in[0].gl_Position;
    f32vec4 p01 = gl_in[1].gl_Position;
    f32vec4 p10 = gl_in[2].gl_Position;
    f32vec4 p11 = gl_in[3].gl_Position;

    vec4 p0 = (p01 - p00) * u + p00;
    vec4 p1 = (p11 - p10) * u + p10;

    gl_Position = (p1 - p0) * v + p0;

    out_uv = f32vec2(gl_Position.x, gl_Position.y);

    const f32 sampled_height = texture(daxa_sampler2D(_height_map, pc.linear_sampler_id), f32vec2(out_uv.xy)).r;
    const f32 adjusted_height = (sampled_height - deref(_globals).terrain_midpoint) * deref(_globals).terrain_height_scale;

    gl_Position.xy *= deref(_globals).terrain_scale;
    if(out_uv.x < 0.999 && out_uv.x > 0.001 && out_uv.y < 0.999 && out_uv.y > 0.001)
    {
        gl_Position.z = adjusted_height;
    } else {
        gl_Position.z = -1000.0;
    }

    f32vec4 pre_trans_scaled_pos = f32vec4(gl_Position.xyz, 1.0);
#if defined(SHADOWMAP_DRAW)
    // offset the position by the camera
    // pre_trans_scaled_pos.xyz -= deref(_globals).offset;
    f32mat4x4 cascade_view = deref(_cascade_data[pc.cascade_level]).cascade_view_matrix;
    f32mat4x4 cascade_proj = deref(_cascade_data[pc.cascade_level]).cascade_proj_matrix;
    f32mat4x4 projection_view = cascade_proj * cascade_view;
    view_space_pos = f32vec4(cascade_view * pre_trans_scaled_pos).xyz;
#else
    f32mat4x4 projection_view;
    if(pc.use_secondary_camera == 1)
    {
        pre_trans_scaled_pos.xyz += deref(_globals).secondary_offset;
        projection_view = deref(_globals).secondary_projection * deref(_globals).secondary_view;
    } else {
        pre_trans_scaled_pos.xyz += deref(_globals).offset;
        projection_view = deref(_globals).projection * deref(_globals).view;
    }
#endif // SHADOWMAP_DRAW
    gl_Position = projection_view * pre_trans_scaled_pos;
}

#elif DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_FRAGMENT
#if defined(SHADOWMAP_DRAW)
layout (location = 2) in f32vec3 view_space_pos;

void main()
{
    gl_FragDepth = view_space_pos.z / deref(_cascade_data[pc.cascade_level]).far_plane;
}
#else
layout (location = 0) in f32vec2 uv;
layout (location = 1) in f32vec3 world_space_pos;
layout (location = 0) out f32vec4 albedo_out;
layout (location = 1) out f32vec4 normal_out;

void main()
{
    albedo_out = texture(daxa_sampler2D(_diffuse_map, pc.linear_sampler_id), uv);
    normal_out = texture(daxa_sampler2D(_normal_map, pc.linear_sampler_id), uv);
}
#endif // SHADOWMAP_DRAW
#endif // SHADER_STAGE_FRAGMENT