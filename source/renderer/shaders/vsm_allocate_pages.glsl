#define DAXA_ENABLE_SHADER_NO_NAMESPACE 1
#define DAXA_ENABLE_IMAGE_OVERLOADS_BASIC 1
#include <shared/shared.inl>
#include "vsm_common.glsl"
#include "tasks/vsm_allocate_pages.inl"

#extension GL_EXT_debug_printf : enable

layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main()
{
    const i32vec2 page_coords = deref(_vsm_allocation_buffer[gl_GlobalInvocationID.x]).coords;

    const u32 page_entry = imageLoad(daxa_uimage2D(_vsm_page_table), page_coords).r;
    const u32 allocated_bit_set_page_entry = page_entry | allocated_mask();
    const u32 bits_requests_and_failed_erased_page_entry = 
        allocated_bit_set_page_entry & (~(requests_allocation_mask() | allocation_failed_mask()));

    imageStore(daxa_uimage2D(_vsm_page_table), page_coords, u32vec4(bits_requests_and_failed_erased_page_entry));
}