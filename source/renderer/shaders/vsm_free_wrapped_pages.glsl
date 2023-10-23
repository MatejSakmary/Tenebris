#define DAXA_ENABLE_IMAGE_OVERLOADS_BASIC 1
#include <shared/shared.inl>
#include "tasks/vsm_free_wrapped_pages.inl"
#define VSM_WRAPPING_FUNCTIONS 1
#include "vsm_common.glsl"

#extension GL_EXT_debug_printf : enable

layout (local_size_x = VSM_PAGE_TABLE_RESOLUTION) in;
void main()
{
    const daxa_i32vec2 clear_offset = deref(_free_wrapped_pages_info[gl_GlobalInvocationID.z]).clear_offset;
    const daxa_i32vec3 vsm_page_coords = daxa_i32vec3(gl_LocalInvocationID.x, gl_GlobalInvocationID.y, gl_GlobalInvocationID.z);
    if(vsm_page_coords.x > VSM_PAGE_TABLE_RESOLUTION) { return; }

    const bool should_clear = 
        (clear_offset.x > 0 && vsm_page_coords.x <  clear_offset.x) || 
        (clear_offset.x < 0 && vsm_page_coords.x >  VSM_PAGE_TABLE_RESOLUTION + (clear_offset.x - 1)) || 
        (clear_offset.y > 0 && vsm_page_coords.y <  clear_offset.y) || 
        (clear_offset.y < 0 && vsm_page_coords.y >  VSM_PAGE_TABLE_RESOLUTION + (clear_offset.y - 1));

    const daxa_i32vec3 vsm_wrapped_page_coords = vsm_page_coords_to_wrapped_coords(vsm_page_coords);

    if(should_clear)
    {
        const daxa_u32 vsm_page_entry = imageLoad(daxa_uimage2DArray(_vsm_page_table), vsm_wrapped_page_coords).r;
        if(get_is_allocated(vsm_page_entry))
        {
            const daxa_i32vec2 meta_memory_coords = get_meta_coords_from_vsm_entry(vsm_page_entry);
            imageStore(daxa_uimage2D(_vsm_meta_memory_table), meta_memory_coords, daxa_u32vec4(0));
            imageStore(daxa_uimage2DArray(_vsm_page_table), vsm_wrapped_page_coords, daxa_u32vec4(0));
        } 
    }
}