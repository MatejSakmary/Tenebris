#define DAXA_ENABLE_SHADER_NO_NAMESPACE 1
#define DAXA_ENABLE_IMAGE_OVERLOADS_BASIC 1
#include <shared/shared.inl>
#include "tasks/deferred_pass.inl"
#define VSM_CLIP_INFO_FUNCTIONS 1
#include "vsm_common.glsl"
#include "common_func.glsl"

#extension GL_EXT_debug_printf : enable

DAXA_DECL_PUSH_CONSTANT(DeferredPassPC, pc)

layout (location = 0) in f32vec2 uv;
layout (location = 0) out f32vec4 out_color;

const f32vec4 sun_color = f32vec4(255.0, 204.0, 153.0, 255.0)/255.0;
// const f32vec4 sun_color = f32vec4(255.0, 255.0, 255.0, 255.0)/255.0;

f32vec3 add_sun_circle(f32vec3 world_dir, f32vec3 sun_dir, f32vec3 sky_color)
{
    const f32 sun_solid_angle = 0.5 * PI / 180.0;
    const f32 min_sun_cos_theta = cos(sun_solid_angle);

    f32 cos_theta = dot(world_dir, sun_dir);
    if(cos_theta >= min_sun_cos_theta) {return sky_color;}
    else {return f32vec3(0.0);}
}

f32vec3 get_far_sky_color(f32vec3 world_direction)
{
    // Because the atmosphere is using km as it's default units and we want one unit in world
    // space to be one meter we need to scale the position by a factor to get from meters -> kilometers
    const f32vec3 camera_position = deref(_globals).camera_position;
    f32vec3 world_camera_position = (camera_position - deref(_globals).offset) * UNIT_SCALE; 
    world_camera_position.z += deref(_globals).atmosphere_bottom;

    const f32vec3 world_up = normalize(world_camera_position);

    const f32vec3 sun_direction = deref(_globals).sun_direction;
    const f32 view_zenith_angle = acos(dot(world_direction, world_up));
    const f32 light_view_angle = acos(dot(
        normalize(f32vec3(sun_direction.xy, 0.0)),
        normalize(f32vec3(world_direction.xy, 0.0))
    ));

    const f32 atmosphere_intersection_distance = ray_sphere_intersect_nearest(
        world_camera_position,
        world_direction,
        f32vec3(0.0, 0.0, 0.0),
        deref(_globals).atmosphere_bottom
    );

    const bool intersects_ground = atmosphere_intersection_distance >= 0.0;
    const f32 camera_height = length(world_camera_position);

    f32vec2 skyview_uv = skyview_lut_params_to_uv(
        intersects_ground,
        SkyviewParams(view_zenith_angle, light_view_angle),
        deref(_globals).atmosphere_bottom,
        deref(_globals).atmosphere_top,
        f32vec2(deref(_globals).sky_lut_dim),
        camera_height
    );

    f32vec3 sky_color = texture(daxa_sampler2D(_skyview, pc.linear_sampler_id), skyview_uv).rgb;
    if(!intersects_ground) { sky_color += add_sun_circle(world_direction, sun_direction, sky_color); };

    return sky_color;
}

// uv going in needs to be in range [-1, 1]
f32vec3 get_far_sky_color(f32vec2 uv)
{
    // Because the atmosphere is using km as it's default units and we want one unit in world
    // space to be one meter we need to scale the position by a factor to get from meters -> kilometers
    const f32vec3 camera_position = deref(_globals).camera_position;
    f32vec3 world_camera_position = (camera_position - deref(_globals).offset) * UNIT_SCALE; 
    world_camera_position.z += deref(_globals).atmosphere_bottom;

    // Get the direction of ray contecting camera origin and current fragment on the near plane 
    // in world coordinate system
    const f32vec4 unprojected_pos = deref(_globals).inv_view_projection * f32vec4(uv, 1.0, 1.0);
    const f32vec3 world_direction = normalize((unprojected_pos.xyz / unprojected_pos.w) - camera_position);

    return get_far_sky_color(world_direction);
}

