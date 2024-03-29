#define DAXA_ENABLE_IMAGE_OVERLOADS_BASIC 1
#include <shared/shared.inl>
#include "tasks/analyze_depthbuffer.inl"
#define VSM_CLIP_INFO_FUNCTIONS 1
#include "vsm_common.glsl"

#extension GL_KHR_shader_subgroup_arithmetic : enable
#extension GL_EXT_debug_printf : enable

DAXA_DECL_IMAGE_ACCESSOR_WITH_FORMAT(uimage2DArray, r32ui, , r32uiImageArray)
DAXA_DECL_IMAGE_ACCESSOR_WITH_FORMAT(uimage2D, r32ui, , r32uiImage)

#define _tile_size 32

layout (local_size_x = _tile_size, local_size_y = _tile_size) in;

DAXA_DECL_PUSH_CONSTANT(AnalyzeDepthPC, pc)

// If subgroup size is less than 32 this will be too small (32 here represents subgroup size)
shared daxa_f32vec2 min_max_depth [_tile_size * _tile_size / 32];

// Calculate the min/max depth values for the entire workgroup and store them in the global buffer
void workgroup_min_max_depth(daxa_f32vec2 thread_min_max_depth)
{
    daxa_f32 min_depth = subgroupMin(thread_min_max_depth.x);
    daxa_f32 max_depth = subgroupMax(thread_min_max_depth.y);

    if(subgroupElect()) { min_max_depth[gl_SubgroupID] = daxa_f32vec2(min_depth, max_depth); }
    memoryBarrierShared();
    barrier();

    // only the first subgroup will now read the values from shared memory and again do subgroup min
    if(gl_SubgroupID == 0) 
    {
        // cutoff in case warp is bigger than 32 threads
        daxa_u32 cutoff = _tile_size * _tile_size / gl_SubgroupSize;
        if(gl_SubgroupInvocationID < cutoff) 
        { 
            daxa_f32vec2 min_max_depth_value = min_max_depth[gl_SubgroupInvocationID];
            min_depth = subgroupMin(min_max_depth_value.x);
            max_depth = subgroupMax(min_max_depth_value.y);

            daxa_u32 global_array_index = gl_WorkGroupID.y * gl_NumWorkGroups.x + gl_WorkGroupID.x;

            // only the 0th thread in 0th warp writes out the min for the entire workgroup
            if(subgroupElect()) 
            {
                deref(_depth_limits[global_array_index]).limits = daxa_f32vec2(min_depth, max_depth);
            }
        }
    }
}

