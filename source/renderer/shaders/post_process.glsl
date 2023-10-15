#define DAXA_ENABLE_SHADER_NO_NAMESPACE 1
#define DAXA_ENABLE_IMAGE_OVERLOADS_BASIC 1
#include <shared/shared.inl>
#include "tasks/post_process.inl"

DAXA_DECL_PUSH_CONSTANT(PostProcessPC, pc)

#define AGX 0
#define SCUFFED_ACES 1
#define TONY_MC_MAPFACE 2

#define TONEMAPPING TONY_MC_MAPFACE

// Source - https://github.com/BelmuTM/Noble/blob/master/shaders/programs/post/pre_final.glsl
//        - https://github.com/BelmuTM/Noble/blob/master/shaders/include/post/grading.glsl
#if TONEMAPPING == AGX
const f32mat3x3 SRGB_2_XYZ_MAT = f32mat3x3(
	0.4124564, 0.3575761, 0.1804375,
    0.2126729, 0.7151522, 0.0721750,
    0.0193339, 0.1191920, 0.9503041
);
const f32 SRGB_ALPHA = 0.055;

f32vec3  saturate(f32vec3 x)  { return clamp(x, f32vec3(0.0), f32vec3(1.0)); }

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
    // Punchy
    const f32vec3  slope      = f32vec3(1.1);
    const f32vec3  power      = f32vec3(1.2);
    const f32 saturation = 1.3;

    f32 luma = luminance(color);
  
    color = pow(color * slope, power);
    color = luma + saturation * (color - luma);
}
#endif

#if (TONEMAPPING == AGX) || (TONEMAPPING == TONY_MC_MAPFACE)
const f32 exposureBias      = 1.0;
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
#endif

#if TONEMAPPING == SCUFFED_ACES
// Used to convert from linear RGB to XYZ space
const f32mat3x3 RGB_2_XYZ = (f32mat3x3(
    0.4124564, 0.2126729, 0.0193339,
    0.3575761, 0.7151522, 0.1191920,
    0.1804375, 0.0721750, 0.9503041
));

// Used to convert from XYZ to linear RGB space
const f32mat3x3 XYZ_2_RGB = (f32mat3x3(
     3.2404542,-0.9692660, 0.0556434,
    -1.5371385, 1.8760108,-0.2040259,
    -0.4985314, 0.0415560, 1.0572252
));

// Converts a color from linear RGB to XYZ space
f32vec3 rgb_to_xyz(f32vec3 rgb) {
    return RGB_2_XYZ * rgb;
}

// Converts a color from XYZ to linear RGB space
f32vec3 xyz_to_rgb(f32vec3 xyz) {
    return XYZ_2_RGB * xyz;
}

