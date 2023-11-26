#define DAXA_ENABLE_IMAGE_OVERLOADS_BASIC 1
#include <shared/shared.inl>
#include "tasks/deferred_pass.inl"
#define VSM_CLIP_INFO_FUNCTIONS 1
#include "vsm_common.glsl"
#include "common_func.glsl"

#extension GL_EXT_debug_printf : enable

DAXA_DECL_PUSH_CONSTANT(DeferredPassPC, pc)

layout (location = 0) in daxa_f32vec2 uv;
layout (location = 0) out daxa_f32vec4 out_color;

// const daxa_f32vec4 sun_color = daxa_f32vec4(255.0, 204.0, 153.0, 255.0)/255.0;
const daxa_f32vec4 sun_color = daxa_f32vec4(255.0, 240.0, 233.0, 255.0)/255.0; // 5800K
// const daxa_f32vec4 sun_color = daxa_f32vec4(255.0, 255.0, 255.0, 255.0)/255.0;

daxa_f32vec3 get_sun_illuminance(
    daxa_f32vec3 view_direction,
    daxa_f32 height,
    daxa_f32 zenith_cos_angle
)
{
    const daxa_f32 sun_solid_angle = 0.5 * PI / 180.0;
    const daxa_f32 min_sun_cos_theta = cos(sun_solid_angle);

    const daxa_f32vec3 sun_direction = deref(_globals).sun_direction;
    daxa_f32 cos_theta = dot(view_direction, sun_direction);

    if(cos_theta >= min_sun_cos_theta) 
    {
        TransmittanceParams transmittance_lut_params = TransmittanceParams(height, zenith_cos_angle);
        daxa_f32vec2 transmittance_texture_uv = transmittance_lut_to_uv(
            transmittance_lut_params,
            deref(_globals).atmosphere_bottom,
            deref(_globals).atmosphere_top
        );
        daxa_f32vec3 transmittance_to_sun = texture(
            daxa_sampler2D(_transmittance, pc.linear_sampler_id),
            transmittance_texture_uv
        ).rgb;
        return transmittance_to_sun * sun_color.rgb * deref(_globals).sun_brightness;
    }
    else 
    {
        return daxa_f32vec3(0.0);
    }
}


daxa_f32vec3 get_atmosphere_illuminance_along_ray(
    daxa_f32vec3 ray,
    daxa_f32vec3 world_camera_position,
    daxa_f32vec3 sun_direction,
    out bool intersects_ground
)
{
    const daxa_f32vec3 world_up = normalize(world_camera_position);

    const daxa_f32 view_zenith_angle = acos(dot(ray, world_up));
    const daxa_f32 light_view_angle = acos(dot(
        normalize(daxa_f32vec3(sun_direction.xy, 0.0)),
        normalize(daxa_f32vec3(ray.xy, 0.0))
    ));

    const daxa_f32 atmosphere_intersection_distance = ray_sphere_intersect_nearest(
        world_camera_position,
        ray,
        daxa_f32vec3(0.0, 0.0, 0.0),
        deref(_globals).atmosphere_bottom
    );

    intersects_ground = atmosphere_intersection_distance >= 0.0;
    const daxa_f32 camera_height = length(world_camera_position);

    daxa_f32vec2 skyview_uv = skyview_lut_params_to_uv(
        intersects_ground,
        SkyviewParams(view_zenith_angle, light_view_angle),
        deref(_globals).atmosphere_bottom,
        deref(_globals).atmosphere_top,
        daxa_f32vec2(deref(_globals).sky_lut_dim),
        camera_height
    );

    const daxa_f32vec3 unitless_atmosphere_illuminance = texture(daxa_sampler2D(_skyview, pc.linear_sampler_id), skyview_uv).rgb;
    const daxa_f32vec3 sun_color_weighed_atmosphere_illuminance = sun_color.rgb * unitless_atmosphere_illuminance;
    const daxa_f32vec3 atmosphere_scattering_illuminance = sun_color_weighed_atmosphere_illuminance * deref(_globals).sun_brightness;

    return atmosphere_scattering_illuminance;
}

struct AtmosphereLightingInfo
{
    // illuminance from atmosphere along normal vector
    daxa_f32vec3 atmosphere_normal_illuminance;
    // illuminance from atmosphere along view vector
    daxa_f32vec3 atmosphere_direct_illuminance;
    // direct sun illuminance
    daxa_f32vec3 sun_direct_illuminance;
};

