#define DAXA_ENABLE_SHADER_NO_NAMESPACE 1
#define DAXA_ENABLE_IMAGE_OVERLOADS_BASIC 1
#include <shared/shared.inl>
#include "vsm_common.glsl"
#include "tasks/vsm_debug_pass.inl"

layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

#if defined(VSM_DEBUG_PAGE_TABLE)
void main()
{
    const u32 page_entry = imageLoad(daxa_uimage2D(_vsm_page_table), i32vec2(gl_GlobalInvocationID.xy)).r;
    const i32vec2 base_pix_pos = i32vec2(gl_GlobalInvocationID.xy * VSM_DEBUG_PAGING_TABLE_SCALE);
    f32vec4 color = f32vec4(0.0, 0.0, 0.0, 1.0);

    if      (get_requests_allocation(page_entry)) { color = f32vec4(0.0, 0.0, 1.0, 1.0); }
    else if (get_is_allocated(page_entry))        { color = f32vec4(0.0, 1.0, 0.0, 1.0); }
    else if (get_allocation_failed(page_entry))   { color = f32vec4(1.0, 0.0, 0.0, 1.0); }

    if (get_is_visited_marked(page_entry))
    { 
        color = f32vec4(1.0, 1.0, 0.0, 1.0);
        const u32 is_visited_marked_erased_entry = page_entry & (~visited_marked_mask()); 
        imageStore(
            daxa_uimage2D(_vsm_page_table),
            i32vec2(gl_GlobalInvocationID.xy),
            u32vec4(is_visited_marked_erased_entry)
        );
    }

    for(i32 x = 0; x < VSM_DEBUG_PAGING_TABLE_SCALE; x++)
    {
        for(i32 y = 0; y < VSM_DEBUG_PAGING_TABLE_SCALE; y++)
        {
            const i32vec2 shaded_pix_pos = i32vec2(base_pix_pos.x + x, base_pix_pos.y + y);
            imageStore(daxa_image2D(_vsm_debug_page_table), shaded_pix_pos, color);
        }
    }
}
#elif defined(VSM_DEBUG_META_MEMORY_TABLE)
void main()
{
    const u32 meta_entry = imageLoad(daxa_uimage2D(_vsm_meta_memory_table), i32vec2(gl_GlobalInvocationID.xy)).r;
    const i32vec2 base_pix_pos = i32vec2(gl_GlobalInvocationID.xy * VSM_DEBUG_META_MEMORY_SCALE);
    f32vec4 color = f32vec4(0.0, 0.0, 0.0, 0.0);

    if (get_meta_memory_is_allocated(meta_entry)) { color = f32vec4(0.0, 1.0, 0.0, 1.0); }

    if (get_meta_memory_is_visited(meta_entry))
    { 
        color = f32vec4(1.0, 1.0, 0.0, 1.0);
        const u32 is_visited_erased_entry = meta_entry & (~meta_memory_visited_mask()); 
        imageStore(
            daxa_uimage2D(_vsm_meta_memory_table),
            i32vec2(gl_GlobalInvocationID.xy),
            u32vec4(is_visited_erased_entry)
        );
    }

    if(color.w != 0.0)
    {
        for(i32 x = 0; x < VSM_DEBUG_META_MEMORY_SCALE; x++)
        {
            for(i32 y = 0; y < VSM_DEBUG_META_MEMORY_SCALE; y++)
            {
                const i32vec2 shaded_pix_pos = i32vec2(base_pix_pos.x + x, base_pix_pos.y + y);
                imageStore(daxa_image2D(_vsm_debug_meta_memory_table), shaded_pix_pos, color);
            }
        }
    }
}
#endif // VSM_DEBUG_META_MEMORY_TABLE