// Converts a color from XYZ to xyY space (Y is luminosity)
f32vec3 xyz_to_xyY(f32vec3 xyz) {
    f32 Y = xyz.y;
    f32 x = xyz.x / (xyz.x + xyz.y + xyz.z);
    f32 y = xyz.y / (xyz.x + xyz.y + xyz.z);
    return f32vec3(x, y, Y);
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

// Converts a color from xyY space to linear RGB
f32vec3 xyY_to_rgb(f32vec3 xyY) {
    f32vec3 xyz = xyY_to_xyz(xyY);
    return xyz_to_rgb(xyz);
}

f32 tonemap_ACES(f32 x) {
    // Narkowicz 2015, "ACES Filmic Tone Mapping Curve"
    const f32 a = 2.51;
    const f32 b = 0.03;
    const f32 c = 2.43;
    const f32 d = 0.59;
    const f32 e = 0.14;
    return (x * (a * x + b)) / (x * (c * x + d) + e);
}
#endif
// Source - https://github.com/h3r2tic/tony-mc-mapface/tree/main 
#if TONEMAPPING == TONY_MC_MAPFACE
f32vec3 tony_mc_mapface(f32vec3 exposed_color)
{
    // Apply a non-linear transform that the LUT is encoded with.
    const f32vec3 encoded = exposed_color / (exposed_color + 1.0);

    // Align the encoded range to texel centers.
    const f32 LUT_DIMS = 48.0;
    const f32vec3 uv = encoded * ((LUT_DIMS - 1.0) / LUT_DIMS) + 0.5 / LUT_DIMS;

    // Note: for OpenGL, do `uv.y = 1.0 - uv.y`
    return texture(daxa_sampler3D(_tonemapping_lut, pc.llce_sampler), uv).xyz;
}
#endif

#define WHITE_POINT   6500
#define WHITE_BALANCE 6500

const mat3 SRGB_2_XYZ_MAT = mat3(
	0.4124564, 0.3575761, 0.1804375,
    0.2126729, 0.7151522, 0.0721750,
    0.0193339, 0.1191920, 0.9503041
);

const mat3 XYZ_2_SRGB_MAT = mat3(
	 3.2409699419,-1.5373831776,-0.4986107603,
    -0.9692436363, 1.8759675015, 0.0415550574,
	 0.0556300797,-0.2039769589, 1.0569715142
);

const f32mat3x3 CONE_RESP_CAT02 = f32mat3x3(
	f32vec3( 0.7328, 0.4296,-0.1624),
	f32vec3(-0.7036, 1.6975, 0.0061),
	f32vec3( 0.0030, 0.0136, 0.9834)
);

vec3 fromXYZ(vec3 color) { return color * XYZ_2_SRGB_MAT; }
vec3 toXYZ(vec3 color) { return color * SRGB_2_XYZ_MAT; }
mat3 fromXYZ(mat3 mat) { return mat * XYZ_2_SRGB_MAT; }
mat3 toXYZ(mat3 mat) { return mat * SRGB_2_XYZ_MAT; }

vec3 plancks(float temperature, vec3 lambda) {
    const float h = 6.62607015e-16; // Planck's constant
    const float c = 2.99792458e17;  // Speed of light in a vacuum
    const float k = 1.38064852e-5;  // Boltzmann's constant

    float numerator   = 2.0 * h * pow(c, 2.0);
    vec3  denominator = (exp(h * c / (lambda * k * temperature)) - vec3(1.0)) * pow(lambda, f32vec3(5.0));
    return (numerator / denominator) * pow(1e9, 2.0);
}

vec3 blackbody(float temperature) {
    vec3 rgb  = plancks(temperature, vec3(660.0, 550.0, 440.0));
         rgb /= max(max(rgb.r, rgb.g), rgb.b); // Keeping the values below 1.0
    return rgb;
}

f32mat3x3 chromaticAdaptationMatrix(f32vec3 source, f32vec3 destination) {
	f32vec3 sourceLMS      = source * CONE_RESP_CAT02;
	f32vec3 destinationLMS = destination * CONE_RESP_CAT02;
	f32vec3 tmp            = destinationLMS / sourceLMS;

	f32mat3x3 vonKries = f32mat3x3(
		tmp.x, 0.0, 0.0,
		0.0, tmp.y, 0.0,
		0.0, 0.0, tmp.z
	);

	return (CONE_RESP_CAT02 * vonKries) * inverse(CONE_RESP_CAT02);
}

void whiteBalance(inout f32vec3 color) {
    vec3 source           = toXYZ(blackbody(WHITE_BALANCE));
    vec3 destination      = toXYZ(blackbody(WHITE_POINT  ));
    mat3 chromaAdaptation = fromXYZ(toXYZ(chromaticAdaptationMatrix(source, destination)));

    color *= chromaAdaptation;
}

layout (location = 0) in f32vec2 in_uv;
layout (location = 0) out f32vec4 out_color;
void main()
{
    f32vec3 hdr_color = texture(daxa_sampler2D(_offscreen, pc.sampler_id), in_uv).rgb;

#if TONEMAPPING == AGX
    f32 exposure = computeExposure(deref(_average_luminance).luminance);
    f32vec3 exposed_color = hdr_color * exposure;

    agx(exposed_color);
    agxLook(exposed_color);
    agxEotf(exposed_color);
    f32vec3 tonemapped_color = exposed_color;
#elif TONEMAPPING == TONY_MC_MAPFACE
    const f32 exposure = computeExposure(deref(_average_luminance).luminance);
    const f32vec3 exposed_color = hdr_color * 0.1/*exposure*/;
    f32vec3 tonemapped_color = tony_mc_mapface(exposed_color);
#elif TONEMAPPING == SCUFFED_ACES
    f32vec3 xyY = rgb_to_xyY(hdr_color);
    f32 lp = xyY.z / (9.6 * deref(_average_luminance).luminance + 0.0001); 
    xyY.z = tonemap_ACES(lp);
    f32vec3 tonemapped_color = xyY_to_rgb(xyY);
#endif
    whiteBalance(tonemapped_color);
    out_color = f32vec4(tonemapped_color, 1.0);
}