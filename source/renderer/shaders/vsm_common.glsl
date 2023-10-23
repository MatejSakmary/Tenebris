#include <shared/shared.inl>

#define NUM_CLIP_VIZ_COLORS 4
const daxa_f32vec3 clip_to_color[NUM_CLIP_VIZ_COLORS] = daxa_f32vec3[](
    daxa_f32vec3( 26, 172, 172) / 255,
    daxa_f32vec3(255,  75, 145) / 255,
    daxa_f32vec3(255, 118, 118) / 255,
    daxa_f32vec3(255, 205,  75) / 255
);

#if VSM_WRAPPING_FUNCTIONS || VSM_CLIP_INFO_FUNCTIONS
daxa_i32vec3 vsm_page_coords_to_wrapped_coords(daxa_i32vec3 page_coords)
{
    const daxa_i32vec2 vsm_toroidal_offset = deref(_vsm_sun_projections[page_coords.z]).page_offset;
    const daxa_i32vec2 vsm_toroidal_pix_coords = page_coords.xy - vsm_toroidal_offset.xy;
    if( 
        page_coords.x < 0 ||
        page_coords.x > (VSM_PAGE_TABLE_RESOLUTION - 1) ||
        page_coords.y < 0 ||
        page_coords.y > (VSM_PAGE_TABLE_RESOLUTION - 1))
    {
        return daxa_i32vec3(-1, -1, page_coords.z);
    }
    const daxa_i32vec2 vsm_wrapped_pix_coords = daxa_i32vec2(mod(vsm_toroidal_pix_coords.xy, daxa_f32(VSM_PAGE_TABLE_RESOLUTION)));
    return daxa_i32vec3(vsm_wrapped_pix_coords, page_coords.z);
}
#endif //VSM_WRAPPING_FUNCTIONS

#if VSM_CLIP_INFO_FUNCTIONS
struct ClipFromUVsInfo
{
    // Should be UVs of the center of the texel
    daxa_f32vec2 uv;
    daxa_u32vec2 screen_resolution;
    daxa_f32 depth;
    daxa_f32mat4x4 inv_projection_view;
    daxa_i32vec3 camera_offset;
    daxa_i32 force_clip_level;
};

struct ClipInfo
{
    daxa_i32 clip_level;
    daxa_f32vec2 sun_depth_uv;
};


daxa_f32 get_page_offset_depth(ClipInfo info, daxa_f32 current_depth)
{
    const daxa_i32vec2 non_wrapped_page_coords = daxa_i32vec2(info.sun_depth_uv * VSM_PAGE_TABLE_RESOLUTION);
    const daxa_i32vec2 inverted_page_coords = daxa_i32vec2((VSM_PAGE_TABLE_RESOLUTION - 1) - non_wrapped_page_coords);
    const daxa_f32vec2 per_inv_page_depth_offset = deref(_vsm_sun_projections[info.clip_level]).depth_page_offset;
    const daxa_f32 depth_offset = 
        inverted_page_coords.x * per_inv_page_depth_offset.x +
        inverted_page_coords.y * per_inv_page_depth_offset.y;
    return clamp(current_depth + depth_offset, 0.0, 1.0);
}

daxa_f32vec3 camera_offset_world_space_from_uv(daxa_f32vec2 screen_space_uv, daxa_f32 depth, daxa_f32mat4x4 inv_projection_view)
{
    const daxa_f32vec2 remap_uv = (screen_space_uv * 2.0) - 1.0;
    const daxa_f32vec4 ndc_position = daxa_f32vec4(remap_uv, depth, 1.0);
    const daxa_f32vec4 unprojected_ndc_position = inv_projection_view * ndc_position;

    const daxa_f32vec3 offset_world_position = unprojected_ndc_position.xyz / unprojected_ndc_position.w;
    return offset_world_position;
}

