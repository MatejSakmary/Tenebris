#define DAXA_ENABLE_SHADER_NO_NAMESPACE 1
#define DAXA_ENABLE_IMAGE_OVERLOADS_BASIC 1
#include <shared/shared.inl>
#include "tasks/analyze_depthbuffer.inl"

#extension GL_KHR_shader_subgroup_arithmetic : enable
#extension GL_EXT_debug_printf : enable

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

    u32vec2 pixel_coords = _tile_size * gl_WorkGroupID.xy + subgroup_offset + in_subgroup_offset;
    // Each thread reads 2x2 block
    pixel_coords *= 2;
    // Offset into the middle of the 2x2 block so gather gets us the correct texels
    pixel_coords += u32vec2(1, 1);

    f32vec2 depth_uv = f32vec2(pixel_coords) / f32vec2(pc.depth_dimensions);
    f32vec4 read_depth_values = textureGather(daxa_sampler2D(_depth, pc.linear_sampler), depth_uv, 0);

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
