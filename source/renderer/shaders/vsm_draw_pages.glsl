#define DAXA_ENABLE_IMAGE_OVERLOADS_BASIC 1
#include <shared/shared.inl>
#include "tasks/vsm_draw_pages.inl"
#define VSM_CLIP_INFO_FUNCTIONS 1
#include "vsm_common.glsl"

#extension GL_EXT_debug_printf : enable

DAXA_DECL_PUSH_CONSTANT(VSMDrawPagesPC, pc)

#if DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_VERTEX
layout(location = 0) out flat daxa_u32 out_clip_level;
void main()
{
    // xy ranging from [0,1]
    const daxa_f32vec2 pre_scale_position = daxa_f32vec2(deref(_vertices[gl_VertexIndex]).position);

    const daxa_f32 sampled_height = texture(daxa_sampler2D(_height_map, pc.llb_sampler), daxa_f32vec2(pre_scale_position.xy)).r;
    const daxa_f32 adjusted_height = (sampled_height - deref(_globals).terrain_midpoint) * deref(_globals).terrain_height_scale;

    gl_Position = daxa_f32vec4(pre_scale_position.xy, adjusted_height, 1.0);
    out_clip_level = gl_InstanceIndex;
}
#elif DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_TESSELATION_CONTROL
layout(location = 0) in daxa_u32 in_clip_level [];

layout(vertices = 4) out;
layout(location = 0) patch out daxa_u32 out_clip_level;

void main()
{
    if(gl_InvocationID == 0)
    {
        daxa_f32vec4 scaled_pos_00 = daxa_f32vec4(gl_in[0].gl_Position.xy * deref(_globals).terrain_scale, gl_in[0].gl_Position.z, 1.0);
        daxa_f32vec4 scaled_pos_01 = daxa_f32vec4(gl_in[1].gl_Position.xy * deref(_globals).terrain_scale, gl_in[1].gl_Position.z, 1.0);
        daxa_f32vec4 scaled_pos_10 = daxa_f32vec4(gl_in[2].gl_Position.xy * deref(_globals).terrain_scale, gl_in[2].gl_Position.z, 1.0);
        daxa_f32vec4 scaled_pos_11 = daxa_f32vec4(gl_in[3].gl_Position.xy * deref(_globals).terrain_scale, gl_in[3].gl_Position.z, 1.0);

        daxa_i32vec3 offset;
        daxa_f32mat4x4 view;

        if(deref(_globals).use_debug_camera)
        {
            offset = deref(_globals).secondary_offset;
            view = deref(_globals).secondary_view;
        } else {
            offset = deref(_globals).offset;
            view = deref(_globals).view;
        }

        scaled_pos_00.xyz += offset; 
        scaled_pos_01.xyz += offset; 
        scaled_pos_10.xyz += offset; 
        scaled_pos_11.xyz += offset; 

        daxa_f32 depth_00 = (view * scaled_pos_00).z;
        daxa_f32 depth_01 = (view * scaled_pos_01).z;
        daxa_f32 depth_10 = (view * scaled_pos_10).z;
        daxa_f32 depth_11 = (view * scaled_pos_11).z;
        daxa_f32 delta =  deref(_globals).terrain_max_depth - deref(_globals).terrain_min_depth;

        daxa_f32 dist_00 = clamp(log(abs(depth_00) - deref(_globals).terrain_min_depth) / deref(_globals).terrain_delta, 0.0, 1.0);
        daxa_f32 dist_01 = clamp(log(abs(depth_01) - deref(_globals).terrain_min_depth) / deref(_globals).terrain_delta, 0.0, 1.0);
        daxa_f32 dist_10 = clamp(log(abs(depth_10) - deref(_globals).terrain_min_depth) / deref(_globals).terrain_delta, 0.0, 1.0);
        daxa_f32 dist_11 = clamp(log(abs(depth_11) - deref(_globals).terrain_min_depth) / deref(_globals).terrain_delta, 0.0, 1.0);

        gl_TessLevelOuter[0] = mix(deref(_globals).terrain_max_tess_level, deref(_globals).terrain_min_tess_level, min(dist_10, dist_00));
        gl_TessLevelOuter[1] = mix(deref(_globals).terrain_max_tess_level, deref(_globals).terrain_min_tess_level, min(dist_00, dist_01));
        gl_TessLevelOuter[2] = mix(deref(_globals).terrain_max_tess_level, deref(_globals).terrain_min_tess_level, min(dist_01, dist_11));
        gl_TessLevelOuter[3] = mix(deref(_globals).terrain_max_tess_level, deref(_globals).terrain_min_tess_level, min(dist_11, dist_10));

        gl_TessLevelInner[0] = max(gl_TessLevelOuter[0], gl_TessLevelOuter[2]);
        gl_TessLevelInner[1] = max(gl_TessLevelOuter[1], gl_TessLevelOuter[3]);

        // gl_TessLevelOuter[0] = (in_clip_level[gl_InvocationID] + 1) * 2;
        // gl_TessLevelOuter[1] = (in_clip_level[gl_InvocationID] + 1) * 2;
        // gl_TessLevelOuter[2] = (in_clip_level[gl_InvocationID] + 1) * 2;
        // gl_TessLevelOuter[3] = (in_clip_level[gl_InvocationID] + 1) * 2;

        // gl_TessLevelInner[0] = (in_clip_level[gl_InvocationID] + 1) * 2;
        // gl_TessLevelInner[1] = (in_clip_level[gl_InvocationID] + 1) * 2;

    }
    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
    out_clip_level = in_clip_level[gl_InvocationID];
}

