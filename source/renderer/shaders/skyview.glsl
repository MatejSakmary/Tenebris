#define DAXA_ENABLE_SHADER_NO_NAMESPACE 1
#define DAXA_ENABLE_IMAGE_OVERLOADS_BASIC 1
#include <shared/shared.inl>
#include "common_func.glsl"

DAXA_USE_PUSH_CONSTANT(SkyviewPC)
daxa_BufferPtr(AtmosphereParameters) params = daxa_push_constant.atmosphere_parameters;

/* ============================= PHASE FUNCTIONS ============================ */
f32 cornette_shanks_mie_phase_function(f32 g, f32 cos_theta)
{
    f32 k = 3.0 / (8.0 * PI) * (1.0 - g * g) / (2.0 + g * g);
    return k * (1.0 + cos_theta * cos_theta) / pow(1.0 + g * g - 2.0 * g * -cos_theta, 1.5);
}

f32 rayleigh_phase(f32 cos_theta)
{
    f32 factor = 3.0 / (16.0 * PI);
    return factor * (1.0 + cos_theta * cos_theta);
}
/* ========================================================================== */

f32vec3 get_multiple_scattering(f32vec3 world_position, f32 view_zenith_cos_angle)
{
    f32vec2 uv = clamp(f32vec2( 
        view_zenith_cos_angle * 0.5 + 0.5,
        (length(world_position) - deref(params).atmosphere_bottom) /
        (deref(params).atmosphere_top - deref(params).atmosphere_bottom)),
        0.0, 1.0);
    uv = f32vec2(from_unit_to_subuv(uv.x, daxa_push_constant.multiscattering_dimensions.x),
                 from_unit_to_subuv(uv.y, daxa_push_constant.multiscattering_dimensions.y));

    return texture( daxa_push_constant.multiscattering_image, daxa_push_constant.sampler_id, uv).rgb;
}


f32vec3 integrate_scattered_luminance(f32vec3 world_position, 
    f32vec3 world_direction, f32vec3 sun_direction, i32 sample_count)
{
    f32vec3 planet_zero = f32vec3(0.0, 0.0, 0.0);
    f32 planet_intersection_distance = ray_sphere_intersect_nearest(
        world_position, world_direction, planet_zero, deref(params).atmosphere_bottom);
    f32 atmosphere_intersection_distance = ray_sphere_intersect_nearest(
        world_position, world_direction, planet_zero, deref(params).atmosphere_top);
    
    f32 integration_length;
    /* ============================= CALCULATE INTERSECTIONS ============================ */
    if((planet_intersection_distance == -1.0) && (atmosphere_intersection_distance == -1.0)){
        /* ray does not intersect planet or atmosphere -> no point in raymarching*/
        return f32vec3(0.0, 0.0, 0.0);
    } 
    else if((planet_intersection_distance == -1.0) && (atmosphere_intersection_distance > 0.0)){
        /* ray intersects only atmosphere */
        integration_length = atmosphere_intersection_distance;
    }
    else if((planet_intersection_distance > 0.0) && (atmosphere_intersection_distance == -1.0)){
        /* ray intersects only planet */
        integration_length = planet_intersection_distance;
    } else {
        /* ray intersects both planet and atmosphere -> return the first intersection */
        integration_length = min(planet_intersection_distance, atmosphere_intersection_distance);
    }

    f32 cos_theta = dot(sun_direction, world_direction);
    f32 mie_phase_value = cornette_shanks_mie_phase_function(deref(params).mie_phase_function_g, -cos_theta);
    f32 rayleigh_phase_value = rayleigh_phase(cos_theta);

    f32vec3 accum_transmittance = f32vec3(1.0, 1.0, 1.0);
    f32vec3 accum_light = f32vec3(0.0, 0.0, 0.0);
    /* ============================= RAYMARCH ============================ */
    for(i32 i = 0; i < sample_count; i++)
    {
        /* Step size computation */
        f32 step_0 = f32(i) / sample_count;
        f32 step_1 = f32(i + 1) / sample_count;

        /* Nonuniform step size*/
        step_0 *= step_0;
        step_1 *= step_1;

        step_0 = step_0 * integration_length;
        step_1 = step_1 > 1.0 ? integration_length : step_1 * integration_length;
        /* Sample at one third of the integrated interval -> better results for
           exponential functions */
        f32 integration_step = step_0 + (step_1 - step_0) * 0.3;
        f32 d_int_step = step_1 - step_0;

        /* Position shift */
        f32vec3 new_position = world_position + integration_step * world_direction;
        ScatteringSample medium_scattering = sample_medium_scattering_detailed(params, new_position);
        f32vec3 medium_extinction = sample_medium_extinction(params, new_position);

        /* Raymarch shifts the angle to the sun a bit recalculate */
        f32vec3 up_vector = normalize(new_position);
        TransmittanceParams transmittance_lut_params = TransmittanceParams(length(new_position), dot(sun_direction, up_vector));

        /* uv coordinates later used to sample transmittance texture */
        f32vec2 trans_texture_uv = transmittance_lut_to_uv(transmittance_lut_params, deref(params).atmosphere_bottom, deref(params).atmosphere_top);

        f32vec3 transmittance_to_sun = texture( daxa_push_constant.transmittance_image, daxa_push_constant.sampler_id, trans_texture_uv).rgb;

        f32vec3 phase_times_scattering = medium_scattering.mie * mie_phase_value + medium_scattering.ray * rayleigh_phase_value;

        f32 earth_intersection_distance = ray_sphere_intersect_nearest(
            new_position, sun_direction, planet_zero + PLANET_RADIUS_OFFSET * up_vector, deref(params).atmosphere_bottom);
        f32 in_earth_shadow = earth_intersection_distance == -1.0 ? 1.0 : 0.0;

        f32vec3 multiscattered_luminance = get_multiple_scattering(new_position, dot(sun_direction, up_vector)); 

        /* Light arriving from the sun to this point */
        f32vec3 sun_light = in_earth_shadow * transmittance_to_sun * phase_times_scattering +
            multiscattered_luminance * (medium_scattering.ray + medium_scattering.mie);

        /* TODO: This probably should be a texture lookup*/
        f32vec3 trans_increase_over_integration_step = exp(-(medium_extinction * d_int_step));
        f32vec3 sun_light_integ = (sun_light - sun_light * trans_increase_over_integration_step) / medium_extinction;
        accum_light += accum_transmittance * sun_light_integ;
        accum_transmittance *= trans_increase_over_integration_step;
    }
    return accum_light;
}

