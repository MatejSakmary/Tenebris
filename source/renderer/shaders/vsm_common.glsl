#define DAXA_ENABLE_SHADER_NO_NAMESPACE 1
#include <shared/shared.inl>

u32 n_mask(u32 count) { return ((1 << count) - 1); }

// BIT 31 -> 0 - FREE/ 1 - ALLOCATED
// BIT 30 -> 1 - REQUESTS_ALLOCATION
// BIT 29 -> 1 - ALLOCATION_FAILED
// BIT 28 -> 1 - DIRTY
// BIT 27 -> 1 - VISITED_MARKED

// VSM PAGE TABLE MASKS AND FUNCTIONS
u32 allocated_mask()           { return 1 << 31; }
u32 requests_allocation_mask() { return 1 << 30; }
u32 allocation_failed_mask()   { return 1 << 29; }
u32 dirty_mask()               { return 1 << 28; }
u32 visited_marked_mask()      { return 1 << 27; }

bool get_is_allocated(u32 page_entry)        { return (page_entry & allocated_mask()) != 0; }
bool get_requests_allocation(u32 page_entry) { return (page_entry & requests_allocation_mask()) != 0; }
bool get_allocation_failed(u32 page_entry)   { return (page_entry & allocation_failed_mask()) != 0; }
bool get_is_dirty(u32 page_entry)            { return (page_entry & dirty_mask()) != 0; }
bool get_is_visited_marked(u32 page_entry)   { return (page_entry & visited_marked_mask()) != 0; }

// BIT 0 - 7  page entry x coord
// BIT 8 - 15 page entry y coord
i32vec2 get_meta_coords_from_vsm_entry(u32 page_entry)
{
    i32vec2 vsm_page_coordinates = i32vec2(0,0);
    vsm_page_coordinates.x = i32((page_entry >> 0) & n_mask(8));
    vsm_page_coordinates.y = i32((page_entry >> 8) & n_mask(8));

    return vsm_page_coordinates;
}

u32 pack_meta_coords_to_vsm_entry(i32vec2 coords)
{
    u32 packed_coords = 0;
    packed_coords |= (coords.y << 8);
    packed_coords |= (coords.x & n_mask(8));
    return packed_coords;
}

// VSM MEMORY META MASKS AND FUNCTIONS
// BIT 31 -> 0 - FREE/ 1 - ALLOCATED
// BIT 30 -> 1 - NEEDS_CLEAR
// BIT 29 -> 1 - VISITED
u32 meta_memory_allocated_mask()   { return 1 << 31; }
u32 meta_memory_needs_clear_mask() { return 1 << 30; }
u32 meta_memory_visited_mask()     { return 1 << 29; }

bool get_meta_memory_is_allocated(u32 meta_page_entry){ return (meta_page_entry & meta_memory_allocated_mask()) != 0; }
bool get_meta_memory_needs_clear(u32 meta_page_entry) { return (meta_page_entry & meta_memory_needs_clear_mask()) != 0; }
bool get_meta_memory_is_visited(u32 meta_page_entry)  { return (meta_page_entry & meta_memory_visited_mask()) != 0; }

// BIT 0 - 7  page entry x coord
// BIT 8 - 15 page entry y coord
i32vec2 get_vsm_coords_from_meta_entry(u32 page_entry)
{
    i32vec2 physical_coordinates = i32vec2(0,0);
    physical_coordinates.x = i32((page_entry >> 0) & n_mask(8));
    physical_coordinates.y = i32((page_entry >> 8) & n_mask(8));

    return physical_coordinates;
}

u32 pack_vsm_coords_to_meta_entry(i32vec2 coords)
{
    u32 packed_coords = 0;
    packed_coords |= (coords.y << 8);
    packed_coords |= (coords.x & n_mask(8));
    return packed_coords;
}