AtmosphereLightingInfo get_atmosphere_lighting(daxa_f32vec3 view_direction, daxa_f32vec3 normal)
{
    // Because the atmosphere is using km as it's default units and we want one unit in world
    // space to be one meter we need to scale the position by a factor to get from meters -> kilometers
    const daxa_f32vec3 camera_position = deref(_globals).camera_position;
    daxa_f32vec3 world_camera_position = (camera_position - deref(_globals).offset) * UNIT_SCALE; 
    world_camera_position.z += deref(_globals).atmosphere_bottom;

    const daxa_f32vec3 sun_direction = deref(_globals).sun_direction;

    bool normal_ray_intersects_ground;
    bool view_ray_intersects_ground;
    const daxa_f32vec3 atmosphere_normal_illuminance = get_atmosphere_illuminance_along_ray(
        normal,
        world_camera_position,
        sun_direction,
        normal_ray_intersects_ground
    );
    const daxa_f32vec3 atmosphere_view_illuminance = get_atmosphere_illuminance_along_ray(
        view_direction,
        world_camera_position,
        sun_direction,
        view_ray_intersects_ground
    );

    const daxa_f32vec3 direct_sun_illuminance = view_ray_intersects_ground ? 
        daxa_f32vec3(0.0) : 
        get_sun_illuminance(
            view_direction,
            length(world_camera_position),
            dot(sun_direction, normalize(world_camera_position))
        );

    return AtmosphereLightingInfo(
        atmosphere_normal_illuminance,
        atmosphere_view_illuminance,
        direct_sun_illuminance
    );
}

// uv going in needs to be in range [-1, 1]
daxa_f32vec3 get_view_direction(daxa_f32vec2 ndc_xy)
{
    // Because the atmosphere is using km as it's default units and we want one unit in world
    // space to be one meter we need to scale the position by a factor to get from meters -> kilometers
    const daxa_f32vec3 camera_position = deref(_globals).camera_position;
    daxa_f32vec3 world_camera_position = (camera_position - deref(_globals).offset) * UNIT_SCALE; 
    world_camera_position.z += deref(_globals).atmosphere_bottom;

    // Get the direction of ray contecting camera origin and current fragment on the near plane 
    // in world coordinate system
    const daxa_f32vec4 unprojected_pos = deref(_globals).inv_view_projection * daxa_f32vec4(ndc_xy, 1.0, 1.0);
    const daxa_f32vec3 world_direction = normalize((unprojected_pos.xyz / unprojected_pos.w) - camera_position);

    return world_direction;
}

