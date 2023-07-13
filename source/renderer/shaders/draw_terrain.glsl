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

    f32vec4 pre_scale_pos = f32vec4(deref(_vertices[gl_VertexIndex]).position, deref(_globals).atmosphere_bottom, 1.0);
    const f32 sampled_height = texture(daxa_sampler2D(_height_map, pc.sampler_id), f32vec2(pre_scale_pos.xy)).r;
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
        f32 depth_00 = (deref(_globals).view * scaled_pos_00).z;
        f32 depth_01 = (deref(_globals).view * scaled_pos_01).z;
        f32 depth_10 = (deref(_globals).view * scaled_pos_10).z;
        f32 depth_11 = (deref(_globals).view * scaled_pos_11).z;
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

    const f32 sampled_height = texture(daxa_sampler2D(_height_map, pc.sampler_id), f32vec2(out_uv.xy)).r;
    const f32 adjusted_height = (sampled_height - deref(_globals).terrain_midpoint) * deref(_globals).terrain_height_scale;

    gl_Position.xy *= deref(_globals).terrain_scale;
    gl_Position.z = deref(_globals).atmosphere_bottom + adjusted_height;
    world_space_pos = gl_Position.xyz;

    const f32vec4 pre_trans_scaled_pos = f32vec4(gl_Position.xyz, 1.0);

#if defined(SHADOWMAP_DRAW)
    f32mat4x4 m_proj_view_model = deref(_globals).shadowmap_projection * deref(_globals).shadowmap_view;
#else
    // f32mat4x4 m_proj_view_model = deref(_globals).shadowmap_projection * deref(_globals).shadowmap_view;
    f32mat4x4 m_proj_view_model = deref(_globals).projection * deref(_globals).view;
#endif // SHADOWMAP_DRAW
    gl_Position = m_proj_view_model * pre_trans_scaled_pos;
}

#elif DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_FRAGMENT
#if defined(SHADOWMAP_DRAW)
void main()
{
}
#else
layout (location = 0) in f32vec2 uv;
layout (location = 1) in f32vec3 world_space_pos;
layout (location = 0) out f32vec4 out_color;

void main()
{
    out_color = texture(daxa_sampler2D(_diffuse_map, pc.sampler_id), uv);
    const f32vec4 shadow_proj_world_pos = deref(_globals).shadowmap_projection * deref(_globals).shadowmap_view * f32vec4(world_space_pos, 1.0);
    const f32vec3 ndc_pos = shadow_proj_world_pos.xyz / shadow_proj_world_pos.w;
    const f32vec2 shadow_map_uv = (ndc_pos.xy + f32vec2(1.0)) / f32vec2(2.0);

    const f32 shadow_dist = texture(daxa_sampler2D(_shadowmap, pc.sampler_id), shadow_map_uv).r;
    if(ndc_pos.z > shadow_dist)
    {
        out_color *= 0.1f;
    }
    // out_color = f32vec4(shadow_dist,shadow_dist,shadow_dist, 1.0 );
}
#endif // SHADOWMAP_DRAW
#endif // SHADER_STAGE_FRAGMENT