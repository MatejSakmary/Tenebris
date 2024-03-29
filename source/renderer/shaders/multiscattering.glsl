#define DAXA_ENABLE_IMAGE_OVERLOADS_BASIC 1
#include <shared/shared.inl>
#include "common_func.glsl"
#include "tasks/multiscattering_LUT.inl"

DAXA_DECL_PUSH_CONSTANT(MultiscatteringPC, pc)

/* This number should match the number of local threads -> z dimension */
const daxa_f32 SPHERE_SAMPLES = 64.0;
const daxa_f32 GOLDEN_RATIO = 1.6180339;
const daxa_f32 uniformPhase = 1.0 / (4.0 * PI);

shared daxa_f32vec3 multiscatt_shared[64];
shared daxa_f32vec3 luminance_shared[64];

struct RaymarchResult 
{
    daxa_f32vec3 luminance;
    daxa_f32vec3 multiscattering;
};

RaymarchResult integrate_scattered_luminance(daxa_f32vec3 world_position, daxa_f32vec3 world_direction, daxa_f32vec3 sun_direction, daxa_f32 sample_count)
{
    RaymarchResult result = RaymarchResult(daxa_f32vec3(0.0, 0.0, 0.0), daxa_f32vec3(0.0, 0.0, 0.0));
    daxa_f32vec3 planet_zero = daxa_f32vec3(0.0, 0.0, 0.0);
    daxa_f32 planet_intersection_distance = ray_sphere_intersect_nearest(
        world_position, world_direction, planet_zero, deref(_globals).atmosphere_bottom);
    daxa_f32 atmosphere_intersection_distance = ray_sphere_intersect_nearest(
        world_position, world_direction, planet_zero, deref(_globals).atmosphere_top);
    
    daxa_f32 integration_length;
    /* ============================= CALCULATE INTERSECTIONS ============================ */
    if((planet_intersection_distance == -1.0) && (atmosphere_intersection_distance == -1.0)){
        /* ray does not intersect planet or atmosphere -> no point in raymarching*/
        return result;
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
    daxa_f32 integration_step = integration_length / daxa_f32(sample_count);

    /* stores accumulated transmittance during the raymarch process */
    daxa_f32vec3 accum_transmittance = daxa_f32vec3(1.0, 1.0, 1.0);
    /* stores accumulated light contribution during the raymarch process */
    daxa_f32vec3 accum_light = daxa_f32vec3(0.0, 0.0, 0.0);
    daxa_f32 old_ray_shift = 0;

    /* ============================= RAYMARCH ==========================================  */
    for(daxa_i32 i = 0; i < sample_count; i++)
    {
        /* Sampling at 1/3rd of the integration step gives better results for exponential
           functions */
        daxa_f32 new_ray_shift = integration_length * (daxa_f32(i) + 0.3) / sample_count;
        integration_step = new_ray_shift - old_ray_shift;
        daxa_f32vec3 new_position = world_position + new_ray_shift * world_direction;
        old_ray_shift = new_ray_shift;

        /* Raymarch shifts the angle to the sun a bit recalculate */
        daxa_f32vec3 up_vector = normalize(new_position);
        TransmittanceParams transmittance_lut_params = TransmittanceParams(length(new_position), dot(sun_direction, up_vector));

        /* uv coordinates later used to sample transmittance texture */
        daxa_f32vec2 trans_texture_uv = transmittance_lut_to_uv(transmittance_lut_params, deref(_globals).atmosphere_bottom, deref(_globals).atmosphere_top);

        daxa_f32vec3 transmittance_to_sun = texture(daxa_sampler2D(_transmittance_LUT, pc.sampler_id), trans_texture_uv).rgb;

        daxa_f32vec3 medium_scattering = sample_medium_scattering(_globals, new_position);
        daxa_f32vec3 medium_extinction = sample_medium_extinction(_globals, new_position);

        /* TODO: This probably should be a texture lookup altho might be slow*/
        daxa_f32vec3 trans_increase_over_integration_step = exp(-(medium_extinction * integration_step));
        /* Check if current position is in earth's shadow */
        daxa_f32 earth_intersection_distance = ray_sphere_intersect_nearest(
            new_position, sun_direction, planet_zero + PLANET_RADIUS_OFFSET * up_vector, deref(_globals).atmosphere_bottom);
        daxa_f32 in_earth_shadow = earth_intersection_distance == -1.0 ? 1.0 : 0.0;

        /* Light arriving from the sun to this point */
        daxa_f32vec3 sunLight = in_earth_shadow * transmittance_to_sun * medium_scattering * uniformPhase;
        daxa_f32vec3 multiscattered_cont_int = (medium_scattering - medium_scattering * trans_increase_over_integration_step) / medium_extinction;
        daxa_f32vec3 inscatteredContInt = (sunLight - sunLight * trans_increase_over_integration_step) / medium_extinction;

        if(medium_extinction.r == 0.0) { multiscattered_cont_int.r = 0.0; inscatteredContInt.r = 0.0; }
        if(medium_extinction.g == 0.0) { multiscattered_cont_int.g = 0.0; inscatteredContInt.g = 0.0; }
        if(medium_extinction.b == 0.0) { multiscattered_cont_int.b = 0.0; inscatteredContInt.b = 0.0; }

        result.multiscattering += accum_transmittance * multiscattered_cont_int;
        accum_light += accum_transmittance * inscatteredContInt;
        accum_transmittance *= trans_increase_over_integration_step;
    }
    result.luminance = accum_light;
    return result;
    /* TODO: Check for bounced light off the earth */
}

layout (local_size_x = 1, local_size_y = 1, local_size_z = 64) in;
void main()
{
    if( gl_GlobalInvocationID.x >= deref(_globals).mult_lut_dim.x ||
        gl_GlobalInvocationID.y >= deref(_globals).mult_lut_dim.y)
    { return; } 

    const daxa_f32 sample_count = 20;

    daxa_f32vec2 uv = (daxa_f32vec2(gl_GlobalInvocationID.xy) + daxa_f32vec2(0.5, 0.5)) / 
                  daxa_f32vec2(deref(_globals).mult_lut_dim.xy);
    uv = daxa_f32vec2(from_subuv_to_unit(uv.x, deref(_globals).mult_lut_dim.x),
                 from_subuv_to_unit(uv.y, deref(_globals).mult_lut_dim.y));
    
    /* Mapping uv to multiscattering LUT parameters
       TODO -> Is the range from 0.0 to -1.0 really needed? */
    daxa_f32 sun_cos_zenith_angle = uv.x * 2.0 - 1.0;
    daxa_f32vec3 sun_direction = daxa_f32vec3(
        0.0,
        safe_sqrt(clamp(1.0 - sun_cos_zenith_angle * sun_cos_zenith_angle, 0.0, 1.0)),
        sun_cos_zenith_angle
    );

   daxa_f32 view_height = deref(_globals).atmosphere_bottom + 
        clamp(uv.y + PLANET_RADIUS_OFFSET, 0.0, 1.0) *
        (deref(_globals).atmosphere_top - deref(_globals).atmosphere_bottom - PLANET_RADIUS_OFFSET);

    daxa_f32vec3 world_position = daxa_f32vec3(0.0, 0.0, view_height);

    daxa_f32 sample_idx = gl_LocalInvocationID.z;
    // local thread dependent raymarch
    { 
        #define USE_HILL_SAMPLING 0
        #if USE_HILL_SAMPLING
            #define SQRTSAMPLECOUNT 8
            const daxa_f32 sqrt_sample = daxa_f32(SQRTSAMPLECOUNT);
            daxa_f32 i = 0.5 + daxa_f32(sample_idx / SQRTSAMPLECOUNT);
            daxa_f32 j = 0.5 + mod(sample_idx, SQRTSAMPLECOUNT);
            daxa_f32 randA = i / sqrt_sample;
            daxa_f32 randB = j / sqrt_sample;

            daxa_f32 theta = 2.0 * PI * randA;
            daxa_f32 phi = PI * randB;
        #else
        /* Fibbonaci lattice -> http://extremelearning.com.au/how-to-evenly-distribute-points-on-a-sphere-more-effectively-than-the-canonical-fibonacci-lattice/ */
            daxa_f32 theta = acos( 1.0 - 2.0 * (sample_idx + 0.5) / SPHERE_SAMPLES );
            daxa_f32 phi = (2 * PI * sample_idx) / GOLDEN_RATIO;
        #endif


        daxa_f32vec3 world_direction = daxa_f32vec3( cos(theta) * sin(phi), sin(theta) * sin(phi), cos(phi));
        RaymarchResult result = integrate_scattered_luminance(world_position, world_direction, sun_direction, sample_count);

        multiscatt_shared[gl_LocalInvocationID.z] = result.multiscattering / SPHERE_SAMPLES;
        luminance_shared[gl_LocalInvocationID.z] = result.luminance / SPHERE_SAMPLES;
    }

    groupMemoryBarrier();
    barrier();

    if(gl_LocalInvocationID.z < 32)
    {
        multiscatt_shared[gl_LocalInvocationID.z] += multiscatt_shared[gl_LocalInvocationID.z + 32];
        luminance_shared[gl_LocalInvocationID.z] += luminance_shared[gl_LocalInvocationID.z + 32];
    }
    groupMemoryBarrier();
    barrier();
    if(gl_LocalInvocationID.z < 16)
    {
        multiscatt_shared[gl_LocalInvocationID.z] += multiscatt_shared[gl_LocalInvocationID.z + 16];
        luminance_shared[gl_LocalInvocationID.z] += luminance_shared[gl_LocalInvocationID.z + 16];
    }
    groupMemoryBarrier();
    barrier();
    if(gl_LocalInvocationID.z < 8)
    {
        multiscatt_shared[gl_LocalInvocationID.z] += multiscatt_shared[gl_LocalInvocationID.z + 8];
        luminance_shared[gl_LocalInvocationID.z] += luminance_shared[gl_LocalInvocationID.z + 8];
    }
    groupMemoryBarrier();
    barrier();
    if(gl_LocalInvocationID.z < 4)
    {
        multiscatt_shared[gl_LocalInvocationID.z] += multiscatt_shared[gl_LocalInvocationID.z + 4];
        luminance_shared[gl_LocalInvocationID.z] += luminance_shared[gl_LocalInvocationID.z + 4];
    }
    groupMemoryBarrier();
    barrier();
    if(gl_LocalInvocationID.z < 2)
    {
        multiscatt_shared[gl_LocalInvocationID.z] += multiscatt_shared[gl_LocalInvocationID.z + 2];
        luminance_shared[gl_LocalInvocationID.z] += luminance_shared[gl_LocalInvocationID.z + 2];
    }
    groupMemoryBarrier();
    barrier();
    if(gl_LocalInvocationID.z < 1)
    {
        multiscatt_shared[gl_LocalInvocationID.z] += multiscatt_shared[gl_LocalInvocationID.z + 1];
        luminance_shared[gl_LocalInvocationID.z] += luminance_shared[gl_LocalInvocationID.z + 1];
    }
    groupMemoryBarrier();
    barrier();
    if(gl_LocalInvocationID.z != 0)
        return;

    daxa_f32vec3 multiscatt_sum = multiscatt_shared[0];
    daxa_f32vec3 inscattered_luminance_sum = luminance_shared[0];

    const daxa_f32vec3 r = multiscatt_sum;
    const daxa_f32vec3 sum_of_all_multiscattering_events_contribution = daxa_f32vec3(1.0/ (1.0 -r.x),1.0/ (1.0 -r.y),1.0/ (1.0 -r.z));
    daxa_f32vec3 lum = inscattered_luminance_sum * sum_of_all_multiscattering_events_contribution;

    imageStore(daxa_image2D(_multiscattering_LUT), daxa_i32vec2(gl_GlobalInvocationID.xy), daxa_f32vec4(lum, 1.0));
}