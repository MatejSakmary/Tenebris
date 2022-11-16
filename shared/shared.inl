#pragma once

#include <daxa/daxa.inl>

DAXA_DECL_BUFFER_STRUCT(
    AtmosphereParameters,
    {
        daxa_f32vec3 mie_scattering;
        daxa_f32vec3 mie_absorption;
        daxa_f32vec3 mie_extinction;
        daxa_f32vec4 mie_density[3];

        daxa_f32vec3 rayleigh_scattering;
        daxa_f32vec4 rayleigh_density[3];

        daxa_f32vec3 absorption_extinction;
        daxa_f32vec4 absorption_density[3];
    }
);

struct TransmittancePush
{
    daxa_ImageViewId transmittance_image;
    daxa_u32vec2 dimensions;
    daxa_BufferRef(AtmosphereParameters) atmosphere_parameters;
};

struct FinalPassPush
{
    daxa_ImageViewId image_id;
    daxa_SamplerId sampler_id;
};