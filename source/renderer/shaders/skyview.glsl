#define DAXA_ENABLE_IMAGE_OVERLOADS_BASIC 1
#include <shared/shared.inl>
#include "common_func.glsl"
#include "tasks/skyview_LUT.inl"

DAXA_DECL_PUSH_CONSTANT(SkyviewPC, pc)

/* ============================= PHASE FUNCTIONS ============================ */
daxa_f32 cornette_shanks_mie_phase_function(daxa_f32 g, daxa_f32 cos_theta)
{
    daxa_f32 k = 3.0 / (8.0 * PI) * (1.0 - g * g) / (2.0 + g * g);
    return k * (1.0 + cos_theta * cos_theta) / pow(1.0 + g * g - 2.0 * g * -cos_theta, 1.5);
}
#define TAU 2 * PI
daxa_f32 klein_nishina_phase(daxa_f32 cos_theta, daxa_f32 e) {
    return e / (TAU * (e * (1.0 - cos_theta) + 1.0) * log(2.0 * e + 1.0));
}

daxa_f32 rayleigh_phase(daxa_f32 cos_theta)
{
    daxa_f32 factor = 3.0 / (16.0 * PI);
    return factor * (1.0 + cos_theta * cos_theta);
}
// https://research.nvidia.com/labs/rtr/approximate-mie/publications/approximate-mie.pdf
daxa_f32 draine_phase(daxa_f32 alpha, daxa_f32 g, daxa_f32 cos_theta)
{
    return 
        (1.0/(4.0 * PI)) *
        ((1.0 - (g * g))/pow((1.0 + (g * g) - (2.0 * g * cos_theta)), 3.0 / 2.0)) *
        ((1.0 + (alpha * cos_theta * cos_theta))/(1.0 + (alpha * (1.0 / 3.0) * (1.0 + (2.0 * g * g)))));
}

daxa_f32 hg_draine_phase(daxa_f32 cos_theta, daxa_f32 diameter)
{
    const daxa_f32 g_hg = exp(-(0.0990567/(diameter - 1.67154)));
    const daxa_f32 g_d = exp(-(2.20679/(diameter + 3.91029)) - 0.428934);
    const daxa_f32 alpha = exp(3.62489 - (0.599085/(diameter + 5.52825)));
    const daxa_f32 w_d = exp(-(0.599085/(diameter - 0.641583)) - 0.665888);
    return (1 - w_d) * draine_phase(0, g_hg, cos_theta) + w_d * draine_phase(alpha, g_d, cos_theta);
}
/* ========================================================================== */

daxa_f32vec3 get_multiple_scattering(daxa_f32vec3 world_position, daxa_f32 view_zenith_cos_angle)
{
    daxa_f32vec2 uv = clamp(daxa_f32vec2( 
        view_zenith_cos_angle * 0.5 + 0.5,
        (length(world_position) - deref(_globals).atmosphere_bottom) /
        (deref(_globals).atmosphere_top - deref(_globals).atmosphere_bottom)),
        0.0, 1.0);
    uv = daxa_f32vec2(from_unit_to_subuv(uv.x, deref(_globals).mult_lut_dim.x),
                 from_unit_to_subuv(uv.y, deref(_globals).mult_lut_dim.y));

    return texture(daxa_sampler2D(_multiscattering_LUT, pc.wrong_sampler_id), uv).rgb;
}