// takes in uvs in the range [0, 1]
f32vec3 get_vsm_debug_page_color(f32vec2 uv, f32 depth)
{
    f32vec3 color = f32vec3(1.0, 1.0, 1.0);

    const f32mat4x4 inv_projection_view = deref(_globals).inv_view_projection;
    const i32vec3 camera_offset = deref(_globals).offset;
    const i32 force_clip_level = deref(_globals).force_view_clip_level ? deref(_globals).vsm_debug_clip_level : -1;

    ClipInfo clip_info;
    // When we are using debug camera and no clip level is manually forced we need to
    // search through all the clip levels linearly to see if one level contains VSM entry
    // for this tile - This is a DEBUG ONLY thing the perf will probably suffer
    if(deref(_globals).use_debug_camera && !deref(_globals).force_view_clip_level)
    {
        for(i32 clip_level = 0; clip_level < VSM_CLIP_LEVELS; clip_level++)
        {
            clip_info = clip_info_from_uvs(ClipFromUVsInfo(
                uv,
                pc.offscreen_resolution,
                depth,
                inv_projection_view,
                camera_offset,
                clip_level
            ));
            const i32vec3 vsm_page_texel_coords = vsm_clip_info_to_wrapped_coords(clip_info);
            const u32 page_entry = texelFetch(daxa_utexture2DArray(_vsm_page_table), vsm_page_texel_coords, 0).r;

            if(get_is_visited_marked(page_entry)) {break;}
        }
    }
    else 
    {
        clip_info = clip_info_from_uvs(ClipFromUVsInfo(
            uv,
            pc.offscreen_resolution,
            depth,
            inv_projection_view,
            camera_offset,
            force_clip_level
        ));
    }
    if(clip_info.clip_level >= VSM_CLIP_LEVELS) { return color; }

    const i32vec3 vsm_page_texel_coords = vsm_clip_info_to_wrapped_coords(clip_info);
    const u32 page_entry = texelFetch(daxa_utexture2DArray(_vsm_page_table), vsm_page_texel_coords, 0).r;

    if(get_is_allocated(page_entry))
    {
        const i32vec2 physical_page_coords = get_meta_coords_from_vsm_entry(page_entry);
        const i32vec2 physical_texel_coords = virtual_uv_to_physical_texel(clip_info.sun_depth_uv, physical_page_coords);
        const i32vec2 in_page_texel_coords = i32vec2(mod(physical_texel_coords, f32(VSM_PAGE_SIZE)));
        bool texel_near_border = any(greaterThan(in_page_texel_coords, i32vec2(126))) ||
                                 any(lessThan(in_page_texel_coords, i32vec2(2)));
        if(texel_near_border)
        {
            color = f32vec3(0.0, 0.0, 0.0);
        } else {
            color = clip_to_color[i32(mod(clip_info.clip_level,f32(NUM_CLIP_VIZ_COLORS)))];
            if(get_is_visited_marked(page_entry)) {color = f32vec3(1.0, 1.0, 0.0);}
        }
    } else {
        color = f32vec3(1.0, 0.0, 0.0);
        if(get_is_dirty(page_entry)) {color = f32vec3(0.0, 0.0, 1.0);}
    }
    return color;
}

i32 get_height_depth_offset(i32vec3 vsm_page_texel_coords)
{
    const i32 page_draw_camera_height = texelFetch(daxa_itexture2DArray(_vsm_page_height_offset), vsm_page_texel_coords, 0).r;
    const i32 current_camera_height = deref(_vsm_sun_projections[vsm_page_texel_coords.z]).camera_height_offset;
    const i32 height_difference = current_camera_height - page_draw_camera_height;
    return height_difference;
}

u32 good_rand_hash(u32 x) {
    x += (x << 10u);
    x ^= (x >> 6u);
    x += (x << 3u);
    x ^= (x >> 11u);
    x += (x << 15u);
    return x;
}
f32 good_rand_float_construct(u32 m) {
    const u32 ieee_mantissa = 0x007FFFFFu;
    const u32 ieee_one = 0x3F800000u;
    m &= ieee_mantissa;
    m |= ieee_one;
    f32 f = uintBitsToFloat(m);
    return f - 1.0;
}
f32 good_rand(f32 x) { return good_rand_float_construct(good_rand_hash(floatBitsToUint(x))); }

