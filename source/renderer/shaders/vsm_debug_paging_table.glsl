#define DAXA_ENABLE_SHADER_NO_NAMESPACE 1
#define DAXA_ENABLE_IMAGE_OVERLOADS_BASIC 1
#include <shared/shared.inl>
#include "vsm_common.glsl"
#include "tasks/vsm_debug_paging_table.inl"

layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

void main()
{
    const u32 page_entry = imageLoad(daxa_uimage2D(_vsm_page_table), i32vec2(gl_GlobalInvocationID.xy)).r;
    if(needs_allocation(page_entry))
    {
        const i32vec2 base_pix_pos = i32vec2(gl_GlobalInvocationID.xy * VSM_DEBUG_PAGING_TABLE_SCALE);
        for(i32 x = 0; x < VSM_DEBUG_PAGING_TABLE_SCALE; x++)
        {
            for(i32 y = 0; y < VSM_DEBUG_PAGING_TABLE_SCALE; y++)
            {
                const i32vec2 shaded_pix_pos = i32vec2(base_pix_pos.x + x, base_pix_pos.y + y);
                imageStore(
                    daxa_image2D(_vsm_debug_paging_table),
                    shaded_pix_pos,
                    f32vec4(1.0, 0.0, 0.0, 1.0)
                );
            }
        }
    }
}