daxa_f32vec3 integrate_scattered_luminance(daxa_f32vec3 world_position, 
    daxa_f32vec3 world_direction, daxa_f32vec3 sun_direction, daxa_i32 sample_count)
{
    daxa_f32vec3 planet_zero = daxa_f32vec3(0.0, 0.0, 0.0);
    daxa_f32 planet_intersection_distance = ray_sphere_intersect_nearest(
        world_position, world_direction, planet_zero, deref(_globals).atmosphere_bottom);
    daxa_f32 atmosphere_intersection_distance = ray_sphere_intersect_nearest(
        world_position, world_direction, planet_zero, deref(_globals).atmosphere_top);
    
    daxa_f32 integration_length;
    /* ============================= CALCULATE INTERSECTIONS ============================ */
    if((planet_intersection_distance == -1.0) && (atmosphere_intersection_distance == -1.0)){
        /* ray does not intersect planet or atmosphere -> no point in raymarching*/
        return daxa_f32vec3(0.0, 0.0, 0.0);
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

    daxa_f32 cos_theta = dot(sun_direction, world_direction);
    // daxa_f32 mie_phase_value = klein_nishina_phase(cos_theta, 2800.0);
    daxa_f32 mie_phase_value = hg_draine_phase(cos_theta, 3.6);
    // daxa_f32 mie_phase_value = cornette_shanks_mie_phase_function(deref(_globals).mie_phase_function_g, -cos_theta);
    daxa_f32 rayleigh_phase_value = rayleigh_phase(cos_theta);

    daxa_f32vec3 accum_transmittance = daxa_f32vec3(1.0, 1.0, 1.0);
    daxa_f32vec3 accum_light = daxa_f32vec3(0.0, 0.0, 0.0);
    /* ============================= RAYMARCH ============================ */
    for(daxa_i32 i = 0; i < sample_count; i++)
    {
        /* Step size computation */
        daxa_f32 step_0 = daxa_f32(i) / sample_count;
        daxa_f32 step_1 = daxa_f32(i + 1) / sample_count;

        /* Nonuniform step size*/
        step_0 *= step_0;
        step_1 *= step_1;

        step_0 = step_0 * integration_length;
        step_1 = step_1 > 1.0 ? integration_length : step_1 * integration_length;
        /* Sample at one third of the integrated interval -> better results for exponential functions */
        daxa_f32 integration_step = step_0 + (step_1 - step_0) * 0.3;
        daxa_f32 d_int_step = step_1 - step_0;

        /* Position shift */
        daxa_f32vec3 new_position = world_position + integration_step * world_direction;
        ScatteringSample medium_scattering = sample_medium_scattering_detailed(_globals, new_position);
        daxa_f32vec3 medium_extinction = sample_medium_extinction(_globals, new_position);

        daxa_f32vec3 up_vector = normalize(new_position);
        TransmittanceParams transmittance_lut_params = TransmittanceParams(length(new_position), dot(sun_direction, up_vector));

        /* uv coordinates later used to sample transmittance texture */
        daxa_f32vec2 trans_texture_uv = transmittance_lut_to_uv(transmittance_lut_params, deref(_globals).atmosphere_bottom, deref(_globals).atmosphere_top);
        daxa_f32vec3 transmittance_to_sun = texture(daxa_sampler2D(_transmittance_LUT, pc.wrong_sampler_id), trans_texture_uv).rgb;

        daxa_f32vec3 phase_times_scattering = medium_scattering.mie * mie_phase_value + medium_scattering.ray * rayleigh_phase_value;

        daxa_f32 earth_intersection_distance = ray_sphere_intersect_nearest(
            new_position, sun_direction, planet_zero, deref(_globals).atmosphere_bottom);
        daxa_f32 in_earth_shadow = earth_intersection_distance == -1.0 ? 1.0 : 0.0;

        daxa_f32vec3 multiscattered_luminance = get_multiple_scattering(new_position, dot(sun_direction, up_vector)); 

        /* Light arriving from the sun to this point */
        daxa_f32vec3 sun_light = 
            ((in_earth_shadow * transmittance_to_sun * phase_times_scattering) + 
            (multiscattered_luminance * (medium_scattering.ray + medium_scattering.mie)));// * deref(_globals).sun_brightness;

        /* TODO: This probably should be a texture lookup*/
        daxa_f32vec3 trans_increase_over_integration_step = exp(-(medium_extinction * d_int_step));

        daxa_f32vec3 sun_light_integ = (sun_light - sun_light * trans_increase_over_integration_step) / medium_extinction;

        if(medium_extinction.r == 0.0) { sun_light_integ.r = 0.0; }
        if(medium_extinction.g == 0.0) { sun_light_integ.g = 0.0; }
        if(medium_extinction.b == 0.0) { sun_light_integ.b = 0.0; }

        accum_light += accum_transmittance * sun_light_integ;
        accum_transmittance *= trans_increase_over_integration_step;
    }
    return accum_light;
}

layout (local_size_x = 8, local_size_y = 4) in;
void main()
{
    if( gl_GlobalInvocationID.x >= deref(_globals).sky_lut_dim.x ||
        gl_GlobalInvocationID.y >= deref(_globals).sky_lut_dim.y)
    { return; } 

    daxa_f32vec3 world_position = (deref(_globals).camera_position - deref(_globals).offset) * UNIT_SCALE;
    world_position.z += deref(_globals).atmosphere_bottom;

    daxa_f32vec2 uv = daxa_f32vec2(gl_GlobalInvocationID.xy) / daxa_f32vec2(deref(_globals).sky_lut_dim.xy);
    SkyviewParams skyview_params = uv_to_skyview_lut_params(
        uv,
        deref(_globals).atmosphere_bottom,
        deref(_globals).atmosphere_top,
        deref(_globals).sky_lut_dim,
        length(world_position)
    );

    daxa_f32 sun_zenith_cos_angle = dot(normalize(world_position), deref(_globals).sun_direction);
    // sin^2 + cos^2 = 1 -> sqrt(1 - cos^2) = sin
    // rotate the sun direction so that we are aligned with the y = 0 axis
    daxa_f32vec3 local_sun_direction = normalize(daxa_f32vec3(
        safe_sqrt(1.0 - sun_zenith_cos_angle * sun_zenith_cos_angle),
        0.0,
        sun_zenith_cos_angle));
    
    daxa_f32vec3 world_direction = daxa_f32vec3(
        cos(skyview_params.light_view_angle) * sin(skyview_params.view_zenith_angle),
        sin(skyview_params.light_view_angle) * sin(skyview_params.view_zenith_angle),
        cos(skyview_params.view_zenith_angle));
    
    world_position = daxa_f32vec3(0, 0, length(world_position));

    if (!move_to_top_atmosphere(world_position, world_direction, deref(_globals).atmosphere_bottom, deref(_globals).atmosphere_top))
    {
        /* No intersection with the atmosphere */
        imageStore(daxa_image2D(_skyview_LUT), daxa_i32vec2(gl_GlobalInvocationID.xy), daxa_f32vec4(0.0, 0.0, 0.0, 1.0));
        return;
    }
    daxa_f32vec3 luminance = integrate_scattered_luminance(world_position, world_direction, local_sun_direction, 50);
    imageStore(daxa_image2D(_skyview_LUT), daxa_i32vec2(gl_GlobalInvocationID.xy), daxa_f32vec4(luminance, 1.0));
}