ClipInfo clip_info_from_uvs(ClipFromUVsInfo info)
{
    daxa_i32 clip_level;
    if(info.force_clip_level == -1)
    {
        const daxa_f32vec2 center_texel_coords = info.uv * info.screen_resolution;

        const daxa_f32vec2 left_side_texel_coords = center_texel_coords - daxa_f32vec2(0.5, 0.0);
        const daxa_f32vec2 left_side_texel_uvs = left_side_texel_coords / daxa_f32vec2(info.screen_resolution);
        const daxa_f32vec3 camera_offset_left_world_space = camera_offset_world_space_from_uv(
            left_side_texel_uvs,
            info.depth,
            info.inv_projection_view
        );

        const daxa_f32vec2 right_side_texel_coords = center_texel_coords + daxa_f32vec2(0.5, 0.0);
        const daxa_f32vec2 right_side_texel_uvs = right_side_texel_coords / daxa_f32vec2(info.screen_resolution);
        const daxa_f32vec3 camera_offset_right_world_space = camera_offset_world_space_from_uv(
            right_side_texel_uvs,
            info.depth,
            info.inv_projection_view
        );


        const daxa_f32 texel_world_size = length(camera_offset_left_world_space - camera_offset_right_world_space);
        clip_level = clamp(daxa_i32(ceil(log2(texel_world_size / deref(_globals).vsm_clip0_texel_world_size))), 0, VSM_CLIP_LEVELS - 1);
    } 
    else 
    {
        clip_level = info.force_clip_level;
    }

    const daxa_f32vec3 camera_offset_center_world_space = camera_offset_world_space_from_uv(
        info.uv,
        info.depth,
        info.inv_projection_view
    );
    const daxa_i32vec3 camera_to_sun_offset = deref(_vsm_sun_projections[clip_level]).offset - info.camera_offset;
    const daxa_f32vec3 sun_offset_world_position = camera_offset_center_world_space + camera_to_sun_offset;
    const daxa_f32vec4 sun_projected_world_position = 
        deref(_vsm_sun_projections[clip_level]).projection_view * daxa_f32vec4(sun_offset_world_position, 1.0);
    const daxa_f32vec3 sun_ndc_position = sun_projected_world_position.xyz / sun_projected_world_position.w;
    const daxa_f32vec2 sun_depth_uv = (sun_ndc_position.xy + daxa_f32vec2(1.0)) / daxa_f32vec2(2.0);
    return ClipInfo(clip_level, sun_depth_uv);
}

daxa_i32vec3 vsm_clip_info_to_wrapped_coords(ClipInfo info)
{
    const daxa_i32vec3 vsm_page_pix_coords = daxa_i32vec3(floor(info.sun_depth_uv * VSM_PAGE_TABLE_RESOLUTION), info.clip_level);
    return vsm_page_coords_to_wrapped_coords(vsm_page_pix_coords);
}
#endif //VSM_CLIP_INFO_FUNCTIONS

daxa_i32vec2 virtual_uv_to_physical_texel(daxa_f32vec2 virtual_uv, daxa_i32vec2 physical_page_coords)
{
    const daxa_i32vec2 virtual_texel_coord = daxa_i32vec2(virtual_uv * VSM_TEXTURE_RESOLUTION);
    const daxa_i32vec2 in_page_texel_coord = daxa_i32vec2(mod(virtual_texel_coord, daxa_f32(VSM_PAGE_SIZE)));
    const daxa_i32vec2 in_memory_offset = physical_page_coords * VSM_PAGE_SIZE;
    const daxa_i32vec2 memory_texel_coord = in_memory_offset + in_page_texel_coord;
    return memory_texel_coord;
}

daxa_u32 n_mask(daxa_u32 count) { return ((1 << count) - 1); }

// BIT 31 -> 0 - FREE/ 1 - ALLOCATED
// BIT 30 -> 1 - REQUESTS_ALLOCATION
// BIT 29 -> 1 - ALLOCATION_FAILED
// BIT 28 -> 1 - DIRTY
// BIT 27 -> 1 - VISITED_MARKED

