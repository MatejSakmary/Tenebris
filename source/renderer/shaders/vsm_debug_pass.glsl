#define DAXA_ENABLE_IMAGE_OVERLOADS_BASIC 1
#include <shared/shared.inl>
#include "vsm_common.glsl"
#include "tasks/vsm_debug_pass.inl"

layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

#if defined(VSM_DEBUG_PAGE_TABLE)

daxa_i32 get_offset_from_clip_level(daxa_i32 clip_level)
{
    const daxa_i32 num_clip0_tiles = daxa_i32(pow(2, VSM_DEBUG_DRAWN_CLIP_LEVELS - 1));
    const daxa_i32 this_clip_tiles = daxa_i32(pow(2, clip_level));
    const daxa_f32 offset_tiles = daxa_f32(num_clip0_tiles - this_clip_tiles) / 2.0;
    return daxa_i32(offset_tiles * VSM_PAGE_TABLE_RESOLUTION * VSM_DEBUG_PAGING_TABLE_SCALE);
}

void main()
{
    const daxa_i32 clip_level = deref(_globals).vsm_debug_clip_level;
    const daxa_i32vec3 page_entry_coords = daxa_i32vec3(gl_GlobalInvocationID.xy, clip_level);
    const daxa_u32 page_entry = imageLoad(daxa_uimage2DArray(_vsm_page_table), page_entry_coords).r;
    daxa_f32vec4 color = daxa_f32vec4(0.0, 0.0, 0.0, 1.0);

    if      (get_requests_allocation(page_entry)) { color = daxa_f32vec4(0.0, 0.0, 1.0, 1.0); }
    else if (get_is_allocated(page_entry))        { color = daxa_f32vec4(0.0, 1.0, 0.0, 1.0); }
    else if (get_allocation_failed(page_entry))   { color = daxa_f32vec4(1.0, 0.0, 0.0, 1.0); }
    else if (get_is_dirty(page_entry))            { color = daxa_f32vec4(0.0, 0.0, 1.0, 1.0); }

    if (get_is_visited_marked(page_entry))
    { 
        color.xyz = clip_to_color[clip_level % NUM_CLIP_VIZ_COLORS];
    }
    if(color.x == 0 && color.y == 0 && color.z == 0) { return; }
    // if(deref(_globals).vsm_debug_clip_level != clip_level) { continue; }

    const daxa_i32vec2 base_pix_pos = daxa_i32vec2(gl_GlobalInvocationID.xy);
    // const daxa_i32 clip_scale = daxa_i32(pow(2, clip_level));
    // const daxa_i32vec2 debug_page_coords = base_pix_pos * clip_scale * VSM_DEBUG_PAGING_TABLE_SCALE;
    // const daxa_i32vec2 offset_debug_page_coords = debug_page_coords + get_offset_from_clip_level(clip_level);
    // const daxa_i32 debug_page_square = daxa_i32(VSM_DEBUG_PAGING_TABLE_SCALE * pow(2, clip_level));

    const daxa_i32vec2 offset_debug_page_coords = base_pix_pos * VSM_DEBUG_PAGING_TABLE_SCALE;
    const daxa_i32 debug_page_square = daxa_i32(VSM_DEBUG_PAGING_TABLE_SCALE);

    for(daxa_i32 x = 0; x < debug_page_square; x++)
    {
        for(daxa_i32 y = 0; y < debug_page_square; y++)
        {
            const daxa_i32vec2 shaded_pix_pos = daxa_i32vec2(offset_debug_page_coords.x + x, offset_debug_page_coords.y + y);
            const daxa_f32vec3 previous_color = imageLoad(daxa_image2D(_vsm_debug_page_table), shaded_pix_pos).rgb;
            // if(!(previous_color.r > 0.0 && previous_color.g > 0.0))
            // {
            imageStore(daxa_image2D(_vsm_debug_page_table), shaded_pix_pos, color);
            // }
        }
    }
}
#elif defined(VSM_DEBUG_META_MEMORY_TABLE)
void main()
{
    const daxa_u32 meta_entry = imageLoad(daxa_uimage2D(_vsm_meta_memory_table), daxa_i32vec2(gl_GlobalInvocationID.xy)).r;
    const daxa_i32vec2 base_pix_pos = daxa_i32vec2(gl_GlobalInvocationID.xy * VSM_DEBUG_META_MEMORY_SCALE);
    daxa_f32vec4 color = daxa_f32vec4(0.0, 0.0, 0.0, 1.0);

    if (get_meta_memory_is_allocated(meta_entry)) { color = daxa_f32vec4(0.0, 1.0, 0.0, 1.0); }

    if (get_meta_memory_is_visited(meta_entry))
    { 
        color = daxa_f32vec4(1.0, 1.0, 0.0, 1.0);
        const daxa_u32 is_visited_erased_entry = meta_entry & (~meta_memory_visited_mask()); 
        imageStore(daxa_uimage2D(_vsm_meta_memory_table), daxa_i32vec2(gl_GlobalInvocationID.xy), daxa_u32vec4(is_visited_erased_entry));

        const daxa_i32vec3 vsm_coords = get_vsm_coords_from_meta_entry(meta_entry);
        const daxa_u32 vsm_entry = imageLoad(daxa_uimage2DArray(_vsm_page_table_mem_pass), vsm_coords).r;
        const daxa_u32 visited_reset_vsm_entry = vsm_entry & (~(visited_marked_mask()));
        imageStore(daxa_uimage2DArray(_vsm_page_table_mem_pass), vsm_coords, daxa_u32vec4(visited_reset_vsm_entry));
    }
    if(get_meta_memory_needs_clear(meta_entry)) { color += daxa_f32vec4(0.0, 0.0, 1.0, 1.0); }

    if(color.w != 0.0)
    {
        for(daxa_i32 x = 0; x < VSM_DEBUG_META_MEMORY_SCALE; x++)
        {
            for(daxa_i32 y = 0; y < VSM_DEBUG_META_MEMORY_SCALE; y++)
            {
                const daxa_i32vec2 shaded_pix_pos = daxa_i32vec2(base_pix_pos.x + x, base_pix_pos.y + y);
                imageStore(daxa_image2D(_vsm_debug_meta_memory_table), shaded_pix_pos, color);
            }
        }
    }
}
#endif // VSM_DEBUG_META_MEMORY_TABLE