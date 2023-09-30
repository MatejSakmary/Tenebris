#define DAXA_ENABLE_SHADER_NO_NAMESPACE 1
#define DAXA_ENABLE_IMAGE_OVERLOADS_BASIC 1
#include <shared/shared.inl>
#include "vsm_common.glsl"
#include "tasks/vsm_find_free_pages.inl"

#extension GL_KHR_shader_subgroup_arithmetic : enable
#extension GL_EXT_debug_printf : enable

struct BufferReserveInfo
{
    u32 reserved_count;
    u32 reserved_offset;
    u32 order;
    bool condition;
};

BufferReserveInfo count_pages_and_reserve_buffer_slots(
    bool count_free_pages,
    daxa_BufferPtr(FindFreePagesHeader) header,
    u32 meta_entry
)
{
    bool condition;
    if(count_free_pages) { condition = !get_meta_memory_is_allocated(meta_entry); }
    else                 { condition =  get_meta_memory_is_allocated(meta_entry) && (!get_meta_memory_is_visited(meta_entry)); }

    const u32vec4 condition_mask = subgroupBallot(condition);
    const u32 order = subgroupBallotExclusiveBitCount(condition_mask);

    u32 broadcast_value = 0;
    // Last thread will attempt to allocate all the pages
    if(gl_SubgroupInvocationID == 31)
    {
        // Because we did ExlusiveSum earlier we also need to recheck this threads result
        // as it was not included in the sum
        const u32 page_count = order + i32(condition);

        u32 previous_counter_value;
        if(count_free_pages) { previous_counter_value = atomicAdd(deref(header).free_buffer_counter, page_count); } 
        else                 { previous_counter_value = atomicAdd(deref(header).not_visited_buffer_counter, page_count); }

        u32 reserve_count = 0;
        u32 counter_overflow = 0;
        if(previous_counter_value < MAX_NUM_VSM_ALLOC_REQUEST)
        {
            const u32 counter_capacity = MAX_NUM_VSM_ALLOC_REQUEST - previous_counter_value;
            reserve_count = min(page_count, counter_capacity);
            counter_overflow = page_count - reserve_count;
        } else {
            counter_overflow = page_count;
        }

        // fix the counter if it overflowed
        if(count_free_pages) { atomicAdd(deref(header).free_buffer_counter, -counter_overflow); }
        else                 { atomicAdd(deref(header).not_visited_buffer_counter, -counter_overflow); }

        // Pack reserve data into a single uint so we can use a single broadcast to distribute it
        // MSB 16 - reserved offset
        // LSB 16 - reserved count
        broadcast_value |= previous_counter_value << 16;
        broadcast_value |= (reserve_count & n_mask(16));
    }

    u32 reserved_info = subgroupBroadcast(broadcast_value, 31);
    return BufferReserveInfo(
        reserved_info & n_mask(16), // reserved_count
        reserved_info >> 16,        // reserved_offset
        order,                      // thread order
        condition                   // condition
    );
}

layout (local_size_x = VSM_FIND_FREE_PAGES_LOCAL_SIZE_X, local_size_y = 1, local_size_z = 1) in;
void main()
{
    const i32 linear_thread_index = i32(gl_WorkGroupID.x * VSM_FIND_FREE_PAGES_LOCAL_SIZE_X + gl_LocalInvocationID.x);
    const i32vec2 thread_coords = i32vec2(
        linear_thread_index % VSM_META_MEMORY_RESOLUTION,
        linear_thread_index / VSM_META_MEMORY_RESOLUTION
    );

    const u32 meta_entry = imageLoad(daxa_uimage2D(_vsm_meta_memory_table), thread_coords).r;

    BufferReserveInfo info = count_pages_and_reserve_buffer_slots(true, _vsm_find_free_pages_header, meta_entry);
    bool fits_into_reserved_slots = info.order < info.reserved_count;
    if(info.condition && fits_into_reserved_slots) 
    {
        deref(_vsm_free_pages_buffer[info.reserved_offset + info.order]).coords = thread_coords;
    }

    info = count_pages_and_reserve_buffer_slots(false, _vsm_find_free_pages_header, meta_entry);
    fits_into_reserved_slots = info.order < info.reserved_count;
    if(info.condition && fits_into_reserved_slots) 
    {
        deref(_vsm_not_visited_pages_buffer[info.reserved_offset + info.order]).coords = thread_coords;
    }
// If we are debug drawing vsm meta memory and paging texture we don't want to clear visited here
// The debug pass will clear those flags in that case
#if VSM_DEBUG_VIZ_PASS == 0
    // Second invocation of count_pages_and_reserve_buffer_slots will attempt to find allocated and non-visited pages
    // We want to clear the visited condition from every single page for the next frame
    if(!info.condition)
    {
        imageStore(daxa_uimage2D(_vsm_meta_memory_table), thread_coords, meta_entry & (~meta_memory_visited_mask()));
        const i32vec2 vsm_coords = get_vsm_coords_from_meta_entry(meta_entry);

        const u32 vsm_entry = imageLoad(daxa_uimage2D(_vsm_page_table), vsm_coords).r;
        const u32 visited_reset_vsm_entry = vsm_entry & (~visited_marked_mask());
        imageStore(daxa_uimage2D(_vsm_page_table), vsm_coords, u32vec4(visited_reset_vsm_entry))
    }
#endif // VSM_DEBUG_PASS_CLEARS_VISITED == 0
}