// VSM PAGE TABLE MASKS AND FUNCTIONS
daxa_u32 allocated_mask()           { return 1 << 31; }
daxa_u32 requests_allocation_mask() { return 1 << 30; }
daxa_u32 allocation_failed_mask()   { return 1 << 29; }
daxa_u32 dirty_mask()               { return 1 << 28; }
daxa_u32 visited_marked_mask()      { return 1 << 27; }

bool get_is_allocated(daxa_u32 page_entry)        { return (page_entry & allocated_mask()) != 0; }
bool get_requests_allocation(daxa_u32 page_entry) { return (page_entry & requests_allocation_mask()) != 0; }
bool get_allocation_failed(daxa_u32 page_entry)   { return (page_entry & allocation_failed_mask()) != 0; }
bool get_is_dirty(daxa_u32 page_entry)            { return (page_entry & dirty_mask()) != 0; }
bool get_is_visited_marked(daxa_u32 page_entry)   { return (page_entry & visited_marked_mask()) != 0; }

// BIT 0 - 7  page entry x coord
// BIT 8 - 15 page entry y coord
daxa_i32vec2 get_meta_coords_from_vsm_entry(daxa_u32 page_entry)
{
    daxa_i32vec2 vsm_page_coordinates = daxa_i32vec2(0,0);
    vsm_page_coordinates.x = daxa_i32((page_entry >> 0) & n_mask(8));
    vsm_page_coordinates.y = daxa_i32((page_entry >> 8) & n_mask(8));

    return vsm_page_coordinates;
}

daxa_u32 pack_meta_coords_to_vsm_entry(daxa_i32vec2 coords)
{
    daxa_u32 packed_coords = 0;
    packed_coords |= (coords.y << 8);
    packed_coords |= (coords.x & n_mask(8));
    return packed_coords;
}

// VSM MEMORY META MASKS AND FUNCTIONS
// BIT 31 -> 0 - FREE/ 1 - ALLOCATED
// BIT 30 -> 1 - NEEDS_CLEAR
// BIT 29 -> 1 - VISITED
daxa_u32 meta_memory_allocated_mask()   { return 1 << 31; }
daxa_u32 meta_memory_needs_clear_mask() { return 1 << 30; }
daxa_u32 meta_memory_visited_mask()     { return 1 << 29; }

bool get_meta_memory_is_allocated(daxa_u32 meta_page_entry){ return (meta_page_entry & meta_memory_allocated_mask()) != 0; }
bool get_meta_memory_needs_clear(daxa_u32 meta_page_entry) { return (meta_page_entry & meta_memory_needs_clear_mask()) != 0; }
bool get_meta_memory_is_visited(daxa_u32 meta_page_entry)  { return (meta_page_entry & meta_memory_visited_mask()) != 0; }

// BIT 0 - 7   page entry x coord
// BIT 8 - 15  page entry y coord
// BIT 16 - 19 vsm clip level
daxa_i32vec3 get_vsm_coords_from_meta_entry(daxa_u32 page_entry)
{
    daxa_i32vec3 physical_coordinates = daxa_i32vec3(0,0,0);
    physical_coordinates.x = daxa_i32((page_entry >> 0)  & n_mask(8));
    physical_coordinates.y = daxa_i32((page_entry >> 8)  & n_mask(8));
    physical_coordinates.z = daxa_i32((page_entry >> 16) & n_mask(4));

    return physical_coordinates;
}

daxa_u32 pack_vsm_coords_to_meta_entry(daxa_i32vec3 coords)
{
    daxa_u32 packed_coords = 0;
    packed_coords |= ((coords.z & n_mask(4)) << 16);
    packed_coords |= ((coords.y & n_mask(8)) << 8);
    packed_coords |= ((coords.x & n_mask(8)) << 0);
    return packed_coords;
}