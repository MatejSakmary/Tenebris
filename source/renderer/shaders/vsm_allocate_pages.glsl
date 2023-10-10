#define DAXA_ENABLE_SHADER_NO_NAMESPACE 1
#define DAXA_ENABLE_IMAGE_OVERLOADS_BASIC 1
#include <shared/shared.inl>
#include "vsm_common.glsl"
#include "tasks/vsm_allocate_pages.inl"

#extension GL_EXT_debug_printf : enable

layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main()
{
    // Prepare indirect dispatch buffer for clear page pass
    if(gl_GlobalInvocationID.x == 0)
    {
        deref(_vsm_clear_indirect).z = deref(_vsm_allocate_indirect).x;
    }

    // - Read the values stored inside FindFreePages header
    //   - If the GlobalThreadID is less than free_buffer_counter:
    //     - Read the entry in FreePageBuffer[GlobalThreadID]
    //     - Read the entry in AllocationRequest[GlobalThreadID]
    //     - Assign new entries to the page_table_texel and meta_memory_texel
    //   - If the (GlobalThreadID - free_buffer_counter) < not_visited_buffer_counter:
    //     - Read the entry in NotVisitedPageBuffer[GlobalThreadID]
    //     - Read the meta memory entry
    //     - Reset (Deallocate) the entry that previously owned this memory in virtual page table 
    //     - Assign new entries to the page_table_texel and meta_memory_texel
    FindFreePagesHeader header = deref(_vsm_find_free_pages_header);

    const i32 id = i32(gl_GlobalInvocationID.x);
    const i32 free_shifted_id = id - i32(header.free_buffer_counter);

    const i32vec3 alloc_request_page_coords = deref(_vsm_allocation_buffer[id]).coords;

    // Use up all free pages
    if(id < header.free_buffer_counter)
    {
        const i32vec2 free_memory_page_coords = deref(_vsm_free_pages_buffer[id]).coords;
        
        u32 new_vsm_page_entry = pack_meta_coords_to_vsm_entry(free_memory_page_coords);
        new_vsm_page_entry |= allocated_mask();
        imageStore(daxa_uimage2DArray(_vsm_page_table), alloc_request_page_coords, u32vec4(new_vsm_page_entry));

        u32 new_meta_memory_page_entry = pack_vsm_coords_to_meta_entry(alloc_request_page_coords);
        new_meta_memory_page_entry |= meta_memory_allocated_mask();
        imageStore(daxa_uimage2D(_vsm_meta_memory_table), free_memory_page_coords, u32vec4(new_meta_memory_page_entry));
    } 
    // If there is not enough free pages free NOT VISITED pages to make space
    else if (free_shifted_id < header.not_visited_buffer_counter)
    {
        const i32vec2 not_visited_memory_page_coords = deref(_vsm_not_visited_pages_buffer[free_shifted_id]).coords;
        // Reset previously owning vsm page
        const u32 meta_entry = imageLoad(daxa_uimage2D(_vsm_meta_memory_table), not_visited_memory_page_coords).r;
        const i32vec3 owning_vsm_coords = get_vsm_coords_from_meta_entry(meta_entry);
        imageStore(daxa_uimage2DArray(_vsm_page_table), owning_vsm_coords, u32vec4(0));
        
        // Perform the allocation
        u32 new_vsm_page_entry = pack_meta_coords_to_vsm_entry(not_visited_memory_page_coords);
        new_vsm_page_entry |= allocated_mask();
        imageStore(daxa_uimage2DArray(_vsm_page_table), alloc_request_page_coords, u32vec4(new_vsm_page_entry));

        u32 new_meta_memory_page_entry = pack_vsm_coords_to_meta_entry(alloc_request_page_coords);
        new_meta_memory_page_entry |= meta_memory_allocated_mask();
        imageStore(daxa_uimage2D(_vsm_meta_memory_table), not_visited_memory_page_coords, u32vec4(new_meta_memory_page_entry));
    } 
    // Else mark the page as allocation failed
    else 
    {
        imageStore(daxa_uimage2DArray(_vsm_page_table), alloc_request_page_coords, u32vec4(allocation_failed_mask()));
    }
}
    