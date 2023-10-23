#define DAXA_ENABLE_IMAGE_OVERLOADS_BASIC 1
#include <shared/shared.inl>
#include "tasks/luminance_histogram.inl"

#extension GL_KHR_shader_subgroup_arithmetic : enable
#extension GL_EXT_debug_printf : enable

DAXA_DECL_PUSH_CONSTANT(LuminanceHistogramPC, pc)

layout (local_size_x = 32, local_size_y = 32) in;

#define PERCEIVED_LUMINANCE_WEIGHTS daxa_f32vec3(0.2127, 0.7152, 0.0722)
#define MIN_LUMINANCE_THRESHOLD 0.005

shared daxa_u32 shared_histogram[HISTOGRAM_BIN_COUNT];
void main()
{
    shared_histogram[gl_LocalInvocationIndex % HISTOGRAM_BIN_COUNT] = 0;
    memoryBarrierShared();
    barrier();

    if(all(lessThan(gl_GlobalInvocationID.xy, daxa_u32vec2(pc.resolution))))
    {
        const daxa_f32vec3 hdr_value = imageLoad(daxa_image2D(_offscreen), daxa_i32vec2(gl_GlobalInvocationID.xy)).rgb;

        daxa_f32 luminance = dot(hdr_value, PERCEIVED_LUMINANCE_WEIGHTS);

        // Avoid log2 on values close to 0
        const daxa_f32 luminance_log2 = luminance < MIN_LUMINANCE_THRESHOLD ? 0.0 : log2(luminance);

        // map the luminance to be relative to [min, max] luminance values
        const daxa_f32 remapped_luminance = 
            (luminance_log2 - deref(_globals).min_luminance_log2)
            * deref(_globals).inv_luminance_range_log2;

        const daxa_f32 clamped_luminance = clamp(remapped_luminance, 0.0, 1.0);
        const daxa_u32 bin_index = daxa_u32(clamped_luminance * 255);

        atomicAdd(shared_histogram[bin_index], 1);
    }
    memoryBarrierShared();
    barrier();

    if(gl_LocalInvocationIndex < HISTOGRAM_BIN_COUNT)
    {
        atomicAdd(
            deref(_histogram[gl_LocalInvocationIndex]).bin_count,
            shared_histogram[gl_LocalInvocationIndex]
        );
    }
}