f32 get_vsm_shadow(f32vec2 uv, f32 depth, f32vec3 offset_world_position)
{
    const f32mat4x4 inv_projection_view = deref(_globals).inv_view_projection;
    const i32vec3 camera_offset = deref(_globals).offset;
    const i32 force_clip_level = deref(_globals).force_view_clip_level ? deref(_globals).vsm_debug_clip_level : -1;
    ClipInfo clip_info = clip_info_from_uvs(ClipFromUVsInfo(
        uv,
        pc.offscreen_resolution,
        depth,
        inv_projection_view,
        camera_offset,
        force_clip_level
    ));
    if(clip_info.clip_level >= VSM_CLIP_LEVELS) { return 1.0; }

    const i32vec3 vsm_page_texel_coords = vsm_clip_info_to_wrapped_coords(clip_info);
    const u32 page_entry = texelFetch(daxa_utexture2DArray(_vsm_page_table), vsm_page_texel_coords, 0).r;

    if(!get_is_allocated(page_entry)) { return 1.0; }

    const i32vec2 physical_page_coords = get_meta_coords_from_vsm_entry(page_entry);
    const i32vec2 physical_texel_coords = virtual_uv_to_physical_texel(clip_info.sun_depth_uv, physical_page_coords);
    const f32 vsm_sample = texelFetch(daxa_texture2D(_vsm_physical_memory), physical_texel_coords, 0).r;

    const f32mat4x4 vsm_shadow_view = deref(_vsm_sun_projections[clip_info.clip_level]).view;
    const f32mat4x4 vsm_shadow_projection = deref(_vsm_sun_projections[clip_info.clip_level]).projection;

    const i32vec3 sun_camera_offset = deref(_vsm_sun_projections[clip_info.clip_level]).offset;
    const i32vec3 main_camera_offset = deref(_globals).offset;
    const i32vec3 main_to_sun_offset = sun_camera_offset - main_camera_offset;

    const f32vec3 sun_offset_world_pos = offset_world_position + main_to_sun_offset;
    const f32vec3 view_projected_world_pos = (vsm_shadow_view * f32vec4(sun_offset_world_pos, 1.0)).xyz;

    const i32 height_offset = get_height_depth_offset(vsm_page_texel_coords);

    const f32 fp_remainder = fract(view_projected_world_pos.z);
    const i32 int_part = i32(floor(view_projected_world_pos.z));
    const i32 modified_view_depth = int_part + height_offset;
    
    const f32vec3 offset_view_pos = f32vec3(view_projected_world_pos.xy, f32(modified_view_depth) + fp_remainder);

    const f32vec4 vsm_projected_world = vsm_shadow_projection * f32vec4(offset_view_pos, 1.0);
    const f32 vsm_projected_depth = vsm_projected_world.z / vsm_projected_world.w;

    const f32 page_offset_projected_depth = get_page_offset_depth(clip_info, vsm_projected_depth);
    const f32 final_projected_depth = page_offset_projected_depth;
    // return good_rand(floor(1000.0 * final_projected_depth));
    // return final_projected_depth - vsm_sample;
    const bool is_in_shadow = vsm_sample < (final_projected_depth);
    return is_in_shadow ? 0.0 : 1.0;
    // return f32(physical_texel_coords.y % VSM_PAGE_SIZE) / 128.0;
    // return good_rand(floor(10000 * page_offset_depth));
}


