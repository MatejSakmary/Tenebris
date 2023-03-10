/* Parts of code take from https://github.com/SebLague/Clouds/blob/master/Assets/Scripts/Clouds/Shaders/Clouds.shader */

#define DAXA_ENABLE_SHADER_NO_NAMESPACE 1
#define DAXA_ENABLE_IMAGE_OVERLOADS_BASIC 1
#include <shared/shared.inl>
#include "common_func.glsl"

DAXA_USE_PUSH_CONSTANT(DrawSkyPC)
daxa_BufferPtr(AtmosphereParameters) atmosphere_params = daxa_push_constant.atmosphere_parameters;
daxa_BufferPtr(CameraParameters) camera_params = daxa_push_constant.camera_parameters;

layout (location = 0) in f32vec2 in_uv;
layout (location = 0) out f32vec4 out_color;

f32vec3 add_sun_circle(f32vec3 world_dir, f32vec3 sun_dir)
{
    const f32 sun_solid_angle = 0.5 * PI / 180.0;
    const f32 min_sun_cos_theta = cos(sun_solid_angle);

    f32 cos_theta = dot(world_dir, sun_dir);
    if(cos_theta >= min_sun_cos_theta) {return f32vec3(1.0);}
    else {return f32vec3(0.0);}
}

/* One unit in global space should be 100 meters in camera coords */

void main() 
{
    f32vec3 camera = deref(camera_params).camera_position;
    f32vec3 sun_direction = normalize(deref(atmosphere_params).sun_direction);

    f32vec2 remap_uv = (in_uv * 2.0) - 1.0;

    // f32vec3 clip_space = f32vec3(in_uv * f32vec2(2.0) - f32vec2(1.0), 1.0);
    // f32vec4 h_pos = deref(camera_params).inv_view_projection * f32vec4(clip_space, 1.0);
    // f32vec3 world_dir = normalize(h_pos.xyz/h_pos.w - camera); 

    f32vec3 world_dir = normalize(
        deref(camera_params).camera_front +
        remap_uv.x * deref(camera_params).camera_frust_right_offset +
        remap_uv.y * deref(camera_params).camera_frust_top_offset);

    f32vec3 world_pos = camera;

    f32 view_height = length(world_pos);
    f32vec3 L = f32vec3(0.0, 0.0, 0.0);

    f32vec3 up_vector = normalize(world_pos);

    f32 view_zenith_angle = acos(dot(world_dir, up_vector));
    f32 light_view_angle = acos(dot(sun_direction, world_dir));

    bool intersects_ground = ray_sphere_intersect_nearest(
        world_pos, world_dir, f32vec3(0.0, 0.0, 0.0),
        deref(atmosphere_params).atmosphere_bottom) >= 0.0;

    f32vec2 uv = skyview_lut_params_to_uv(
        intersects_ground,
        SkyviewParams(view_zenith_angle, light_view_angle),
        deref(atmosphere_params).atmosphere_bottom,
        deref(atmosphere_params).atmosphere_top,
        f32vec2(daxa_push_constant.skyview_dimensions),
        view_height);

    L += f32vec3(texture(daxa_push_constant.skyview_image, daxa_push_constant.sampler_id, uv).rgb);

    if(!intersects_ground) { L += add_sun_circle(world_dir, sun_direction); };

    out_color = f32vec4(L, 1.0);
    // out_color = f32vec4(world_dir, 1.0);
}