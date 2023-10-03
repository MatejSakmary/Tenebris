#define DAXA_ENABLE_SHADER_NO_NAMESPACE 1
#define DAXA_ENABLE_IMAGE_OVERLOADS_BASIC 1
#include <shared/shared.inl>
#include "tasks/adapt_average_luminance.inl"

#extension GL_KHR_shader_subgroup_arithmetic : enable
#extension GL_EXT_debug_printf : enable

DAXA_DECL_PUSH_CONSTANT(AdaptAverageLuminancePC, pc)

// #define DEBUG
#if defined(DEBUG)
layout (local_size_x = 1) in;
#else
layout (local_size_x = 256) in;
#endif //DEBUG

shared u32 shared_histogram[HISTOGRAM_BIN_COUNT];

void main()
{
#if defined(DEBUG)
    u32 bin_sum = 0;
    u32 weighed_sum = 0;
    for(i32 i = 0; i < 256; i++)
    {
        bin_sum += deref(_histogram[i]).bin_count;
        // weighed_sum += clamp(deref(_histogram[i]).bin_count, 0, 10000) * i;
        weighed_sum += deref(_histogram[i]).bin_count * i;
    }
    shared_histogram[0] = weighed_sum;
    const u32 bin_count = deref(_histogram[0]).bin_count;
    if(pc.resolution.x * pc.resolution.y != bin_sum)
    {
        debugPrintfEXT("bin_sum should be %d but is %d\n", pc.resolution.x * pc.resolution.y, bin_sum);
    }
#else
    const u32 bin_count = deref(_histogram[gl_LocalInvocationIndex]).bin_count;
    // store weighed luminance
    shared_histogram[gl_LocalInvocationIndex] = bin_count * gl_LocalInvocationIndex;
    memoryBarrierShared();
    barrier();
    // 8 - 128 -> 128
    // 7 - 64 -> 64
    // 6 - 32 -> 32
    // 5 - 16 -> 16
    // 4 - 8 -> 8
    // 3 - 4 -> 4
    // 2 - 2 -> 2
    // 1 - 1 -> 1
    // END
    u32 threshold = HISTOGRAM_BIN_COUNT / 2;
    for(i32 i = i32(log2(HISTOGRAM_BIN_COUNT)); i > 0; i--)
    {
        if(gl_LocalInvocationIndex < threshold)
        {
            shared_histogram[gl_LocalInvocationIndex] += shared_histogram[gl_LocalInvocationIndex + threshold];
        }
        threshold /= 2;
        memoryBarrierShared();
        barrier();
    }
#endif

    if(gl_LocalInvocationIndex == 0)
    {
        // Non-black pixel count
        const f32 valid_pixel_count = max(f32(pc.resolution.x) * f32(pc.resolution.y) - f32(bin_count), 1.0);
        const f32 weighed_average_log2 = shared_histogram[0] / valid_pixel_count;
        const f32 average_luminance = exp2(
            (weighed_average_log2 / 254.0) * (1.0/deref(_globals).inv_luminance_range_log2)
            + deref(_globals).min_luminance_log2
        );

        // f32 lum_last_frame = deref(_average_luminance).luminance;
        deref(_average_luminance).luminance = average_luminance;
    }
}