layout (local_size_x = 8, local_size_y = 4) in;
void main()
{
    if( gl_GlobalInvocationID.x >= daxa_push_constant.skyview_dimensions.x ||
        gl_GlobalInvocationID.y >= daxa_push_constant.skyview_dimensions.y)
    { return; } 

    /* TODO: change to represent real camera position */
    const f32 camera_height = deref(params).camera_position.z; // * cameraScale;
    f32vec3 world_position = f32vec3(0.0, 0.0, camera_height + deref(params).atmosphere_bottom);

    f32vec2 uv = f32vec2(gl_GlobalInvocationID.xy) / f32vec2(daxa_push_constant.skyview_dimensions.xy);
    SkyviewParams skyview_params = uv_to_skyview_lut_params(uv, deref(params).atmosphere_bottom,
        deref(params).atmosphere_top, daxa_push_constant.skyview_dimensions, length(world_position));

    f32 sun_zenith_cos_angle = dot(normalize(world_position), deref(params).sun_direction);
    f32vec3 local_sun_direction = normalize(f32vec3(
        safe_sqrt(1.0 - sun_zenith_cos_angle * sun_zenith_cos_angle),
        0.0,
        sun_zenith_cos_angle));
    
    f32vec3 world_direction = f32vec3(
        cos(skyview_params.light_view_angle) * sin(skyview_params.view_zenith_angle),
        sin(skyview_params.light_view_angle) * sin(skyview_params.view_zenith_angle),
        cos(skyview_params.view_zenith_angle));
    
    if (!move_to_top_atmosphere(world_position, world_direction, deref(params).atmosphere_bottom, deref(params).atmosphere_top))
    {
        /* No intersection with the atmosphere */
        imageStore(daxa_push_constant.skyview_image, i32vec2(gl_GlobalInvocationID.xy), f32vec4(0.0, 0.0, 0.0, 1.0));
        return;
    }
    f32vec3 luminance = integrate_scattered_luminance(world_position, world_direction, local_sun_direction, 30);
    imageStore(daxa_push_constant.skyview_image, i32vec2(gl_GlobalInvocationID.xy), f32vec4(luminance, 1.0));
}