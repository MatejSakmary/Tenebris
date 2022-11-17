#pragma once

#include <daxa/daxa.inl>

// An atmosphere layer density which can be calculated as:
//   density = exp_term * exp(exp_scale * h) + linear_term * h + constant_term,
struct DensityProfileLayer
{
    daxa_f32 layer_width;
    daxa_f32 exp_term;
    daxa_f32 exp_scale;
    daxa_f32 lin_term;
    daxa_f32 const_term;
};

DAXA_DECL_BUFFER_STRUCT(
    AtmosphereParameters,
    {
        daxa_f32 atmosphere_bottom;
        daxa_f32 atmosphere_top;

        daxa_f32vec3 mie_scattering;
        daxa_f32vec3 mie_extinction;
        daxa_f32 mie_scale_height;
        DensityProfileLayer mie_density[2];

        daxa_f32vec3 rayleigh_scattering;
        daxa_f32 rayleigh_scale_height;
        DensityProfileLayer rayleigh_density[2];

        daxa_f32vec3 absorption_extinction;
        DensityProfileLayer absorption_density[2];
    }
);

struct TransmittancePush
{
    daxa_ImageViewId transmittance_image;
    daxa_u32vec2 dimensions;
    daxa_BufferRef(AtmosphereParameters) atmosphere_parameters;
};

struct MultiscatteringPush
{
    daxa_ImageViewId transmittance_image;
    daxa_ImageViewId multiscattering_image;
    daxa_u32vec2 multiscattering_dimensions;
    daxa_SamplerId sampler_id;
    daxa_BufferRef(AtmosphereParameters) atmosphere_parameters;
};

struct SkyviewPush
{
    daxa_ImageViewId transmittance_image;
    daxa_ImageViewId multiscattering_image;
    daxa_ImageViewId skyview_image;
    daxa_u32vec2 skyview_dimensions;
    daxa_SamplerId sampler_id;
    daxa_BufferRef(AtmosphereParameters) atmosphere_parameters;
};

struct FinalPassPush
{
    daxa_ImageViewId image_id;
    daxa_SamplerId sampler_id;
};