// For each fragment check if the page that will be needed during the shadowmap test is allocated
// if not mark page as needing allocation
void request_vsm_pages(daxa_f32vec4 depths, daxa_u32vec2 scaled_pixel_coords)
{
    // texel gather component mapping - (00,w);(01,x);(11,y);(10,z) 
    // where the 00 is top left
    const daxa_f32vec2 offsets[4] = daxa_f32vec2[](
        daxa_f32vec2(0.5, 1.5),
        daxa_f32vec2(1.5, 1.5),
        daxa_f32vec2(1.5, 0.5),
        daxa_f32vec2(0.5, 0.5)
    );

    for(daxa_i32 idx = 0; idx < 4; idx++)
    {
        // Skip fragments into which no objects were rendered
        if(depths[idx] == 0.0) { continue; }

        const bool use_secondary_camera = deref(_globals).use_debug_camera;
        // We always want to use the main camera - if debug pass is active main camera info is stored in secondary camera
        const daxa_f32mat4x4 inv_projection_view = use_secondary_camera ? deref(_globals).secondary_inv_view_projection : deref(_globals).inv_view_projection;
        const daxa_i32vec3 camera_offset = use_secondary_camera ? deref(_globals).secondary_offset : deref(_globals).offset;
        const daxa_f32vec2 screen_space_uv = (scaled_pixel_coords + offsets[idx]) / daxa_f32vec2(pc.depth_dimensions);

        ClipInfo clip_info = clip_info_from_uvs(ClipFromUVsInfo(
            screen_space_uv,
            pc.depth_dimensions,
            depths[idx],
            inv_projection_view,
            camera_offset,
            -1
        ));
        if(clip_info.clip_level >= VSM_CLIP_LEVELS) { continue; }

        const daxa_i32vec3 vsm_page_wrapped_coords = vsm_clip_info_to_wrapped_coords(clip_info);
        if(vsm_page_wrapped_coords.x < 0 || vsm_page_wrapped_coords.y < 0) { continue; }
        const daxa_u32 page_entry = imageLoad(daxa_uimage2DArray(_vsm_page_table), vsm_page_wrapped_coords).r;

        const bool is_not_allocated = !get_is_allocated(page_entry);
        const bool allocation_available = atomicAdd(deref(_vsm_allocation_count).count, 0) < MAX_NUM_VSM_ALLOC_REQUEST;

        if(is_not_allocated && allocation_available)
        {
            const daxa_u32 prev_state = imageAtomicOr(
                daxa_access(r32uiImageArray, _vsm_page_table),
                vsm_page_wrapped_coords,
                requests_allocation_mask()
            );

            if(!get_requests_allocation(prev_state))
            {
                // If this is the thread to mark this page as REQUESTS_ALLOCATION
                //    -> create a new allocation request in the allocation buffer
                daxa_u32 idx = atomicAdd(deref(_vsm_allocation_count).count, 1);
                if(idx < MAX_NUM_VSM_ALLOC_REQUEST)
                {
                    deref(_vsm_allocation_buffer[idx]) = AllocationRequest(vsm_page_wrapped_coords);
                } 
                else 
                {
                    atomicAdd(deref(_vsm_allocation_count).count, -1);
                    imageAtomicAnd(
                        daxa_access(r32uiImageArray, _vsm_page_table),
                        vsm_page_wrapped_coords,
                        ~requests_allocation_mask()
                    );
                }
            } 
        } 
        else if (!get_is_visited_marked(page_entry) && !is_not_allocated)
        {
            const daxa_u32 prev_state = imageAtomicOr(
                daxa_access(r32uiImageArray, _vsm_page_table),
                vsm_page_wrapped_coords,
                visited_marked_mask()
            );
            // If this is the first thread to mark this page as VISITED_MARKED 
            //   -> mark the physical page as VISITED
            if(!get_is_visited_marked(prev_state))
            { 
                imageAtomicOr(
                    daxa_access(r32uiImage, _vsm_meta_memory_table),
                    get_meta_coords_from_vsm_entry(page_entry),
                    meta_memory_visited_mask()
                );
            }
        }
    }
}

#if defined(FIRST_PASS)

// min/max depth values for the local thread
daxa_f32vec2 thread_min_max_depth(daxa_f32vec4 read_depth_values)
{
    daxa_f32 thread_min_depth_value = 0.0;
    daxa_f32 thread_max_depth_value = 0.0;

    thread_max_depth_value = max(
        max(read_depth_values.x, read_depth_values.y),
        max(read_depth_values.z, read_depth_values.w)
    );

    read_depth_values.x = read_depth_values.x == 0.0 ? 42.0 : read_depth_values.x;
    read_depth_values.y = read_depth_values.y == 0.0 ? 42.0 : read_depth_values.y;
    read_depth_values.z = read_depth_values.z == 0.0 ? 42.0 : read_depth_values.z;
    read_depth_values.w = read_depth_values.w == 0.0 ? 42.0 : read_depth_values.w;

    thread_min_depth_value = min(
        min(read_depth_values.x, read_depth_values.y),
        min(read_depth_values.z, read_depth_values.w)
    );
    return daxa_f32vec2(thread_min_depth_value, thread_max_depth_value);
}

