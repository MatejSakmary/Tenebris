#define DAXA_ENABLE_SHADER_NO_NAMESPACE 1
#define DAXA_ENABLE_IMAGE_OVERLOADS_BASIC 1
#include <shared/shared.inl>
#include "common_func.glsl"
#include "tasks/deferred_pass.inl"

#extension GL_EXT_debug_printf : enable

DAXA_DECL_PUSH_CONSTANT(DeferredPassPC, pc)

layout (location = 0) in f32vec2 uv;
layout (location = 0) out f32vec4 out_color;

f32vec3 add_sun_circle(f32vec3 world_dir, f32vec3 sun_dir)
{
    const f32 sun_solid_angle = 0.5 * PI / 180.0;
    const f32 min_sun_cos_theta = cos(sun_solid_angle);

    f32 cos_theta = dot(world_dir, sun_dir);
    if(cos_theta >= min_sun_cos_theta) {return f32vec3(1.0);}
    else {return f32vec3(0.0);}
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

    const f32vec3 world_up = normalize(world_camera_position);

    const f32vec3 sun_direction = deref(_globals).sun_direction;
    const f32 view_zenith_angle = acos(dot(world_direction, world_up));
    const f32 light_view_angle = acos(dot(world_direction, sun_direction));

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
    if(!intersects_ground) { sky_color += add_sun_circle(world_direction, sun_direction); };

    return sky_color;
}

void main() 
{
    const f32 depth = texture(daxa_sampler2D(_depth, pc.nearest_sampler_id), uv).r;

    // scale uvs to be in the range [-1, 1]
    const f32vec2 remap_uv = (uv * 2.0) - 1.0;

    // depth == 0 means there was nothing written to the depth buffer
    //      -> there is nothing obscuring the sky so we sample the far sky texture there
    if(depth == 0.0)
    {
        out_color = f32vec4(get_far_sky_color(remap_uv), 1.0);
        return;
    } 

    // Distance of the processed fragment from the main camera in view space
    const f32vec4 unprojected_position = deref(_globals).inv_projection * f32vec4(remap_uv, depth, 1.0);
    const f32 viewspace_distance = -unprojected_position.z / unprojected_position.w;

    // Position of the processed fragment in world space
    f32vec4 h_pos = deref(_globals).inv_view_projection * f32vec4(remap_uv, depth, 1.0);
    f32vec3 world_pos = h_pos.xyz / h_pos.w;
    world_pos -= deref(_globals).offset;

    // Get the cascade index of the current fragment
    u32 cascade_idx = 0;
    for(cascade_idx; cascade_idx < NUM_CASCADES; cascade_idx++)
    {
        if(viewspace_distance < deref(_cascade_data[cascade_idx]).cascade_far_depth)
        {
            break;
        }
    }

    f32mat4x4 shadow_proj_view = deref(_cascade_data[cascade_idx]).cascade_proj_matrix * deref(_cascade_data[cascade_idx]).cascade_view_matrix;

    const f32vec4 shadow_proj_world_pos = shadow_proj_view * f32vec4(world_pos, 1.0);

    const f32vec3 ndc_pos = shadow_proj_world_pos.xyz / shadow_proj_world_pos.w;
    const f32vec3 shadow_map_uv = f32vec3((ndc_pos.xy + f32vec2(1.0)) / f32vec2(2.0), f32(cascade_idx));
    const f32 shadowmap_dist = texture(daxa_sampler2DArray(_esm, pc.linear_sampler_id), shadow_map_uv).r;

    const f32 real_dist = ndc_pos.z;

    const f32 c = 80.0;
    f32 shadow = exp(-c * real_dist) * shadowmap_dist;

    const f32 threshold = 0.02;

    if(shadow > 1.0 + threshold)
    {
        f32vec4 gather = textureGather(daxa_sampler2DArray(_esm, pc.linear_sampler_id), shadow_map_uv, 0);
        f32vec4 shadow_gathered = clamp(exp(-c * real_dist) * gather, f32vec4(0.0, 0.0, 0.0, 0.0), f32vec4(1.0, 1.0, 1.0, 1.0));

        // This is needed because textureGather and fract are slightly imprecise so 
        const f32 offset = 1.0/512.0;
        f32vec2 shadow_pix_coord = shadow_map_uv.xy * pc.esm_resolution + (-0.5 + offset);
        f32vec2 blend_factor = fract(shadow_pix_coord);

        // texel gather component mapping - (00,w);(01,x);(11,y);(10,z) 
        f32 tmp0 = mix(shadow_gathered.w, shadow_gathered.z, blend_factor.x);
        f32 tmp1 = mix(shadow_gathered.x, shadow_gathered.y, blend_factor.x);
        shadow = mix(tmp0, tmp1, blend_factor.y);
    }

    const f32vec4 albedo = texture(daxa_sampler2D(_g_albedo, pc.nearest_sampler_id), uv);
    const f32vec3 normal = texture(daxa_sampler2D(_g_normals, pc.nearest_sampler_id), uv).xyz;

    const f32 sun_norm_dot = dot(normal, deref(_globals).sun_direction);
    out_color = albedo * (clamp(sun_norm_dot, 0.0, 1.0) * clamp(shadow, 0.0, 1.0) + 0.02);
    // out_color = albedo * (clamp(pow(shadow, 5), 0.0, 1.0) + 0.02);
    const f32vec4 cascade_colors[] = f32vec4[4](
        f32vec4(0.2, 0.0, 0.0, 0.0),
        f32vec4(0.0, 0.2, 0.0, 0.0),
        f32vec4(0.0, 0.0, 0.2, 0.0),
        f32vec4(0.2, 0.2, 0.0, 0.0)
    );
        // out_color = f32vec4(shadow, shadow, shadow, 1.0);
        // out_color += cascade_colors[cascade_idx];
}