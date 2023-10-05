#define DAXA_ENABLE_SHADER_NO_NAMESPACE 1
#define DAXA_ENABLE_IMAGE_OVERLOADS_BASIC 1
#include <shared/shared.inl>
#include "vsm_common.glsl"
#include "tasks/analyze_depthbuffer.inl"

#extension GL_KHR_shader_subgroup_arithmetic : enable
#extension GL_EXT_debug_printf : enable

DAXA_DECL_IMAGE_ACCESSOR_WITH_FORMAT(uimage2DArray, r32ui, , r32uiImageArray)
DAXA_DECL_IMAGE_ACCESSOR_WITH_FORMAT(uimage2D, r32ui, , r32uiImage)

#define _tile_size 32

layout (local_size_x = _tile_size, local_size_y = _tile_size) in;

DAXA_DECL_PUSH_CONSTANT(AnalyzeDepthPC, pc)

// If subgroup size is less than 32 this will be too small (32 here represents subgroup size)
shared f32vec2 min_max_depth [_tile_size * _tile_size / 32];

// Calculate the min/max depth values for the entire workgroup and store them in the global buffer
void workgroup_min_max_depth(f32vec2 thread_min_max_depth)
{
    f32 min_depth = subgroupMin(thread_min_max_depth.x);
    f32 max_depth = subgroupMax(thread_min_max_depth.y);

    if(subgroupElect()) { min_max_depth[gl_SubgroupID] = f32vec2(min_depth, max_depth); }
    memoryBarrierShared();
    barrier();

    // only the first subgroup will now read the values from shared memory and again do subgroup min
    if(gl_SubgroupID == 0) 
    {
        // cutoff in case warp is bigger than 32 threads
        u32 cutoff = _tile_size * _tile_size / gl_SubgroupSize;
        if(gl_SubgroupInvocationID < cutoff) 
        { 
            f32vec2 min_max_depth_value = min_max_depth[gl_SubgroupInvocationID];
            min_depth = subgroupMin(min_max_depth_value.x);
            max_depth = subgroupMax(min_max_depth_value.y);

            u32 global_array_index = gl_WorkGroupID.y * gl_NumWorkGroups.x + gl_WorkGroupID.x;

            // only the 0th thread in 0th warp writes out the min for the entire workgroup
            if(subgroupElect()) 
            {
                deref(_depth_limits[global_array_index]).limits = f32vec2(min_depth, max_depth);
            }
        }
    }
}

// Takes in Screen space uvs - in the range [0, 1]
f32vec3 world_space_from_uv(f32vec2 screen_space_uv, f32 depth, f32mat4x4 inv_projection_view)
{
    const f32vec2 remap_uv = (screen_space_uv * 2.0) - 1.0;
    const f32vec4 ndc_position = f32vec4(remap_uv, depth, 1.0);
    const f32vec4 unprojected_ndc_position = inv_projection_view * ndc_position;

    const f32vec3 offset_world_position = unprojected_ndc_position.xyz / unprojected_ndc_position.w;
    return offset_world_position - deref(_globals).offset;
}

