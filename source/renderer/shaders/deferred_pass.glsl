/* Parts of code take from https://github.com/SebLague/Clouds/blob/master/Assets/Scripts/Clouds/Shaders/Clouds.shader */

#define DAXA_ENABLE_SHADER_NO_NAMESPACE 1
#define DAXA_ENABLE_IMAGE_OVERLOADS_BASIC 1
#include <shared/shared.inl>
#include "common_func.glsl"
#include "tasks/deferred_pass.inl"

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

f32vec3 get_far_sky_color(f32vec2 remap_uv)
{
    const f32 unit_scale = 0.001;
    f32vec3 camera = (deref(_globals).camera_position - deref(_globals).offset) * unit_scale;
    camera.z += deref(_globals).atmosphere_bottom;
    f32vec3 sun_direction = normalize(deref(_globals).sun_direction);

    f32vec4 h_pos = deref(_globals).inv_view_projection * f32vec4(remap_uv, 1.0, 1.0);

    f32vec3 world_dir = normalize((h_pos.xyz / h_pos.w) - deref(_globals).camera_position);

    f32vec3 world_pos = camera;

    f32 view_height = length(world_pos);
    f32vec3 L = f32vec3(0.0, 0.0, 0.0);

    f32vec3 up_vector = normalize(world_pos);

    f32 view_zenith_angle = acos(dot(world_dir, up_vector));
    f32 light_view_angle = acos(dot(sun_direction, world_dir));

    bool intersects_ground = ray_sphere_intersect_nearest(
        world_pos, world_dir, f32vec3(0.0, 0.0, 0.0),
        deref(_globals).atmosphere_bottom) >= 0.0;

    f32vec2 uv = skyview_lut_params_to_uv(
        intersects_ground,
        SkyviewParams(view_zenith_angle, light_view_angle),
        deref(_globals).atmosphere_bottom,
        deref(_globals).atmosphere_top,
        f32vec2(deref(_globals).sky_lut_dim),
        view_height);

    L += f32vec3(texture(daxa_sampler2D(_skyview, pc.linear_sampler_id), uv).rgb);

    if(!intersects_ground) { L += add_sun_circle(world_dir, sun_direction); };

    return L;
}

void main() 
{
    f32vec3 normal = texture(daxa_sampler2D(_g_normals, pc.nearest_sampler_id), uv).xyz;
    if(normal.xyz == f32vec3(0.0, 0.0, 0.0))
    {
        f32vec2 remap_uv = (uv * 2.0) - 1.0;
        out_color = f32vec4(get_far_sky_color(remap_uv), 1.0);
    } else {
        f32 depth = texture(daxa_sampler2D(_depth, pc.nearest_sampler_id), uv).x;

        f32vec2 remap_uv = (uv * 2.0) - 1.0;
        f32vec4 h_pos = deref(_globals).inv_view_projection * f32vec4(remap_uv, depth, 1.0);
        f32vec3 world_pos = h_pos.xyz / h_pos.w;

        f32vec4 albedo = texture(daxa_sampler2D(_g_albedo, pc.nearest_sampler_id), uv);

        const f32vec4 shadow_proj_world_pos = deref(_globals).shadowmap_projection * deref(_globals).shadowmap_view * f32vec4(world_pos, 1.0);

        const f32vec3 ndc_pos = shadow_proj_world_pos.xyz / shadow_proj_world_pos.w;
        const f32vec2 shadow_map_uv = (ndc_pos.xy + f32vec2(1.0)) / f32vec2(2.0);
        const f32 shadowmap_dist = texture(daxa_sampler2D(_esm, pc.linear_sampler_id), shadow_map_uv).r;

        const f32vec3 shadow_view_pos = f32vec4(deref(_globals).shadowmap_view * f32vec4(world_pos, 1.0)).xyz;
        const f32 near = 2000.0;
        const f32 far = 12000.0;
        const f32 depth_factor = 1/(far - near);
        const f32 real_dist = length(shadow_view_pos) * depth_factor;

        const f32 c = 80.0;
        f32 shadow = exp(-c * real_dist) * shadowmap_dist;

        // if shadowmap is out of bounds
        if(shadowmap_dist == 0.0f) { shadow = 1.0f; }

        const f32 threshold = 0.02;

        if(shadow > 1.0 + threshold)
        {
            f32vec4 gather = textureGather(daxa_sampler2D(_esm, pc.linear_sampler_id), shadow_map_uv, 0);
            f32vec4 shadow_gathered = clamp(exp(-c * real_dist) * gather, f32vec4(0.0, 0.0, 0.0, 0.0), f32vec4(1.0, 1.0, 1.0, 1.0));

            // This is needed because textureGather and fract are slightly imprecise so 
            const f32 offset = 1.0/512.0;
            f32vec2 shadow_pix_coord = shadow_map_uv * pc.esm_resolution + (-0.5 + offset);
            f32vec2 blend_factor = fract(shadow_pix_coord);

            // texel gather component mapping - (00,w);(01,x);(11,y);(10,z) 
            f32 tmp0 = mix(shadow_gathered.w, shadow_gathered.z, blend_factor.x);
            f32 tmp1 = mix(shadow_gathered.x, shadow_gathered.y, blend_factor.x);
            shadow = mix(tmp0, tmp1, blend_factor.y);
        }
        const f32 sun_norm_dot = dot(normal, deref(_globals).sun_direction);
        out_color = albedo * (clamp(sun_norm_dot, 0.0, 1.0) * clamp(shadow, 0.0, 1.0) + 0.02);
    }
}