void main() 
{
    // const f32 depth = texture(daxa_sampler2D(_depth, pc.nearest_sampler_id), uv).r;
    const f32 depth = texelFetch(daxa_texture2D(_depth), i32vec2(gl_FragCoord.xy), 0).r;

    // scale uvs to be in the range [-1, 1]
    const f32vec2 remap_uv = (uv * 2.0) - 1.0;

    // depth == 0 means there was nothing written to the depth buffer
    //      -> there is nothing obscuring the sky so we sample the far sky texture there
    if(depth == 0.0)
    {
        out_color = f32vec4(get_far_sky_color(remap_uv), 1.0) * deref(_globals).sun_brightness * sun_color;
        return;
    } 

    // Distance of the processed fragment from the main camera in view space
    const f32vec4 unprojected_position = deref(_globals).inv_projection * f32vec4(remap_uv, depth, 1.0);
    const f32 viewspace_distance = -unprojected_position.z / unprojected_position.w;

    // Position of the processed fragment in world space
    const f32vec4 unprojected_world_pos = deref(_globals).inv_view_projection * f32vec4(remap_uv, depth, 1.0);
    const f32vec3 offset_world_position = unprojected_world_pos.xyz / unprojected_world_pos.w;
    const f32vec3 world_position = offset_world_position - deref(_globals).offset;

    // Get the cascade index of the current fragment
    u32 cascade_idx = 0;
    for(cascade_idx; cascade_idx < NUM_CASCADES; cascade_idx++)
    {
        if(viewspace_distance < deref(_cascade_data[cascade_idx]).cascade_far_depth)
        {
            break;
        }
    }
    // Due to accuracy issues when reprojecting some samples may think
    // they are behind the last far depth - clip these to still be in the last cascade
    // to avoid out of bounds indexing
    cascade_idx = min(cascade_idx, 3);

    const f32mat4x4 shadow_view = deref(_cascade_data[cascade_idx]).cascade_view_matrix;
    const f32mat4x4 shadow_proj_view = deref(_cascade_data[cascade_idx]).cascade_proj_matrix * shadow_view;

    // Project the world position by the shadowmap camera
    const f32vec4 shadow_projected_world = shadow_proj_view * f32vec4(world_position, 1.0);
    const f32vec3 shadow_ndc_pos = shadow_projected_world.xyz / shadow_projected_world.w;
    const f32vec3 shadow_map_uv = f32vec3((shadow_ndc_pos.xy + f32vec2(1.0)) / f32vec2(2.0), f32(cascade_idx));
    const f32 distance_in_shadowmap = texture(daxa_sampler2DArray(_esm, pc.linear_sampler_id), shadow_map_uv).r;

    const f32vec4 shadow_view_world_pos = shadow_view * f32vec4(world_position, 1.0);
    const f32 shadow_reprojected_distance = shadow_view_world_pos.z / deref(_cascade_data[cascade_idx]).far_plane;

    // Equation 3 in ESM paper
    const f32 c = 80.0;
    f32 shadow = exp(-c * (shadow_reprojected_distance - distance_in_shadowmap));

    const f32 threshold = 0.2;

    // For the cases where we break the shadowmap assumption (see figure 3 in ESM paper)
    // we do manual filtering where we clamp the individual samples before blending them
    if(shadow > 1.0 + threshold)
    {
        const f32vec4 gather = textureGather(daxa_sampler2DArray(_esm, pc.linear_sampler_id), shadow_map_uv, 0);
        // clamp each sample we take individually before blending them together
        const f32vec4 shadow_gathered = clamp(
            exp(-c * (shadow_reprojected_distance - gather)),
            f32vec4(0.0, 0.0, 0.0, 0.0),
            f32vec4(1.0, 1.0, 1.0, 1.0)
        );

        // This is needed because textureGather uses a sampler which only has 8bits of precision in
        // the fractional part - this causes a slight imprecision where texture gather already 
        // collects next texel while the fract part is still on the inital texel
        //    - fract will be 0.998 while texture gather will already return the texel coresponding to 1.0
        //      (see https://www.reedbeta.com/blog/texture-gathers-and-coordinate-precision/)
        const f32 offset = 1.0/512.0;
        const f32vec2 shadow_pix_coord = shadow_map_uv.xy * pc.esm_resolution + (-0.5 + offset);
        const f32vec2 blend_factor = fract(shadow_pix_coord);

        // texel gather component mapping - (00,w);(01,x);(11,y);(10,z) 
        const f32 tmp0 = mix(shadow_gathered.w, shadow_gathered.z, blend_factor.x);
        const f32 tmp1 = mix(shadow_gathered.x, shadow_gathered.y, blend_factor.x);
        shadow = mix(tmp0, tmp1, blend_factor.y);
        // out_color = f32vec4(1.0, 0.0, 0.0, 1.0);
        // return;
    }

    const f32vec3 vsm_debug_color = get_vsm_debug_page_color(uv, depth);
    // TODO(msakmary) Improve accuracy by doing integer offset math instead of passing
    // world pos as float
    const f32 vsm_shadow = get_vsm_shadow(uv, depth, offset_world_position);

    const f32vec4 albedo = texture(daxa_sampler2D(_g_albedo, pc.nearest_sampler_id), uv);
    const f32vec3 normal = texture(daxa_sampler2D(_g_normals, pc.nearest_sampler_id), uv).xyz;

    const f32vec3 sun_direction = deref(_globals).sun_direction;
    f32vec3 transmittance_to_sun = get_far_sky_color(sun_direction);

    const f32 sun_norm_dot = dot(normal, deref(_globals).sun_direction);
    out_color = pow(albedo, f32vec4(f32vec3(2.4), 1.0));
    f32vec4 ambient = f32vec4(deref(_globals).sun_brightness * sun_color.xyz * get_far_sky_color(normal) * 0.5, 1.0); 

    out_color *= clamp(sun_norm_dot, 0.0, 1.0) * 
                vsm_shadow * 
                //  clamp(pow(shadow, 2), 0.0, 1.0) *
                 f32vec4(transmittance_to_sun, 1.0) *
                 deref(_globals).sun_brightness * sun_color + ambient;
    // out_color = vsm_shadow > 0.0 ? f32vec4(0.0, 0.0, vsm_shadow, 1.0) : f32vec4(-vsm_shadow, 0.0, 0.0, 1.0);
    // out_color = f32vec4(vsm_shadow, vsm_shadow, vsm_shadow, 1.0);
    // out_color.xyz = f32vec3(fract(world_position));
    // out_color.xyz *= vsm_debug_color;
}