// For each fragment check if the page that will be needed during the shadowmap test is allocated
// if not mark page as needing allocation
void request_vsm_pages(f32vec4 depths, u32vec2 scaled_pixel_coords)
{
    // texel gather component mapping - (00,w);(01,x);(11,y);(10,z) 
    // where the 00 is top left
    const f32vec2 offsets[4] = f32vec2[](
        f32vec2(0.5, 1.5),
        f32vec2(1.5, 1.5),
        f32vec2(1.5, 0.5),
        f32vec2(0.5, 0.5)
    );

    for(i32 idx = 0; idx < 4; idx++)
    {
        // Skip fragments into which no objects were rendered
        if(depths[idx] == 0.0) { continue; }
        const f32mat4x4 inv_projection_view = deref(_globals).inv_view_projection;

        // Figure out the clipmap level
        const f32vec2 left_texel_side =  (scaled_pixel_coords + offsets[idx] - f32vec2(0.5, 0.0)) / f32vec2(pc.depth_dimensions);
        const f32vec2 right_texel_side = (scaled_pixel_coords + offsets[idx] + f32vec2(0.5, 0.0)) / f32vec2(pc.depth_dimensions);
        const f32vec3 left_world_space = world_space_from_uv(left_texel_side, depths[idx], inv_projection_view);
        const f32vec3 right_world_space = world_space_from_uv(right_texel_side, depths[idx], inv_projection_view);
        const f32 texel_world_size = length(left_world_space - right_world_space);
        i32 clip_level = max(i32(floor(log2(texel_world_size / deref(_globals).vsm_clip0_texel_world_size))), 0);

        const f32vec2 screen_space_uv = (scaled_pixel_coords + offsets[idx]) / f32vec2(pc.depth_dimensions);
        const f32vec3 world_position = world_space_from_uv(screen_space_uv, depths[idx], inv_projection_view);

        f32vec2 sun_depth_uv;
        for(clip_level; clip_level <= VSM_CLIP_LEVELS; clip_level++)
        {
            const f32vec3 sun_offset_world_position = world_position + deref(_globals).vsm_sun_offset;
            const f32vec4 sun_projected_world_position = deref(_vsm_sun_projections[clip_level]).projection_view * f32vec4(sun_offset_world_position, 1.0);
            const f32vec3 sun_ndc_position = sun_projected_world_position.xyz / sun_projected_world_position.w;
            sun_depth_uv = (sun_ndc_position.xy + f32vec2(1.0)) / f32vec2(2.0);
            bool is_in_clip_bounds = all(lessThanEqual(sun_depth_uv, f32vec2(1.0))) &&
                                     all(greaterThanEqual(sun_depth_uv, f32vec2(0.0)));
            if(is_in_clip_bounds) { break; }
        }
        if(clip_level >= VSM_CLIP_LEVELS) { continue; }

        const i32vec3 vsm_page_pix_coords = i32vec3(sun_depth_uv * VSM_PAGE_TABLE_RESOLUTION, clip_level);
        const u32 page_entry = imageLoad(daxa_uimage2DArray(_vsm_page_table), vsm_page_pix_coords).r;

        const bool is_not_allocated = !get_is_allocated(page_entry);
        const bool allocation_available = atomicAdd(deref(_vsm_allocate_indirect).x, 0) < MAX_NUM_VSM_ALLOC_REQUEST;

        if(is_not_allocated && allocation_available)
        {
            const u32 prev_state = imageAtomicOr(
                daxa_access(r32uiImageArray, _vsm_page_table),
                vsm_page_pix_coords,
                requests_allocation_mask()
            );

            if(!get_requests_allocation(prev_state))
            {
                // If this is the thread to mark this page as REQUESTS_ALLOCATION
                //    -> create a new allocation request in the allocation buffer
                u32 idx = atomicAdd(deref(_vsm_allocate_indirect).x, 1);
                if(idx < MAX_NUM_VSM_ALLOC_REQUEST)
                {
                    deref(_vsm_allocation_buffer[idx]) = AllocationRequest(vsm_page_pix_coords);
                } 
                else 
                {
                    // debugPrintfEXT("Allocation attempted and failed\n");
                    atomicAdd(deref(_vsm_allocate_indirect).x, -1);
                    imageAtomicAnd(
                        daxa_access(r32uiImageArray, _vsm_page_table),
                        vsm_page_pix_coords,
                        ~requests_allocation_mask()
                    );
                }
            } 
        } 
        else if (!get_is_visited_marked(page_entry) && !is_not_allocated)
        {
            const u32 prev_state = imageAtomicOr(
                daxa_access(r32uiImageArray, _vsm_page_table),
                vsm_page_pix_coords,
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
f32vec2 thread_min_max_depth(f32vec4 read_depth_values)
{
    f32 thread_min_depth_value = 0.0;
    f32 thread_max_depth_value = 0.0;

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
    return f32vec2(thread_min_depth_value, thread_max_depth_value);
}

void main()
{
    // ========================== FIRST PASS =====================================
    //        Writes out min/max value per Workgroup into the global memory
    u32vec2 subgroup_offset;
    subgroup_offset.x = 0;
    subgroup_offset.y = (gl_SubgroupID * gl_SubgroupSize) / _tile_size;

    u32vec2 in_subgroup_offset;
    in_subgroup_offset.x = gl_SubgroupInvocationID % _tile_size;
    in_subgroup_offset.y = gl_SubgroupInvocationID / _tile_size;

    const u32vec2 pixel_coords = _tile_size * gl_WorkGroupID.xy + subgroup_offset + in_subgroup_offset;
    // Each thread reads 2x2 block
    const u32vec2 scaled_pixel_coords = pixel_coords * 2;
    // Offset into the middle of the 2x2 block so gather gets us the correct texels
    const u32vec2 scaled_offset_pixel_coords = scaled_pixel_coords + u32vec2(1, 1);

    f32vec2 depth_uv = f32vec2(scaled_offset_pixel_coords) / f32vec2(pc.depth_dimensions);
    f32vec4 read_depth_values = textureGather(daxa_sampler2D(_depth, pc.linear_sampler), depth_uv, 0);

    request_vsm_pages(read_depth_values, scaled_pixel_coords);

    f32vec2 local_min_max_depth = thread_min_max_depth(read_depth_values);
    workgroup_min_max_depth(local_min_max_depth);
}
#else
void main()
{
    u32 work_group_threads = _tile_size * _tile_size;
    u32 workgroup_offset = gl_WorkGroupID.x * work_group_threads;
    u32 _subgroup_offset = gl_SubgroupID * gl_SubgroupSize;

    u32 into_global_index = workgroup_offset + _subgroup_offset + gl_SubgroupInvocationID;
    // Each thread processes 4 values
    into_global_index *= 4; 

    f32vec4 read_min_depth_values;
    f32vec4 read_max_depth_values;

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

    f32vec2 thread_min_max_depth;

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
