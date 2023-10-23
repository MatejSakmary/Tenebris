#define DAXA_ENABLE_IMAGE_OVERLOADS_BASIC 1
#include <shared/shared.inl>
#include "common_func.glsl"
#include "tasks/transmittance_LUT.inl"

layout (local_size_x = 8, local_size_y = 4) in;

daxa_f32vec3 integrate_transmittance(daxa_f32vec3 world_position, daxa_f32vec3 world_direction, daxa_u32 sample_count)
{
    /* The length of ray between position and nearest atmosphere top boundary */
    daxa_f32 integration_length = ray_sphere_intersect_nearest(
        world_position,
        world_direction,
        daxa_f32vec3(0.0, 0.0, 0.0),
        deref(_globals).atmosphere_top);

    daxa_f32 integration_step = integration_length / daxa_f32(sample_count);

    /* Result of the integration */
    daxa_f32vec3 optical_depth = daxa_f32vec3(0.0, 0.0, 0.0);

    for(daxa_i32 i = 0; i < sample_count; i++)
    {
        /* Move along the world direction ray to new position */
        daxa_f32vec3 new_pos = world_position + i * integration_step * world_direction;
        daxa_f32vec3 atmosphere_extinction = sample_medium_extinction(_globals, new_pos);
        optical_depth += atmosphere_extinction * integration_step;
    }
    return optical_depth;
}

void main()
{
    if( gl_GlobalInvocationID.x >= deref(_globals).trans_lut_dim.x ||
        gl_GlobalInvocationID.y >= deref(_globals).trans_lut_dim.y)
    { return; } 

    daxa_f32vec2 uv = daxa_f32vec2(gl_GlobalInvocationID.xy) / daxa_f32vec2(deref(_globals).trans_lut_dim.xy);

    TransmittanceParams mapping = uv_to_transmittance_lut_params(
        uv,
        deref(_globals).atmosphere_bottom,
        deref(_globals).atmosphere_top
    );

    daxa_f32vec3 world_position = daxa_f32vec3(0.0, 0.0, mapping.height);
    daxa_f32vec3 world_direction = daxa_f32vec3(
        safe_sqrt(1.0 - mapping.zenith_cos_angle * mapping.zenith_cos_angle),
        0.0,
        mapping.zenith_cos_angle
    );

    daxa_f32vec3 transmittance = exp(-integrate_transmittance(world_position, world_direction, 400));

    imageStore(daxa_image2D(_transmittance_LUT), daxa_i32vec2(gl_GlobalInvocationID.xy), daxa_f32vec4(transmittance, 1.0));
}