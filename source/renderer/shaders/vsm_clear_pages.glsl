#define DAXA_ENABLE_IMAGE_OVERLOADS_BASIC 1
#include <shared/shared.inl>
#include "vsm_common.glsl"
#include "tasks/vsm_clear_pages.inl"

#extension GL_KHR_shader_subgroup_arithmetic : enable
#extension GL_EXT_debug_printf : enable

layout (local_size_x = VSM_CLEAR_PAGES_LOCAL_SIZE_XY, local_size_y = VSM_CLEAR_PAGES_LOCAL_SIZE_XY) in;
void main()
{
    daxa_u32 vsm_page_entry;
    if(gl_SubgroupInvocationID == 0)
    {
        const daxa_i32vec3 alloc_request_page_coords = deref(_vsm_allocation_buffer[gl_GlobalInvocationID.z]).coords;
        vsm_page_entry = imageLoad(daxa_uimage2DArray(_vsm_page_table), alloc_request_page_coords).r;
        const daxa_u32 vsm_page_entry_marked_dirty = vsm_page_entry | dirty_mask();
        imageStore(daxa_uimage2DArray(_vsm_page_table), alloc_request_page_coords, daxa_u32vec4(vsm_page_entry_marked_dirty));
    }
    vsm_page_entry = subgroupBroadcast(vsm_page_entry, 0);
    if(!get_is_allocated(vsm_page_entry)) { return; }

    const daxa_i32vec2 memory_page_coords = get_meta_coords_from_vsm_entry(vsm_page_entry);
    const daxa_i32vec2 in_memory_corner_coords = memory_page_coords * VSM_PAGE_SIZE;
    const daxa_i32vec2 in_memory_workgroup_offset = daxa_i32vec2(gl_WorkGroupID.xy) * VSM_CLEAR_PAGES_LOCAL_SIZE_XY;
    const daxa_i32vec2 thread_memory_coords = in_memory_corner_coords + in_memory_workgroup_offset + daxa_i32vec2(gl_LocalInvocationID.xy);
    imageStore(daxa_image2D(_vsm_memory), thread_memory_coords, daxa_f32vec4(1.0));
}