#elif DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_TESSELATION_EVALUATION
layout (quads, fractional_odd_spacing , cw) in;

layout (location = 0) patch in daxa_u32 in_clip_level;
layout (location = 0) out flat daxa_u32 out_clip_level;

void main()
{
    daxa_f32 u = gl_TessCoord.x;
    daxa_f32 v = gl_TessCoord.y;

    daxa_f32vec4 p00 = gl_in[0].gl_Position;
    daxa_f32vec4 p01 = gl_in[1].gl_Position;
    daxa_f32vec4 p10 = gl_in[2].gl_Position;
    daxa_f32vec4 p11 = gl_in[3].gl_Position;

    vec4 p0 = (p01 - p00) * u + p00;
    vec4 p1 = (p11 - p10) * u + p10;

    const daxa_f32vec4 tesselated_position = (p1 - p0) * v + p0;
    const daxa_f32vec2 uv = daxa_f32vec2(tesselated_position.x, tesselated_position.y);

    const daxa_f32 sampled_height = texture(daxa_sampler2D(_height_map, pc.llb_sampler), daxa_f32vec2(uv)).r;
    const daxa_f32 adjusted_height = (sampled_height - deref(_globals).terrain_midpoint) * deref(_globals).terrain_height_scale;

    const daxa_f32vec3 scaled_position = daxa_f32vec3(tesselated_position.xy * deref(_globals).terrain_scale, tesselated_position.z);

    // Clamp the borders of the terrain so that the sun does not see under it when drawing at shallow angles
    const bool is_on_boundary = uv.x > 0.999 || uv.x < 0.001 || uv.y > 0.999 || uv.y < 0.001;
    const daxa_f32 correct_height = is_on_boundary ? -100000.0 : adjusted_height;

    const daxa_f32vec3 height_correct_position = daxa_f32vec3(scaled_position.xy, correct_height);
    // offset the terrain position by the current clip camera world offset
    const daxa_f32vec3 clip_offset_position = height_correct_position + deref(_vsm_sun_projections[in_clip_level]).offset;
    const daxa_f32mat4x4 projection_view = deref(_vsm_sun_projections[in_clip_level]).projection_view;

    gl_Position = projection_view * daxa_f32vec4(clip_offset_position, 1.0);
    out_clip_level = in_clip_level;
}
#elif DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_FRAGMENT
DAXA_DECL_IMAGE_ACCESSOR_WITH_FORMAT(uimage2D, r32ui, , r32uiImage)

layout (location = 0) in flat daxa_u32 in_clip_level;
layout (location = 0) out daxa_f32vec4 albedo_out;
void main()
{
    const daxa_f32vec2 virtual_uv = gl_FragCoord.xy / VSM_TEXTURE_RESOLUTION;
    ClipInfo info = ClipInfo(daxa_i32(in_clip_level), virtual_uv);


    const daxa_i32vec3 wrapped_coords = vsm_clip_info_to_wrapped_coords(info);
    const daxa_u32 vsm_page_entry = imageLoad(daxa_uimage2DArray(_vsm_page_table), wrapped_coords).r;

    if(get_is_allocated(vsm_page_entry) && get_is_dirty(vsm_page_entry))
    {
        const daxa_i32vec2 memory_page_coords = get_meta_coords_from_vsm_entry(vsm_page_entry);
        const daxa_i32vec2 physical_texel_coords = virtual_uv_to_physical_texel(virtual_uv, memory_page_coords);

        imageAtomicMin(
            daxa_access(r32uiImage, pc.daxa_u32_vsm_memory_view),
            physical_texel_coords,
            floatBitsToUint(get_page_offset_depth(info, gl_FragCoord.z))
        );
    } 
}

#endif // DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_FRAGMENT