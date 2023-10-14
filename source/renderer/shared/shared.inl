#pragma once

#include <daxa/daxa.inl>


#define MAX_FRUSTUM_COUNT 32
#define FRUSTUM_VERTEX_COUNT 8
#define NUM_CASCADES 4
#define SHADOWMAP_RESOLUTION 1024
#define UNIT_SCALE 0.001
#define HISTOGRAM_BIN_COUNT 256

#define VSM_TEXTURE_RESOLUTION 512
#define VSM_MEMORY_RESOLUTION 1024
#define VSM_PAGE_SIZE 128
#define VSM_CLIP_LEVELS 1
#define VSM_PAGE_TABLE_RESOLUTION (VSM_TEXTURE_RESOLUTION / VSM_PAGE_SIZE)
#define VSM_META_MEMORY_RESOLUTION (VSM_MEMORY_RESOLUTION / VSM_PAGE_SIZE)
// How many pixels in debug texture does a single page table entry span
// for example a value of 4 means a single page entry will span 4x4 pixels in the debug texture
#define VSM_DEBUG_PAGING_TABLE_SCALE 16
#define VSM_DEBUG_DRAWN_CLIP_LEVELS 1
#if __cplusplus
static constexpr daxa_i32 pow(daxa_i32 base, daxa_i32 exponent)
{
    if(exponent == 0) return 1;
    daxa_i32 result = base;
    for(daxa_i32 i = 1; i < exponent; i++)
    {
        result *= base;
    }
    return result;
}
static constexpr daxa_u32 vsm_debug_paging_table_resolution()
{
    // return pow(2, VSM_DEBUG_DRAWN_CLIP_LEVELS - 1) * VSM_PAGE_TABLE_RESOLUTION * VSM_DEBUG_PAGING_TABLE_SCALE;
    return VSM_PAGE_TABLE_RESOLUTION * VSM_DEBUG_PAGING_TABLE_SCALE;
}
#endif // cplusplus
#define VSM_DEBUG_VIZ_PASS 1

// How many pixels in debug texture does a single page table entry span
// for example a value of 4 means a single page entry will span 4x4 pixels in the debug texture
#define VSM_DEBUG_META_MEMORY_SCALE 3
#define VSM_DEBUG_META_MEMORY_RESOLUTION (VSM_META_MEMORY_RESOLUTION * VSM_DEBUG_META_MEMORY_SCALE)

#define MAX_NUM_VSM_ALLOC_REQUEST 256
// #define MAX_NUM_VSM_ALLOC_REQUEST 1

#define VSM_FIND_FREE_PAGES_LOCAL_SIZE_X 32
#define VSM_CLEAR_PAGES_LOCAL_SIZE_XY 16
#define VSM_CLEAR_DIRTY_BIT_LOCAL_SIZE_X 32
#define VSM_ALLOCATE_PAGES_LOCAL_SIZE_X 32
#ifdef __cplusplus
static_assert((VSM_PAGE_SIZE % VSM_CLEAR_PAGES_LOCAL_SIZE_XY) == 0,
    "Clear pages pass is written in a way that it requires the page size to be a multiple \
     of 16, either align the page size or change the code to account for this");
#endif //__cplusplus

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
    daxa_u32 frame_index;

    // =============== Atmosphere =====================
    daxa_f32 sun_brightness;
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
    daxa_b32 use_debug_camera;
    daxa_b32 control_main_camera;
    daxa_b32 force_view_clip_level;

    daxa_f32vec3 camera_position;
    daxa_i32vec3 offset;
    daxa_f32mat4x4 view;
    daxa_f32mat4x4 projection;
    daxa_f32mat4x4 inv_projection;
    daxa_f32mat4x4 inv_view_projection;

    daxa_f32vec3 secondary_camera_position;
    daxa_i32vec3 secondary_offset;
    daxa_f32mat4x4 secondary_view;
    daxa_f32mat4x4 secondary_projection;
    daxa_f32mat4x4 secondary_inv_view_projection;

    daxa_f32vec3 camera_front;
    daxa_f32vec3 camera_frust_top_offset;
    daxa_f32vec3 camera_frust_right_offset;


    // =============== Terrrain ======================
    daxa_f32vec2 terrain_scale;
    daxa_f32 terrain_midpoint;
    daxa_f32 terrain_height_scale;
    daxa_f32 terrain_delta;
    daxa_f32 terrain_min_depth;
    daxa_f32 terrain_max_depth;
    daxa_i32 terrain_min_tess_level;
    daxa_i32 terrain_max_tess_level;

    // ================ Shadows ======================
    daxa_f32 lambda;
    daxa_i32vec3 vsm_sun_offset;
    daxa_f32 vsm_clip0_texel_world_size;
    daxa_i32 vsm_debug_clip_level;

    // ================ Post process =================
    daxa_f32 min_luminance_log2;
    daxa_f32 max_luminance_log2;
    daxa_f32 inv_luminance_range_log2;
};

