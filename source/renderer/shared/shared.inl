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

struct Globals
{
    // =============== Random variables ===============
    daxa_f32 time;
    daxa_u32vec2 trans_lut_dim;
    daxa_u32vec2 mult_lut_dim;
    daxa_u32vec2 sky_lut_dim;

    // =============== Atmosphere =====================
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
    
    // =============== Camera =========================
    daxa_f32vec3 camera_position;
    daxa_f32vec3 camera_front;
    daxa_f32vec3 camera_frust_top_offset;
    daxa_f32vec3 camera_frust_right_offset;
    daxa_f32mat4x4 view;
    daxa_f32mat4x4 ground_view;
    daxa_f32mat4x4 projection;
    daxa_f32mat4x4 inv_view_projection;

    // =============== Terrrain ======================
    daxa_f32vec3 terrain_scale;
    daxa_f32 terrain_delta;
    daxa_f32 terrain_min_depth;
    daxa_f32 terrain_max_depth;
    daxa_i32 terrain_min_tess_level;
    daxa_i32 terrain_max_tess_level;
};

DAXA_ENABLE_BUFFER_PTR(Globals)

struct TerrainVertex
{
    daxa_f32vec2 position;
};
DAXA_ENABLE_BUFFER_PTR(TerrainVertex)

struct TerrainIndex
{
    daxa_u32 index;
};
DAXA_ENABLE_BUFFER_PTR(TerrainIndex)