// takes in uvs in the range [0, 1]
daxa_f32vec3 get_vsm_debug_page_color(daxa_f32vec2 uv, daxa_f32 depth)
{
    daxa_f32vec3 color = daxa_f32vec3(1.0, 1.0, 1.0);

    const daxa_f32mat4x4 inv_projection_view = deref(_globals).inv_view_projection;
    const daxa_i32vec3 camera_offset = deref(_globals).offset;
    const daxa_i32 force_clip_level = deref(_globals).force_view_clip_level ? deref(_globals).vsm_debug_clip_level : -1;

    ClipInfo clip_info;
    // When we are using debug camera and no clip level is manually forced we need to
    // search through all the clip levels linearly to see if one level contains VSM entry
    // for this tile - This is a DEBUG ONLY thing the perf will probably suffer
    if(deref(_globals).use_debug_camera && !deref(_globals).force_view_clip_level)
    {
        for(daxa_i32 clip_level = 0; clip_level < VSM_CLIP_LEVELS; clip_level++)
        {
            clip_info = clip_info_from_uvs(ClipFromUVsInfo(
                uv,
                pc.offscreen_resolution,
                depth,
                inv_projection_view,
                camera_offset,
                clip_level
            ));
            const daxa_i32vec3 vsm_page_texel_coords = vsm_clip_info_to_wrapped_coords(clip_info);
            const daxa_u32 page_entry = texelFetch(daxa_utexture2DArray(_vsm_page_table), vsm_page_texel_coords, 0).r;

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

    const daxa_i32vec3 vsm_page_texel_coords = vsm_clip_info_to_wrapped_coords(clip_info);
    const daxa_u32 page_entry = texelFetch(daxa_utexture2DArray(_vsm_page_table), vsm_page_texel_coords, 0).r;

    if(get_is_allocated(page_entry))
    {
        const daxa_i32vec2 physical_page_coords = get_meta_coords_from_vsm_entry(page_entry);
        const daxa_i32vec2 physical_texel_coords = virtual_uv_to_physical_texel(clip_info.sun_depth_uv, physical_page_coords);
        const daxa_i32vec2 in_page_texel_coords = daxa_i32vec2(mod(physical_texel_coords, daxa_f32(VSM_PAGE_SIZE)));
        bool texel_near_border = any(greaterThan(in_page_texel_coords, daxa_i32vec2(126))) ||
                                 any(lessThan(in_page_texel_coords, daxa_i32vec2(2)));
        if(texel_near_border)
        {
            color = daxa_f32vec3(0.0, 0.0, 0.0);
        } else {
            color = clip_to_color[daxa_i32(mod(clip_info.clip_level,daxa_f32(NUM_CLIP_VIZ_COLORS)))];
            if(get_is_visited_marked(page_entry)) {color = daxa_f32vec3(1.0, 1.0, 0.0);}
        }
    } else {
        color = daxa_f32vec3(1.0, 0.0, 0.0);
        if(get_is_dirty(page_entry)) {color = daxa_f32vec3(0.0, 0.0, 1.0);}
    }
    return color;
}

daxa_i32 get_height_depth_offset(daxa_i32vec3 vsm_page_texel_coords)
{
    const daxa_i32 page_draw_camera_height = texelFetch(daxa_itexture2DArray(_vsm_page_height_offset), vsm_page_texel_coords, 0).r;
    const daxa_i32 current_camera_height = deref(_vsm_sun_projections[vsm_page_texel_coords.z]).camera_height_offset;
    const daxa_i32 height_difference = current_camera_height - page_draw_camera_height;
    return height_difference;
}

daxa_f32 get_vsm_shadow(daxa_f32vec2 uv, daxa_f32 depth, daxa_f32vec3 offset_world_position)
{
    const daxa_f32mat4x4 inv_projection_view = deref(_globals).inv_view_projection;
    const daxa_i32vec3 camera_offset = deref(_globals).offset;
    const daxa_i32 force_clip_level = deref(_globals).force_view_clip_level ? deref(_globals).vsm_debug_clip_level : -1;
    ClipInfo clip_info = clip_info_from_uvs(ClipFromUVsInfo(
        uv,
        pc.offscreen_resolution,
        depth,
        inv_projection_view,
        camera_offset,
        force_clip_level
    ));
    if(clip_info.clip_level >= VSM_CLIP_LEVELS) { return 1.0; }

    const daxa_i32vec3 vsm_page_texel_coords = vsm_clip_info_to_wrapped_coords(clip_info);
    const daxa_u32 page_entry = texelFetch(daxa_utexture2DArray(_vsm_page_table), vsm_page_texel_coords, 0).r;

    if(!get_is_allocated(page_entry)) { return 1.0; }

    const daxa_i32vec2 physical_page_coords = get_meta_coords_from_vsm_entry(page_entry);
    const daxa_i32vec2 physical_texel_coords = virtual_uv_to_physical_texel(clip_info.sun_depth_uv, physical_page_coords);
    const daxa_f32 vsm_sample = texelFetch(daxa_texture2D(_vsm_physical_memory), physical_texel_coords, 0).r;

    const daxa_f32mat4x4 vsm_shadow_view = deref(_vsm_sun_projections[clip_info.clip_level]).view;
    const daxa_f32mat4x4 vsm_shadow_projection = deref(_vsm_sun_projections[clip_info.clip_level]).projection;

    const daxa_i32vec3 sun_camera_offset = deref(_vsm_sun_projections[clip_info.clip_level]).offset;
    const daxa_i32vec3 main_camera_offset = deref(_globals).offset;
    const daxa_i32vec3 main_to_sun_offset = sun_camera_offset - main_camera_offset;

    const daxa_f32vec3 sun_offset_world_pos = offset_world_position + main_to_sun_offset;
    const daxa_f32vec3 view_projected_world_pos = (vsm_shadow_view * daxa_f32vec4(sun_offset_world_pos, 1.0)).xyz;

    const daxa_i32 height_offset = get_height_depth_offset(vsm_page_texel_coords);

    // avoid acne
    const daxa_f32 view_space_offset = 0.02 * pow(2, clip_info.clip_level);
    const daxa_f32 fp_remainder = fract(view_projected_world_pos.z) + view_space_offset;
    const daxa_i32 int_part = daxa_i32(floor(view_projected_world_pos.z));
    const daxa_i32 modified_view_depth = int_part + height_offset;
    
    const daxa_f32vec3 offset_view_pos = daxa_f32vec3(view_projected_world_pos.xy, daxa_f32(modified_view_depth) + fp_remainder);

    const daxa_f32vec4 vsm_projected_world = vsm_shadow_projection * daxa_f32vec4(offset_view_pos, 1.0);
    const daxa_f32 vsm_projected_depth = vsm_projected_world.z / vsm_projected_world.w;

    const daxa_f32 page_offset_projected_depth = get_page_offset_depth(clip_info, vsm_projected_depth);
    const daxa_f32 final_projected_depth = page_offset_projected_depth;
    // return good_rand(floor(1000.0 * final_projected_depth));
    // return final_projected_depth - vsm_sample;
    // const daxa_f32 depth_offset = 0.002 / pow(2, clip_info.clip_level);
    const bool is_in_shadow = vsm_sample < final_projected_depth;
    return is_in_shadow ? 0.0 : 1.0;
    // return daxa_f32(physical_texel_coords.y % VSM_PAGE_SIZE) / 128.0;
    // return good_rand(floor(10000 * page_offset_depth));
}

void main() 
{
    const daxa_f32 depth = texelFetch(daxa_texture2D(_depth), daxa_i32vec2(gl_FragCoord.xy), 0).r;

    // scale uvs to be in the range [-1, 1]
    const daxa_f32vec2 ndc_xy = (uv * 2.0) - 1.0;
    const daxa_f32vec3 view_direction = get_view_direction(ndc_xy);

    // depth == 0 means there was nothing written to the depth buffer
    //      -> there is nothing obscuring the sky so we sample the far sky texture there
    if(depth == 0.0)
    {
        AtmosphereLightingInfo atmosphere_lighting = get_atmosphere_lighting(view_direction, daxa_f32vec3(0.0, 0.0, 1.0));
        const daxa_f32vec3 total_direct_illuminance = 
            (atmosphere_lighting.atmosphere_direct_illuminance + atmosphere_lighting.sun_direct_illuminance);
        out_color = daxa_f32vec4(total_direct_illuminance, 1.0);
        return;
    } 

    // Distance of the processed fragment from the main camera in view space
    const daxa_f32vec4 unprojected_position = deref(_globals).inv_projection * daxa_f32vec4(ndc_xy, depth, 1.0);
    const daxa_f32 viewspace_distance = -unprojected_position.z / unprojected_position.w;

    // Position of the processed fragment in world space
    const daxa_f32vec4 unprojected_world_pos = deref(_globals).inv_view_projection * daxa_f32vec4(ndc_xy, depth, 1.0);
    const daxa_f32vec3 offset_world_position = unprojected_world_pos.xyz / unprojected_world_pos.w;
    const daxa_f32vec3 world_position = offset_world_position - deref(_globals).offset;

    // Get the cascade index of the current fragment
    daxa_u32 cascade_idx = 0;
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

    const daxa_f32mat4x4 shadow_view = deref(_cascade_data[cascade_idx]).cascade_view_matrix;
    const daxa_f32mat4x4 shadow_proj_view = deref(_cascade_data[cascade_idx]).cascade_proj_matrix * shadow_view;

    // Project the world position by the shadowmap camera
    const daxa_f32vec4 shadow_projected_world = shadow_proj_view * daxa_f32vec4(world_position, 1.0);
    const daxa_f32vec3 shadow_ndc_pos = shadow_projected_world.xyz / shadow_projected_world.w;
    const daxa_f32vec3 shadow_map_uv = daxa_f32vec3((shadow_ndc_pos.xy + daxa_f32vec2(1.0)) / daxa_f32vec2(2.0), daxa_f32(cascade_idx));
    const daxa_f32 distance_in_shadowmap = texture(daxa_sampler2DArray(_esm, pc.linear_sampler_id), shadow_map_uv).r;

    const daxa_f32vec4 shadow_view_world_pos = shadow_view * daxa_f32vec4(world_position, 1.0);
    const daxa_f32 shadow_reprojected_distance = shadow_view_world_pos.z / deref(_cascade_data[cascade_idx]).far_plane;

    // Equation 3 in ESM paper
    const daxa_f32 c = 80.0;
    daxa_f32 shadow = exp(-c * (shadow_reprojected_distance - distance_in_shadowmap));

    const daxa_f32 threshold = 0.2;

    // For the cases where we break the shadowmap assumption (see figure 3 in ESM paper)
    // we do manual filtering where we clamp the individual samples before blending them
    if(shadow > 1.0 + threshold)
    {
        const daxa_f32vec4 gather = textureGather(daxa_sampler2DArray(_esm, pc.linear_sampler_id), shadow_map_uv, 0);
        // clamp each sample we take individually before blending them together
        const daxa_f32vec4 shadow_gathered = clamp(
            exp(-c * (shadow_reprojected_distance - gather)),
            daxa_f32vec4(0.0, 0.0, 0.0, 0.0),
            daxa_f32vec4(1.0, 1.0, 1.0, 1.0)
        );

        // This is needed because textureGather uses a sampler which only has 8bits of precision in
        // the fractional part - this causes a slight imprecision where texture gather already 
        // collects next texel while the fract part is still on the inital texel
        //    - fract will be 0.998 while texture gather will already return the texel coresponding to 1.0
        //      (see https://www.reedbeta.com/blog/texture-gathers-and-coordinate-precision/)
        const daxa_f32 offset = 1.0/512.0;
        const daxa_f32vec2 shadow_pix_coord = shadow_map_uv.xy * pc.esm_resolution + (-0.5 + offset);
        const daxa_f32vec2 blend_factor = fract(shadow_pix_coord);

        // texel gather component mapping - (00,w);(01,x);(11,y);(10,z) const daxa_f32 tmp0 = mix(shadow_gathered.w, shadow_gathered.z, blend_factor.x);
        const daxa_f32 tmp0 = mix(shadow_gathered.w, shadow_gathered.z, blend_factor.x);
        const daxa_f32 tmp1 = mix(shadow_gathered.x, shadow_gathered.y, blend_factor.x);
        shadow = mix(tmp0, tmp1, blend_factor.y);
        // out_color = daxa_f32vec4(1.0, 0.0, 0.0, 1.0);
        // return;
    }

    const daxa_f32vec3 vsm_debug_color = get_vsm_debug_page_color(uv, depth);
    // TODO(msakmary) Improve accuracy by doing integer offset math instead of passing world pos as float
    const daxa_f32 vsm_shadow = get_vsm_shadow(uv, depth, offset_world_position);

    const daxa_f32vec4 albedo = texture(daxa_sampler2D(_g_albedo, pc.linear_sampler_id), uv);
    out_color = daxa_f32vec4(pow(albedo.xyz, daxa_f32vec3(1.7)) * 0.1, 1.0);

    const daxa_f32vec3 normal = texture(daxa_sampler2D(_g_normals, pc.linear_sampler_id), uv).xyz;
    const daxa_f32vec3 sun_direction = deref(_globals).sun_direction;
    const daxa_f32 sun_norm_dot = clamp(dot(normal, deref(_globals).sun_direction), 0.0, 1.0);

    AtmosphereLightingInfo atmosphere_lighting = get_atmosphere_lighting(sun_direction, normal);
    const daxa_f32vec3 view_ray_atmosphere_illuminnace = atmosphere_lighting.atmosphere_direct_illuminance;
    const daxa_f32vec3 normal_atmosphere_illuminance = atmosphere_lighting.atmosphere_normal_illuminance;
    const daxa_f32vec3 sun_illuminance = atmosphere_lighting.sun_direct_illuminance;

    const daxa_f32 final_shadow = sun_norm_dot * vsm_shadow;
    // ESM SHADOWS
    // const daxa_f32 final_shadow = sun_norm_dot * clamp(pow(shadow, 2), 0.0, 1.0);

    out_color.rgb *= final_shadow * (view_ray_atmosphere_illuminnace + sun_illuminance) + normal_atmosphere_illuminance;
    // out_color = vsm_shadow > 0.0 ? daxa_f32vec4(0.0, 0.0, vsm_shadow, 1.0) : daxa_f32vec4(-vsm_shadow, 0.0, 0.0, 1.0);
    // out_color = daxa_f32vec4(vsm_shadow, vsm_shadow, vsm_shadow, 1.0);
    // out_color.xyz = daxa_f32vec3(fract(world_position));
    // out_color.xyz *= vsm_debug_color;
}