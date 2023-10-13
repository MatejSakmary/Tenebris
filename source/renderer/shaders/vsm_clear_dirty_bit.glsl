#define DAXA_ENABLE_SHADER_NO_NAMESPACE 1
#define DAXA_ENABLE_IMAGE_OVERLOADS_BASIC 1

#include <shared/shared.inl>
#include "vsm_common.glsl"
#include "tasks/vsm_clear_dirty_bit.inl"

layout (local_size_x = VSM_CLEAR_DIRTY_BIT_LOCAL_SIZE_X) in;
void main()
{
    const i32 id = i32((gl_GlobalInvocationID.z * VSM_CLEAR_DIRTY_BIT_LOCAL_SIZE_X) + gl_LocalInvocationID.x);
    if(id >= deref(_vsm_allocation_count).count) {return;}

    const i32vec3 alloc_request_page_coords = deref(_vsm_allocation_buffer[id]).coords;
    const u32 vsm_page_entry = imageLoad(daxa_uimage2DArray(_vsm_page_table), alloc_request_page_coords).r;
    const u32 dirty_bit_reset_page_entry = vsm_page_entry & ~(dirty_mask());
    imageStore(daxa_uimage2DArray(_vsm_page_table), alloc_request_page_coords, u32vec4(dirty_bit_reset_page_entry));
}