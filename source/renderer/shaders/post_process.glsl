#define DAXA_ENABLE_SHADER_NO_NAMESPACE 1
#define DAXA_ENABLE_IMAGE_OVERLOADS_BASIC 1
#include <shared/shared.inl>
#include "tasks/post_process.inl"

DAXA_DECL_PUSH_CONSTANT(PostProcessPC, pc)

layout (location = 0) in f32vec2 in_uv;
layout (location = 0) out f32vec4 out_color;

// Used to convert from linear RGB to XYZ space
const f32mat3x3 RGB_2_XYZ = f32mat3x3(
    0.4124564, 0.2126729, 0.0193339,
    0.3575761, 0.7151522, 0.1191920,
    0.1804375, 0.0721750, 0.9503041
);

// Used to convert from XYZ to linear RGB space
const f32mat3x3 XYZ_2_RGB = f32mat3x3(
     3.2404542,-0.9692660, 0.0556434,
    -1.5371385, 1.8760108,-0.2040259,
    -0.4985314, 0.0415560, 1.0572252
);

// Converts a color from XYZ to xyY space (Y is luminosity)
f32vec3 xyz_to_xyY(f32vec3 xyz) {
    f32 Y = xyz.y;
    f32 x = xyz.x / (xyz.x + xyz.y + xyz.z);
    f32 y = xyz.y / (xyz.x + xyz.y + xyz.z);
    return f32vec3(x, y, Y);
}

// Converts a color from linear RGB to XYZ space
f32vec3 rgb_to_xyz(f32vec3 rgb) {
    return RGB_2_XYZ * rgb;
}

// Converts a color from linear RGB to xyY space
f32vec3 rgb_to_xyY(f32vec3 rgb) {
    f32vec3 xyz = rgb_to_xyz(rgb);
    return xyz_to_xyY(xyz);
}

// Converts a color from xyY space to XYZ space
f32vec3 xyY_to_xyz(f32vec3 xyY) {
    f32 Y = xyY.z;
    f32 x = Y * xyY.x / xyY.y;
    f32 z = Y * (1.0 - xyY.x - xyY.y) / xyY.y;
    return f32vec3(x, Y, z);
}

// Converts a color from XYZ to linear RGB space
f32vec3 xyz_to_rgb(f32vec3 xyz) {
    return XYZ_2_RGB * xyz;
}

// Converts a color from xyY space to linear RGB
f32vec3 xyY_to_rgb(f32vec3 xyY) {
    f32vec3 xyz = xyY_to_xyz(xyY);
    return xyz_to_rgb(xyz);
}

f32 Tonemap_ACES(f32 x) {
    // Narkowicz 2015, "ACES Filmic Tone Mapping Curve"
    // const f32 a = 2.21;
    // const f32 b = 0.03;
    // const f32 c = 2.43;
    // const f32 d = 0.59;
    // const f32 e = 0.14;
    const f32 a = 0.2;
    const f32 b = 0.29;
    const f32 c = 0.24;
    const f32 d = 0.272;
    const f32 e = 0.02;
    return (x * (a * x + b)) / (x * (c * x + d) + e);
}
// AGX =====================================
// Source - https://github.com/BelmuTM/Noble/blob/master/shaders/programs/post/pre_final.glsl
//        - https://github.com/BelmuTM/Noble/blob/master/shaders/include/post/grading.glsl
const f32mat3x3 SRGB_2_XYZ_MAT = f32mat3x3
(
	0.4124564, 0.3575761, 0.1804375,
    0.2126729, 0.7151522, 0.0721750,
    0.0193339, 0.1191920, 0.9503041
);

f32 luminance(f32vec3 color) 
{
    f32vec3 luminanceCoefficients = SRGB_2_XYZ_MAT[1];
    return dot(color, luminanceCoefficients);
}

const f32mat3x3 agxTransform = f32mat3x3(
    0.842479062253094 , 0.0423282422610123, 0.0423756549057051,
    0.0784335999999992, 0.878468636469772 , 0.0784336,
    0.0792237451477643, 0.0791661274605434, 0.879142973793104
);

const f32mat3x3 agxTransformInverse = f32mat3x3(
     1.19687900512017  , -0.0528968517574562, -0.0529716355144438,
    -0.0980208811401368,  1.15190312990417  , -0.0980434501171241,
    -0.0990297440797205, -0.0989611768448433,  1.15107367264116
);

f32vec3 agxDefaultContrastApproximation(f32vec3 x) {
    f32vec3 x2 = x * x;
    f32vec3 x4 = x2 * x2;
  
    return + 15.5     * x4 * x2
           - 40.14    * x4 * x
           + 31.96    * x4
           - 6.868    * x2 * x
           + 0.4298   * x2
           + 0.1191   * x
           - 0.00232;
}

void agx(inout f32vec3 color) {
    const f32 minEv = -12.47393;
    const f32 maxEv = 4.026069;

    color = agxTransform * color;
    color = clamp(log2(color), minEv, maxEv);
    color = (color - minEv) / (maxEv - minEv);
    color = agxDefaultContrastApproximation(color);
}

void agxEotf(inout f32vec3 color) {
    color = agxTransformInverse * color;
}

void agxLook(inout f32vec3 color) {
    // #if AGX_LOOK == 0
    //     // Default
        // const f32vec3  slope      = f32vec3(1.0);
        // const f32vec3  power      = f32vec3(1.0);
        // const f32 saturation = 1.0;
    // #elif AGX_LOOK == 1
        // Golden
        // const f32vec3  slope      = f32vec3(1.0, 0.9, 0.5);
        // const f32vec3  power      = f32vec3(0.8);
        // const f32 saturation = 0.8;
    // #elif AGX_LOOK == 2
    //     // Punchy
        const f32vec3  slope      = f32vec3(1.1);
        const f32vec3  power      = f32vec3(1.2);
        const f32 saturation = 1.2;
    // #endif

    f32 luma = luminance(color);
  
    color = pow(color * slope, power);
    color = luma + saturation * (color - luma);
}

const f32 exposureBias      = 1.5;
const f32 calibration       = 12.5;  // Light meter calibration
const f32 sensorSensitivity = 100.0; // Sensor sensitivity

f32 computeEV100fromLuminance(f32 luminance) {
    return log2(luminance * sensorSensitivity * exposureBias / calibration);
}

f32 computeExposureFromEV100(f32 ev100) {
    return 1.0 / (1.2 * exp2(ev100));
}

f32 computeExposure(f32 averageLuminance) {
	f32 ev100	 = computeEV100fromLuminance(averageLuminance);
	f32 exposure = computeExposureFromEV100(ev100);

	return exposure;
}

void main()
{
    f32vec3 hdr_color = texture(daxa_sampler2D(_offscreen, pc.sampler_id), in_uv).rgb;
    f32vec3 xyY = rgb_to_xyY(hdr_color);
    const f32 lp = xyY.z / (9.6 * deref(_average_luminance).luminance + 0.0001);
    xyY.z = Tonemap_ACES(lp);

    f32 exposure = computeExposure(deref(_average_luminance).luminance);
    f32vec3 color = hdr_color * exposure;
    agx(color);
    agxLook(color);
    agxEotf(color);
    out_color = f32vec4(color, 1.0);
// #endif
}