DAXA_DECL_BUFFER_PTR(Globals)

struct TerrainVertex
{
    daxa_f32vec2 position;
};
DAXA_DECL_BUFFER_PTR(TerrainVertex)

struct TerrainIndex
{
    daxa_u32 index;
};
DAXA_DECL_BUFFER_PTR(TerrainIndex)

struct DepthLimits
{
    daxa_f32vec2 limits;
};
DAXA_DECL_BUFFER_PTR(DepthLimits)

struct ShadowmapCascadeData
{
    daxa_f32mat4x4 cascade_view_matrix;
    daxa_f32mat4x4 cascade_proj_matrix;
    daxa_f32 cascade_far_depth;
    daxa_f32 far_plane;
};
DAXA_DECL_BUFFER_PTR(ShadowmapCascadeData)

// ============== DEBUG FRUSTUM DRAW ==================
struct FrustumIndex
{
    daxa_u32 index;
};
DAXA_DECL_BUFFER_PTR(FrustumIndex)

struct FrustumVertex
{
    daxa_f32vec3 vertex;
};
DAXA_DECL_BUFFER_PTR(FrustumVertex)

struct FrustumColor
{
    daxa_f32vec3 color;
};
DAXA_DECL_BUFFER_PTR(FrustumColor)

struct DrawIndexedIndirectStruct
{
    daxa_u32 index_count;
    daxa_u32 instance_count;
    daxa_u32 first_index;
    daxa_u32 vertex_offset;
    daxa_u32 first_instance;
};
DAXA_DECL_BUFFER_PTR(DrawIndexedIndirectStruct)

struct DispatchIndirectStruct
{
    daxa_u32 x;
    daxa_u32 y;
    daxa_u32 z;
};
DAXA_DECL_BUFFER_PTR(DispatchIndirectStruct)

// ============== POST PROCESS ======================
struct Histogram
{
    daxa_u32 bin_count;
};
DAXA_DECL_BUFFER_PTR(Histogram)

struct AverageLuminance
{
    daxa_f32 luminance;
};
DAXA_DECL_BUFFER_PTR(AverageLuminance)

// =============== VSM ===============================
struct VSMClipProjection
{
    daxa_f32vec2 depth_page_offset;
    daxa_i32vec2 page_offset;
    daxa_i32vec3 offset;
    daxa_f32mat4x4 projection_view;
    daxa_f32mat4x4 inv_projection_view;
};
DAXA_DECL_BUFFER_PTR(VSMClipProjection)
struct AllocationCount
{
    daxa_u32 count;
};
DAXA_DECL_BUFFER_PTR(AllocationCount)
struct AllocationRequest
{
    daxa_i32vec3 coords;
};
DAXA_DECL_BUFFER_PTR(AllocationRequest)

struct PageCoordBuffer
{
    daxa_i32vec2 coords;
};
DAXA_DECL_BUFFER_PTR(PageCoordBuffer)

struct FindFreePagesHeader
{
    daxa_u32 free_buffer_counter;
    daxa_u32 not_visited_buffer_counter;
};
DAXA_DECL_BUFFER_PTR(FindFreePagesHeader)

struct FreeWrappedPagesInfo
{
    daxa_i32vec2 clear_offset;
};
DAXA_DECL_BUFFER_PTR(FreeWrappedPagesInfo)