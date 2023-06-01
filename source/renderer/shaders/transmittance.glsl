#define DAXA_ENABLE_SHADER_NO_NAMESPACE 1
#define DAXA_ENABLE_IMAGE_OVERLOADS_BASIC 1
#include <shared/shared.inl>
#include "common_func.glsl"
#include "tasks/transmittance_LUT.inl"

DAXA_USE_PUSH_CONSTANT(TransmittancePC, pc)

layout (local_size_x = 8, local_size_y = 4) in;

f32vec3 integrate_transmittance(f32vec3 world_position, f32vec3 world_direction, u32 sample_count)
{
    /* The length of ray between position and nearest atmosphere top boundary */
    f32 integration_length = ray_sphere_intersect_nearest(
        world_position,
        world_direction,
        f32vec3(0.0, 0.0, 0.0),
        deref(_globals).atmosphere_top);

    f32 integration_step = integration_length / f32(sample_count);

    /* Result of the integration */
    f32vec3 optical_depth = f32vec3(0.0, 0.0, 0.0);

    for(i32 i = 0; i < sample_count; i++)
    {
        /* Move along the world direction ray to new position */
        f32vec3 new_pos = world_position + i * integration_step * world_direction;
        f32vec3 atmosphere_extinction = sample_medium_extinction(_globals, new_pos);
        optical_depth += atmosphere_extinction * integration_step;
    }
    return optical_depth;
}

void main()
{
    if( gl_GlobalInvocationID.x >= pc.dimensions.x ||
        gl_GlobalInvocationID.y >= pc.dimensions.y)
    { return; } 

    f32vec2 uv = f32vec2(gl_GlobalInvocationID.xy) / f32vec2(pc.dimensions.xy);

    TransmittanceParams mapping = uv_to_transmittance_lut_params(
        uv,
        deref(_globals).atmosphere_bottom,
        deref(_globals).atmosphere_top
    );

    f32vec3 world_position = f32vec3(0.0, 0.0, mapping.height);
    f32vec3 world_direction = f32vec3(
        safe_sqrt(1.0 - mapping.zenith_cos_angle * mapping.zenith_cos_angle),
        0.0,
        mapping.zenith_cos_angle
    );

    f32vec3 transmittance = exp(-integrate_transmittance(world_position, world_direction, 400));

    imageStore(_transmittance_LUT, i32vec2(gl_GlobalInvocationID.xy), f32vec4(transmittance, 1.0));
}