void main()
{
    // ========================== FIRST PASS =====================================
    //        Writes out min/max value per Workgroup into the global memory
    daxa_u32vec2 subgroup_offset;
    subgroup_offset.x = 0;
    subgroup_offset.y = (gl_SubgroupID * gl_SubgroupSize) / _tile_size;

    daxa_u32vec2 in_subgroup_offset;
    in_subgroup_offset.x = gl_SubgroupInvocationID % _tile_size;
    in_subgroup_offset.y = gl_SubgroupInvocationID / _tile_size;

    const daxa_u32vec2 pixel_coords = _tile_size * gl_WorkGroupID.xy + subgroup_offset + in_subgroup_offset;
    // Each thread reads 2x2 block
    const daxa_u32vec2 scaled_pixel_coords = pixel_coords * 2;
    // Offset into the middle of the 2x2 block so gather gets us the correct texels
    const daxa_u32vec2 scaled_offset_pixel_coords = scaled_pixel_coords + daxa_u32vec2(1, 1);

    daxa_f32vec2 depth_uv = daxa_f32vec2(scaled_offset_pixel_coords) / daxa_f32vec2(pc.depth_dimensions);
    daxa_f32vec4 read_depth_values = textureGather(daxa_sampler2D(_depth, pc.linear_sampler), depth_uv, 0);

    request_vsm_pages(read_depth_values, scaled_pixel_coords);

    daxa_f32vec2 local_min_max_depth = thread_min_max_depth(read_depth_values);
    workgroup_min_max_depth(local_min_max_depth);
}
#else
void main()
{
    // Prepare indirect dispatches for further vsm passes
    if(all(equal(gl_GlobalInvocationID, daxa_u32vec3(0, 0, 0))))
    {
        const daxa_u32 allocations_number = deref(_vsm_allocation_count).count;

        const daxa_u32 allocate_dispach_count = 
            (allocations_number + VSM_ALLOCATE_PAGES_LOCAL_SIZE_X - 1) / VSM_ALLOCATE_PAGES_LOCAL_SIZE_X;
        deref(_vsm_allocate_indirect).x = 1;
        deref(_vsm_allocate_indirect).y = 1;
        deref(_vsm_allocate_indirect).z = allocate_dispach_count;

        deref(_vsm_clear_indirect).x = VSM_PAGE_SIZE / VSM_CLEAR_PAGES_LOCAL_SIZE_XY;
        deref(_vsm_clear_indirect).y = VSM_PAGE_SIZE / VSM_CLEAR_PAGES_LOCAL_SIZE_XY;
        deref(_vsm_clear_indirect).z = deref(_vsm_allocation_count).count;

        const daxa_u32 clear_dirty_bit_distpach_count = 
            (allocations_number + VSM_CLEAR_DIRTY_BIT_LOCAL_SIZE_X - 1) / VSM_CLEAR_DIRTY_BIT_LOCAL_SIZE_X;
        deref(_vsm_clear_dirty_bit_indirect).x = 1;
        deref(_vsm_clear_dirty_bit_indirect).y = 1;
        deref(_vsm_clear_dirty_bit_indirect).z = clear_dirty_bit_distpach_count;

    }
    daxa_u32 work_group_threads = _tile_size * _tile_size;
    daxa_u32 workgroup_offset = gl_WorkGroupID.x * work_group_threads;
    daxa_u32 _subgroup_offset = gl_SubgroupID * gl_SubgroupSize;

    daxa_u32 into_global_index = workgroup_offset + _subgroup_offset + gl_SubgroupInvocationID;
    // Each thread processes 4 values
    into_global_index *= 4; 

    daxa_f32vec4 read_min_depth_values;
    daxa_f32vec4 read_max_depth_values;

    // Read values from global array into local variable
    into_global_index = min(into_global_index, pc.prev_thread_count - 1);
    read_min_depth_values.x = deref(_depth_limits[into_global_index]).limits.x;
    read_max_depth_values.x = deref(_depth_limits[into_global_index]).limits.y;
    into_global_index = min(into_global_index + 1, pc.prev_thread_count - 1);
    read_min_depth_values.y = deref(_depth_limits[into_global_index]).limits.x;
    read_max_depth_values.y = deref(_depth_limits[into_global_index]).limits.y;
    into_global_index = min(into_global_index + 1, pc.prev_thread_count - 1);
    read_min_depth_values.z = deref(_depth_limits[into_global_index]).limits.x;
    read_max_depth_values.z = deref(_depth_limits[into_global_index]).limits.y;
    into_global_index = min(into_global_index + 1, pc.prev_thread_count - 1);
    read_min_depth_values.w = deref(_depth_limits[into_global_index]).limits.x;
    read_max_depth_values.w = deref(_depth_limits[into_global_index]).limits.y;

    daxa_f32vec2 thread_min_max_depth;

    thread_min_max_depth.x = min(
        min(read_min_depth_values.x, read_min_depth_values.y),
        min(read_min_depth_values.z, read_min_depth_values.w)
    );

    thread_min_max_depth.y = max(
        max(read_max_depth_values.x, read_max_depth_values.y),
        max(read_max_depth_values.z, read_max_depth_values.w)
    );

    workgroup_min_max_depth(thread_min_max_depth);
}
#endif
