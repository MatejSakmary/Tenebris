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

struct AtmosphereParameters
{
    daxa_f32vec3 sun_direction;

    daxa_f32 atmosphere_bottom;
    daxa_f32 atmosphere_top;

    daxa_f32vec3 mie_scattering;
    daxa_f32vec3 mie_extinction;
    daxa_f32 mie_scale_height;
    daxa_f32 mie_phase_function_g;
    DensityProfileLayer mie_density[2];

    daxa_f32vec3 rayleigh_scattering;
    daxa_f32 rayleigh_scale_height;
    DensityProfileLayer rayleigh_density[2];

    daxa_f32vec3 absorption_extinction;
    DensityProfileLayer absorption_density[2];
};

DAXA_ENABLE_BUFFER_PTR(AtmosphereParameters)

struct CameraParameters
{
    daxa_f32vec3 camera_position;
    daxa_f32vec3 camera_front;
    daxa_f32vec3 camera_frust_top_offset;
    daxa_f32vec3 camera_frust_right_offset;
    daxa_f32mat4x4 view;
    daxa_f32mat4x4 projection;
    daxa_f32mat4x4 inv_view_projection;
    daxa_f32 time;
};

DAXA_ENABLE_BUFFER_PTR(CameraParameters)

struct TerrainVertex
{
    daxa_f32vec3 position;
};
DAXA_ENABLE_BUFFER_PTR(TerrainVertex)

struct TerrainIndex
{
    daxa_u32 index;
};
DAXA_ENABLE_BUFFER_PTR(TerrainIndex)

struct TransmittancePC
{
    daxa_RWImage2Df32 transmittance_image;
    daxa_u32vec2 dimensions;
    daxa_BufferPtr(AtmosphereParameters) atmosphere_parameters;
};

struct MultiscatteringPC
{
    daxa_Image2Df32 transmittance_image;
    daxa_RWImage2Df32 multiscattering_image;
    daxa_u32vec2 multiscattering_dimensions;
    daxa_SamplerId sampler_id;
    daxa_BufferPtr(AtmosphereParameters) atmosphere_parameters;
};

struct SkyviewPC
{
    daxa_Image2Df32 transmittance_image;
    daxa_Image2Df32 multiscattering_image;
    daxa_RWImage2Df32 skyview_image;
    daxa_u32vec2 skyview_dimensions;
    daxa_u32vec2 multiscattering_dimensions;
    daxa_SamplerId sampler_id;
    daxa_BufferPtr(AtmosphereParameters) atmosphere_parameters;
    daxa_BufferPtr(CameraParameters) camera_parameters;
};

struct DrawTerrainPC
{
    daxa_BufferPtr(TerrainVertex) vertices;
    daxa_BufferPtr(CameraParameters) camera_parameters;
};

struct DrawSkyPC
{
    daxa_Image2Df32 skyview_image;
    daxa_SamplerId sampler_id;
    daxa_u32vec2 skyview_dimensions;
    daxa_BufferPtr(AtmosphereParameters) atmosphere_parameters;
    daxa_BufferPtr(CameraParameters) camera_parameters;
};

struct PostProcessPC
{
    daxa_Image2Df32 offscreen_id;
    daxa_SamplerId sampler_id;
};