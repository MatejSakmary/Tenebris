#define DAXA_ENABLE_SHADER_NO_NAMESPACE 1
#define DAXA_ENABLE_IMAGE_OVERLOADS_BASIC 1
#include <shared/shared.inl>
#include "tasks/analyse_depthbuffer.inl"

#extension GL_KHR_shader_subgroup_arithmetic : enable
#extension GL_EXT_debug_printf : enable

#define _tile_size 32

layout (local_size_x = _tile_size, local_size_y = _tile_size) in;

DAXA_DECL_PUSH_CONSTANT(AnalyseDepthPC, pc)

// If subgroup size is less than 32 this will be too small (32 here represents subgroup size)
shared f32vec2 min_max_depth [_tile_size * _tile_size / 32];

#if defined(FIRST_PASS)
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

    u32vec2 pixel_coords = _tile_size * gl_WorkGroupID.xy + subgroup_offset + in_subgroup_offset;

    f32 depth_value = 0.0;
    if(any(lessThan(pixel_coords, pc.depth_dimensions)))
    {
        depth_value = texelFetch(daxa_texture2D(_depth), i32vec2(pixel_coords), 0).r;
    }

    // we want to ignore depth values where nothing was drawn to the depth buffer
    f32 modified_min_depth = depth_value == 0.0 ? 42.0 : depth_value;
    f32 min_depth = subgroupMin(modified_min_depth);
    f32 max_depth = subgroupMax(depth_value);

    if(subgroupElect()) { min_max_depth[gl_SubgroupID] = f32vec2(min_depth, max_depth); }
    // Implies memoryBarrierShared()
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
#else
void main()
{
    u32 work_group_threads = _tile_size * _tile_size;
    u32 workgroup_offset = gl_WorkGroupID.x * work_group_threads;
    u32 _subgroup_offset = gl_SubgroupID * gl_SubgroupSize;

    u32 into_global_index = workgroup_offset + _subgroup_offset + gl_SubgroupInvocationID;
    // load min_max depth limits from global array
    // Values which will be ignored by the min/max because there are always higher/smaller ones
    f32vec2 depth_limits = f32vec2(42.0, -1.0);
    if(into_global_index < pc.subsequent_thread_count)
    {
        depth_limits = deref(_depth_limits[into_global_index]).limits;
    }

    f32 min_depth = subgroupMin(depth_limits.x);
    f32 max_depth = subgroupMax(depth_limits.y);

    if(subgroupElect()) { min_max_depth[gl_SubgroupID] = f32vec2(min_depth, max_depth); }
    // Implies memoryBarrierShared()
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

            // only the 0th thread in 0th warp writes out the min for the entire workgroup
            if(subgroupElect()) 
            {
                deref(_depth_limits[gl_WorkGroupID.x]).limits = f32vec2(min_depth, max_depth);
            }
        }
    }
}
#endif
