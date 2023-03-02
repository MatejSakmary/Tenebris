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

// f32vec3 sunWithBloom(f32vec3 worldDir, f32vec3 sunDir)
// {
//     const f32 sunSolidAngle = 1.0 * PI / 180.0;
//     const f32 minSunCosTheta = cos(sunSolidAngle);

//     f32 cosTheta = dot(worldDir, sunDir);
//     if(cosTheta >= minSunCosTheta) {return f32vec3(0.5) ;}
//     f32 offset = minSunCosTheta - cosTheta;
//     f32 gaussianBloom = exp(-offset * 50000.0) * 0.5;
//     f32 invBloom = 1.0/(0.02 + offset * 300.0) * 0.01;
//     return f32vec3(gaussianBloom + invBloom);
// }

/* One unit in global space should be 100 meters in camera coords */

void main() 
{
    const f32vec3 camera = deref(camera_params).camera_position;
    f32vec3 sun_direction = deref(atmosphere_params).sun_direction;

    f32vec3 clip_space = f32vec3(in_uv * f32vec2(2.0) - f32vec2(1.0), 1.0);
    
    f32vec4 h_pos = deref(camera_params).inv_view_projection * f32vec4(clip_space, 1.0);

    f32vec3 world_dir = normalize(h_pos.xyz/h_pos.w - camera); 
    f32vec3 world_pos = camera + f32vec3(0, 0, deref(atmosphere_params).atmosphere_bottom);

    f32 view_height = length(world_pos);
    f32vec3 L = f32vec3(0.0, 0.0, 0.0);

    f32vec2 sky_uv;
    f32vec3 up_vector = normalize(world_pos);
    f32 view_zenith_angle = acos(dot(world_dir, up_vector));

    f32 light_view_angle = acos(dot(normalize(f32vec3(sun_direction.xy, 0.0)), normalize(f32vec3(world_dir.xy, 0.0))));
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

    // if(!IntersectGround)
    // {
    //     L += sunWithBloom(WorldDir, sun_direction);
    // };

    // out_color = f32vec4(L, 1.0);
    // out_color = f32vec4(h_pos.xyz, 1.0);
    out_color = f32vec4(L, 1.0);
}