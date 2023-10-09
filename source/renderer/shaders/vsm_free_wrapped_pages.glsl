#define DAXA_ENABLE_SHADER_NO_NAMESPACE 1
#define DAXA_ENABLE_IMAGE_OVERLOADS_BASIC 1
#include <shared/shared.inl>
#include "tasks/vsm_free_wrapped_pages.inl"
#define VSM_WRAPPING_FUNCTIONS 1
#include "vsm_common.glsl"

#extension GL_EXT_debug_printf : enable

layout (local_size_x = VSM_PAGE_TABLE_RESOLUTION) in;
void main()
{
    const i32vec2 clear_offset = deref(_free_wrapped_pages_info[gl_GlobalInvocationID.z]).clear_offset;
    const i32vec3 vsm_page_coords = i32vec3(gl_LocalInvocationID.x, gl_GlobalInvocationID.y, gl_GlobalInvocationID.z);

    const bool should_clear = 
        (clear_offset.x > 0 && vsm_page_coords.x <  clear_offset.x) || 
        (clear_offset.x < 0 && vsm_page_coords.x >  VSM_PAGE_TABLE_RESOLUTION + (clear_offset.x - 1)) || 
        (clear_offset.y > 0 && vsm_page_coords.y <  clear_offset.y) || 
        (clear_offset.y < 0 && vsm_page_coords.y >  VSM_PAGE_TABLE_RESOLUTION + (clear_offset.y - 1));

    const i32vec3 vsm_wrapped_page_coords = vsm_page_coords_to_wrapped_coords(vsm_page_coords);

    if(should_clear)
    {
        const u32 vsm_page_entry = imageLoad(daxa_uimage2DArray(_vsm_page_table), vsm_wrapped_page_coords).r;
        imageStore(daxa_uimage2DArray(_vsm_page_table), vsm_wrapped_page_coords, u32vec4(dirty_mask()));
        if(get_is_allocated(vsm_page_entry))
        {
            const i32vec2 meta_memory_coords = get_meta_coords_from_vsm_entry(vsm_page_entry);
            imageStore(daxa_uimage2D(_vsm_meta_memory_table), meta_memory_coords, u32vec4(0));
            // imageStore(daxa_uimage2DArray(_vsm_page_table), vsm_wrapped_page_coords, u32vec4(dirty_mask()));
        }
    }
}