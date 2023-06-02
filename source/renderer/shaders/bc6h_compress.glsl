// Ported to glsl from https://github.com/knarkowicz/GPURealTimeBC6H/blob/master/bin/compress.hlsl
#define DAXA_ENABLE_SHADER_NO_NAMESPACE 1
#define DAXA_ENABLE_IMAGE_OVERLOADS_BASIC 1
#include <shared/shared.inl>
#include <tasks/bc6h_compress.inl>

DAXA_USE_PUSH_CONSTANT(BC6HCompressPC, pc)

// Whether to use P2 modes (4 endpoints) for compression. Slow, but improves quality.
#define ENCODE_P2 (QUALITY == 1)

// Improve quality at small performance loss
#define INSET_COLOR_BBOX 1
#define OPTIMIZE_ENDPOINTS 1

// Whether to optimize for luminance error or for RGB error
#define LUMINANCE_WEIGHTS 1


const f32 HALF_MAX = 65504.0f;
const u32 PATTERN_NUM = 32u;

u32 f32tof16(f32 val)
{
    f32vec2 pack = f32vec2(val, 0);
    return packHalf2x16(pack);
}

u32vec2 f32tof16(f32vec2 val)
{
    u32vec2 res;
    res.x = packHalf2x16(f32vec2(val.x, 0));
    res.y = packHalf2x16(f32vec2(val.y, 0));
    return res;
}

u32vec3 f32tof16(f32vec3 val)
{
    u32vec3 res;
    res.x = packHalf2x16(f32vec2(val.x, 0));
    res.y = packHalf2x16(f32vec2(val.y, 0));
    res.z = packHalf2x16(f32vec2(val.z, 0));
    return res;
}

f32 f16tof32(f32 val)
{
    return unpackHalf2x16(u32(val)).x;
}

f32vec2 f16tof32(f32vec2 val)
{
    f32vec2 ret;
    ret.x = unpackHalf2x16(u32(val.x)).x;
    ret.y = unpackHalf2x16(u32(val.y)).x;
	return ret;
}

f32vec3 f16tof32(f32vec3 val)
{
    f32vec3 ret;
    ret.x = unpackHalf2x16(u32(val.x)).x;
    ret.y = unpackHalf2x16(u32(val.y)).x;
    ret.z = unpackHalf2x16(u32(val.z)).x;
	return ret;
}


f32 CalcMSLE(f32vec3 a, f32vec3 b)
{
	f32vec3 delta = log2((b + 1.0f) / (a + 1.0f));
	f32vec3 deltaSq = delta * delta;

#if LUMINANCE_WEIGHTS
	f32vec3 luminanceWeights = f32vec3(0.299f, 0.587f, 0.114f);
	deltaSq *= luminanceWeights;
#endif

	return deltaSq.x + deltaSq.y + deltaSq.z;
}

u32 PatternFixupID(u32 i)
{
	u32 ret = 15;
	ret = bool((3441033216 >> i) & 0x1) ? 2 : ret;
	ret = bool((845414400 >> i) & 0x1) ? 8 : ret;
	return ret;
}

u32 Pattern(u32 p, u32 i)
{
	u32 p2 = p / 2;
	u32 p3 = p - p2 * 2;

	u32 enc = 0;
	enc = p2 == 0 ? 2290666700 : enc;
	enc = p2 == 1 ? 3972591342 : enc;
	enc = p2 == 2 ? 4276930688 : enc;
	enc = p2 == 3 ? 3967876808 : enc;
	enc = p2 == 4 ? 4293707776 : enc;
	enc = p2 == 5 ? 3892379264 : enc;
	enc = p2 == 6 ? 4278255592 : enc;
	enc = p2 == 7 ? 4026597360 : enc;
	enc = p2 == 8 ? 9369360 : enc;
	enc = p2 == 9 ? 147747072 : enc;
	enc = p2 == 10 ? 1930428556 : enc;
	enc = p2 == 11 ? 2362323200 : enc;
	enc = p2 == 12 ? 823134348 : enc;
	enc = p2 == 13 ? 913073766 : enc;
	enc = p2 == 14 ? 267393000 : enc;
	enc = p2 == 15 ? 966553998 : enc;

	enc = bool(p3) ? enc >> 16 : enc;
	u32 ret = (enc >> i) & 0x1;
	return ret;
}

f32vec3 Quantize7(f32vec3 x)
{
	return (f32tof16(x) * 128.0f) / (0x7bff + 1.0f);
}

f32vec3 Quantize9(f32vec3 x)
{
	return (f32tof16(x) * 512.0f) / (0x7bff + 1.0f);
}

f32vec3 Quantize10(f32vec3 x)
{
	return (f32tof16(x) * 1024.0f) / (0x7bff + 1.0f);
}

f32vec3 Unquantize7(f32vec3 x)
{
	return (x * 65536.0f + 0x8000) / 128.0f;
}

f32vec3 Unquantize9(f32vec3 x)
{
	return (x * 65536.0f + 0x8000) / 512.0f;
}

f32vec3 Unquantize10(f32vec3 x)
{
	return (x * 65536.0f + 0x8000) / 1024.0f;
}

f32vec3 FinishUnquantize(f32vec3 endpoint0Unq, f32vec3 endpoint1Unq, f32 weight)
{
	f32vec3 comp = (endpoint0Unq * (64.0f - weight) + endpoint1Unq * weight + 32.0f) * (31.0f / 4096.0f);
    return f16tof32(u32vec3(comp));
}

void Swap(inout f32vec3 a, inout f32vec3 b)
{
	f32vec3 tmp = a;
	a = b;
	b = tmp;
}

void Swap(inout f32 a, inout f32 b)
{
	f32 tmp = a;
	a = b;
	b = tmp;
}

u32 ComputeIndex3(f32 texelPos, f32 endPoint0Pos, f32 endPoint1Pos)
{
	f32 r = (texelPos - endPoint0Pos) / (endPoint1Pos - endPoint0Pos);
	return u32(clamp(r * 6.98182f + 0.00909f + 0.5f, 0.0f, 7.0f));
}

u32 ComputeIndex4(f32 texelPos, f32 endPoint0Pos, f32 endPoint1Pos)
{
	f32 r = (texelPos - endPoint0Pos) / (endPoint1Pos - endPoint0Pos);
	return u32(clamp(r * 14.93333f + 0.03333f + 0.5f, 0.0f, 15.0f));
}

void SignExtend(inout f32vec3 v1, u32 mask, u32 signFlag)
{
	i32vec3 v = i32vec3(v1);
	v.x = int((v.x & mask) | (v.x < 0 ? signFlag : 0));
	v.y = int((v.y & mask) | (v.y < 0 ? signFlag : 0));
	v.z = int((v.z & mask) | (v.z < 0 ? signFlag : 0));
	v1 = v;
}

// Refine endpoints by insetting bounding box in log2 RGB space
void InsetColorBBoxP1(f32vec3 texels[16], inout f32vec3 blockMin, inout f32vec3 blockMax)
{
	f32vec3 refinedBlockMin = blockMax;
	f32vec3 refinedBlockMax = blockMin;

	for (u32 i = 0; i < 16; ++i)
	{
		refinedBlockMin = min(refinedBlockMin, texels[i] == blockMin ? refinedBlockMin : texels[i]);
		refinedBlockMax = max(refinedBlockMax, texels[i] == blockMax ? refinedBlockMax : texels[i]);
	}

	f32vec3 logRefinedBlockMax = log2(refinedBlockMax + 1.0f);
	f32vec3 logRefinedBlockMin = log2(refinedBlockMin + 1.0f);

	f32vec3 logBlockMax = log2(blockMax + 1.0f);
	f32vec3 logBlockMin = log2(blockMin + 1.0f);
	f32vec3 logBlockMaxExt = (logBlockMax - logBlockMin) * (1.0f / 32.0f);

	logBlockMin += min(logRefinedBlockMin - logBlockMin, logBlockMaxExt);
	logBlockMax -= min(logBlockMax - logRefinedBlockMax, logBlockMaxExt);

	blockMin = exp2(logBlockMin) - 1.0f;
	blockMax = exp2(logBlockMax) - 1.0f;
}

// Refine endpoints by insetting bounding box in log2 RGB space
void InsetColorBBoxP2(f32vec3 texels[16], u32 pattern, u32 patternSelector, inout f32vec3 blockMin, inout f32vec3 blockMax)
{
	f32vec3 refinedBlockMin = blockMax;
	f32vec3 refinedBlockMax = blockMin;

	for (u32 i = 0; i < 16; ++i)
	{
		u32 paletteID = Pattern(pattern, i);
		if (paletteID == patternSelector)
		{
			refinedBlockMin = min(refinedBlockMin, texels[i] == blockMin ? refinedBlockMin : texels[i]);
			refinedBlockMax = max(refinedBlockMax, texels[i] == blockMax ? refinedBlockMax : texels[i]);
		}
	}

	f32vec3 logRefinedBlockMax = log2(refinedBlockMax + 1.0f);
	f32vec3 logRefinedBlockMin = log2(refinedBlockMin + 1.0f);

	f32vec3 logBlockMax = log2(blockMax + 1.0f);
	f32vec3 logBlockMin = log2(blockMin + 1.0f);
	f32vec3 logBlockMaxExt = (logBlockMax - logBlockMin) * (1.0f / 32.0f);

	logBlockMin += min(logRefinedBlockMin - logBlockMin, logBlockMaxExt);
	logBlockMax -= min(logBlockMax - logRefinedBlockMax, logBlockMaxExt);

	blockMin = exp2(logBlockMin) - 1.0f;
	blockMax = exp2(logBlockMax) - 1.0f;
}

// Least squares optimization to find best endpoints for the selected block indices
void OptimizeEndpointsP1(f32vec3 texels[16], inout f32vec3 blockMin, inout f32vec3 blockMax)
{
	f32vec3 blockDir = blockMax - blockMin;
	blockDir = blockDir / (blockDir.x + blockDir.y + blockDir.z);

	f32 endPoint0Pos = f32tof16(dot(blockMin, blockDir));
	f32 endPoint1Pos = f32tof16(dot(blockMax, blockDir));

	f32vec3 alphaTexelSum = f32vec3(0.0f, 0.0f, 0.0f);
	f32vec3 betaTexelSum = f32vec3(0.0f, 0.0f, 0.0f);
	f32 alphaBetaSum = 0.0f;
	f32 alphaSqSum = 0.0f;
	f32 betaSqSum = 0.0f;

	for (i32 i = 0; i < 16; i++)
	{
		f32 texelPos = f32tof16(dot(texels[i], blockDir));
		u32 texelIndex = ComputeIndex4(texelPos, endPoint0Pos, endPoint1Pos);

		f32 beta = clamp(texelIndex / 15.0f, 0.0, 1.0);
		f32 alpha = 1.0f - beta;

		f32vec3 texelF16 = f32tof16(texels[i].xyz);
		alphaTexelSum += alpha * texelF16;
		betaTexelSum += beta * texelF16;

		alphaBetaSum += alpha * beta;

		alphaSqSum += alpha * alpha;
		betaSqSum += beta * beta;
	}

	f32 det = alphaSqSum * betaSqSum - alphaBetaSum * alphaBetaSum;

	if (abs(det) > 0.00001f)
	{
		f32 detRcp = 1.0 / det;
		blockMin = f16tof32(clamp(detRcp * (alphaTexelSum * betaSqSum - betaTexelSum * alphaBetaSum), 0.0f, HALF_MAX));
		blockMax = f16tof32(clamp(detRcp * (betaTexelSum * alphaSqSum - alphaTexelSum * alphaBetaSum), 0.0f, HALF_MAX));
	}
}

// Least squares optimization to find best endpoints for the selected block indices
void OptimizeEndpointsP2(f32vec3 texels[16], u32 pattern, u32 patternSelector, inout f32vec3 blockMin, inout f32vec3 blockMax)
{
	f32vec3 blockDir = blockMax - blockMin;
	blockDir = blockDir / (blockDir.x + blockDir.y + blockDir.z);

	f32 endPoint0Pos = f32tof16(dot(blockMin, blockDir));
	f32 endPoint1Pos = f32tof16(dot(blockMax, blockDir));

	f32vec3 alphaTexelSum = f32vec3(0.0f, 0.0f, 0.0f);
	f32vec3 betaTexelSum = f32vec3(0.0f, 0.0f, 0.0f);
	f32 alphaBetaSum = 0.0f;
	f32 alphaSqSum = 0.0f;
	f32 betaSqSum = 0.0f;

	for (i32 i = 0; i < 16; i++)
	{
		u32 paletteID = Pattern(pattern, i);
		if (paletteID == patternSelector)
		{
			f32 texelPos = f32tof16(dot(texels[i], blockDir));
			u32 texelIndex = ComputeIndex3(texelPos, endPoint0Pos, endPoint1Pos);

			f32 beta = clamp(texelIndex / 7.0f, 0.0, 1.0);
			f32 alpha = 1.0f - beta;

			f32vec3 texelF16 = f32tof16(texels[i].xyz);
			alphaTexelSum += alpha * texelF16;
			betaTexelSum += beta * texelF16;

			alphaBetaSum += alpha * beta;

			alphaSqSum += alpha * alpha;
			betaSqSum += beta * beta;
		}
	}

	f32 det = alphaSqSum * betaSqSum - alphaBetaSum * alphaBetaSum;

	if (abs(det) > 0.00001f)
	{
		f32 detRcp = 1.0 / det;
		blockMin = f16tof32(clamp(detRcp * (alphaTexelSum * betaSqSum - betaTexelSum * alphaBetaSum), 0.0f, HALF_MAX));
		blockMax = f16tof32(clamp(detRcp * (betaTexelSum * alphaSqSum - alphaTexelSum * alphaBetaSum), 0.0f, HALF_MAX));
	}
}

void EncodeP1(inout u32vec4 block, inout f32 blockMSLE, f32vec3 texels[16])
{
	// compute endpoints (min/max RGB bbox)
	f32vec3 blockMin = texels[0];
	f32vec3 blockMax = texels[0];
	for (u32 i = 1; i < 16; ++i)
	{
		blockMin = min(blockMin, texels[i]);
		blockMax = max(blockMax, texels[i]);
	}

#if INSET_COLOR_BBOX
	InsetColorBBoxP1(texels, blockMin, blockMax);
#endif

#if OPTIMIZE_ENDPOINTS
	OptimizeEndpointsP1(texels, blockMin, blockMax);
#endif


	f32vec3 blockDir = blockMax - blockMin;
	blockDir = blockDir / (blockDir.x + blockDir.y + blockDir.z);

	f32vec3 endpoint0 = Quantize10(blockMin);
	f32vec3 endpoint1 = Quantize10(blockMax);
	f32 endPoint0Pos = f32tof16(dot(blockMin, blockDir));
	f32 endPoint1Pos = f32tof16(dot(blockMax, blockDir));

	// check if endpoint swap is required
	f32 fixupTexelPos = f32tof16(dot(texels[0], blockDir));
	u32 fixupIndex = ComputeIndex4(fixupTexelPos, endPoint0Pos, endPoint1Pos);
	if (fixupIndex > 7)
	{
		Swap(endPoint0Pos, endPoint1Pos);
		Swap(endpoint0, endpoint1);
	}

	// compute indices
	u32 indices[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	for (u32 i = 0; i < 16; ++i)
	{
		f32 texelPos = f32tof16(dot(texels[i], blockDir));
		indices[i] = ComputeIndex4(texelPos, endPoint0Pos, endPoint1Pos);
	}

	// compute compression error (MSLE)
	f32vec3 endpoint0Unq = Unquantize10(endpoint0);
	f32vec3 endpoint1Unq = Unquantize10(endpoint1);
	f32 msle = 0.0f;
	for (u32 i = 0; i < 16; ++i)
	{
		f32 weight = floor((indices[i] * 64.0f) / 15.0f + 0.5f);
		f32vec3 texelUnc = FinishUnquantize(endpoint0Unq, endpoint1Unq, weight);

		msle += CalcMSLE(texels[i], texelUnc);
	}


	// encode block for mode 11
	blockMSLE = msle;
	block.x = 0x03;

	// endpoints
	block.x |= u32(endpoint0.x) << 5;
	block.x |= u32(endpoint0.y) << 15;
	block.x |= u32(endpoint0.z) << 25;
	block.y |= u32(endpoint0.z) >> 7;
	block.y |= u32(endpoint1.x) << 3;
	block.y |= u32(endpoint1.y) << 13;
	block.y |= u32(endpoint1.z) << 23;
	block.z |= u32(endpoint1.z) >> 9;

	// indices
	block.z |= indices[0] << 1;
	block.z |= indices[1] << 4;
	block.z |= indices[2] << 8;
	block.z |= indices[3] << 12;
	block.z |= indices[4] << 16;
	block.z |= indices[5] << 20;
	block.z |= indices[6] << 24;
	block.z |= indices[7] << 28;
	block.w |= indices[8] << 0;
	block.w |= indices[9] << 4;
	block.w |= indices[10] << 8;
	block.w |= indices[11] << 12;
	block.w |= indices[12] << 16;
	block.w |= indices[13] << 20;
	block.w |= indices[14] << 24;
	block.w |= indices[15] << 28;
}

f32 DistToLineSq(f32vec3 PointOnLine, f32vec3 LineDirection, f32vec3 Point)
{
	f32vec3 w = Point - PointOnLine;
	f32vec3 x = w - dot(w, LineDirection) * LineDirection;
	return dot(x, x);
}

// Evaluate how good is given P2 pattern for encoding current block
f32 EvaluateP2Pattern(i32 pattern, f32vec3 texels[16])
{
	f32vec3 p0BlockMin = f32vec3(HALF_MAX, HALF_MAX, HALF_MAX);
	f32vec3 p0BlockMax = f32vec3(0.0f, 0.0f, 0.0f);
	f32vec3 p1BlockMin = f32vec3(HALF_MAX, HALF_MAX, HALF_MAX);
	f32vec3 p1BlockMax = f32vec3(0.0f, 0.0f, 0.0f);

	for (u32 i = 0; i < 16; ++i)
	{
		u32 paletteID = Pattern(pattern, i);
		if (paletteID == 0)
		{
			p0BlockMin = min(p0BlockMin, texels[i]);
			p0BlockMax = max(p0BlockMax, texels[i]);
		}
		else
		{
			p1BlockMin = min(p1BlockMin, texels[i]);
			p1BlockMax = max(p1BlockMax, texels[i]);
		}
	}

	f32vec3 p0BlockDir = normalize(p0BlockMax - p0BlockMin);
	f32vec3 p1BlockDir = normalize(p1BlockMax - p1BlockMin);

	f32 sqDistanceFromLine = 0.0f;

	for (u32 i = 0; i < 16; ++i)
	{
		u32 paletteID = Pattern(pattern, i);
		if (paletteID == 0)
		{
			sqDistanceFromLine += DistToLineSq(p0BlockMin, p0BlockDir, texels[i]);
		}
		else
		{
			sqDistanceFromLine += DistToLineSq(p1BlockMin, p1BlockDir, texels[i]);
		}
	}

	return sqDistanceFromLine;
}

void EncodeP2Pattern(inout u32vec4 block, inout f32 blockMSLE, i32 pattern, f32vec3 texels[16])
{
	f32vec3 p0BlockMin = f32vec3(HALF_MAX, HALF_MAX, HALF_MAX);
	f32vec3 p0BlockMax = f32vec3(0.0f, 0.0f, 0.0f);
	f32vec3 p1BlockMin = f32vec3(HALF_MAX, HALF_MAX, HALF_MAX);
	f32vec3 p1BlockMax = f32vec3(0.0f, 0.0f, 0.0f);

	for (u32 i = 0; i < 16; ++i)
	{
		u32 paletteID = Pattern(pattern, i);
		if (paletteID == 0)
		{
			p0BlockMin = min(p0BlockMin, texels[i]);
			p0BlockMax = max(p0BlockMax, texels[i]);
		}
		else
		{
			p1BlockMin = min(p1BlockMin, texels[i]);
			p1BlockMax = max(p1BlockMax, texels[i]);
		}
	}

#if INSET_COLOR_BBOX
	// Disabled because it was a negligible quality increase
	//InsetColorBBoxP2(texels, pattern, 0, p0BlockMin, p0BlockMax);
	//InsetColorBBoxP2(texels, pattern, 1, p1BlockMin, p1BlockMax);
#endif

#if OPTIMIZE_ENDPOINTS
	OptimizeEndpointsP2(texels, pattern, 0, p0BlockMin, p0BlockMax);
	OptimizeEndpointsP2(texels, pattern, 1, p1BlockMin, p1BlockMax);
#endif

	f32vec3 p0BlockDir = p0BlockMax - p0BlockMin;
	f32vec3 p1BlockDir = p1BlockMax - p1BlockMin;
	p0BlockDir = p0BlockDir / (p0BlockDir.x + p0BlockDir.y + p0BlockDir.z);
	p1BlockDir = p1BlockDir / (p1BlockDir.x + p1BlockDir.y + p1BlockDir.z);


	f32 p0Endpoint0Pos = f32tof16(dot(p0BlockMin, p0BlockDir));
	f32 p0Endpoint1Pos = f32tof16(dot(p0BlockMax, p0BlockDir));
	f32 p1Endpoint0Pos = f32tof16(dot(p1BlockMin, p1BlockDir));
	f32 p1Endpoint1Pos = f32tof16(dot(p1BlockMax, p1BlockDir));


	u32 fixupID = PatternFixupID(pattern);
	f32 p0FixupTexelPos = f32tof16(dot(texels[0], p0BlockDir));
	f32 p1FixupTexelPos = f32tof16(dot(texels[fixupID], p1BlockDir));
	u32 p0FixupIndex = ComputeIndex3(p0FixupTexelPos, p0Endpoint0Pos, p0Endpoint1Pos);
	u32 p1FixupIndex = ComputeIndex3(p1FixupTexelPos, p1Endpoint0Pos, p1Endpoint1Pos);
	if (p0FixupIndex > 3)
	{
		Swap(p0Endpoint0Pos, p0Endpoint1Pos);
		Swap(p0BlockMin, p0BlockMax);
	}
	if (p1FixupIndex > 3)
	{
		Swap(p1Endpoint0Pos, p1Endpoint1Pos);
		Swap(p1BlockMin, p1BlockMax);
	}

	u32 indices[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	for (u32 i = 0; i < 16; ++i)
	{
		f32 p0TexelPos = f32tof16(dot(texels[i], p0BlockDir));
		f32 p1TexelPos = f32tof16(dot(texels[i], p1BlockDir));
		u32 p0Index = ComputeIndex3(p0TexelPos, p0Endpoint0Pos, p0Endpoint1Pos);
		u32 p1Index = ComputeIndex3(p1TexelPos, p1Endpoint0Pos, p1Endpoint1Pos);

		u32 paletteID = Pattern(pattern, i);
		indices[i] = paletteID == 0 ? p0Index : p1Index;
	}

	f32vec3 endpoint760 = floor(Quantize7(p0BlockMin));
	f32vec3 endpoint761 = floor(Quantize7(p0BlockMax));
	f32vec3 endpoint762 = floor(Quantize7(p1BlockMin));
	f32vec3 endpoint763 = floor(Quantize7(p1BlockMax));

	f32vec3 endpoint950 = floor(Quantize9(p0BlockMin));
	f32vec3 endpoint951 = floor(Quantize9(p0BlockMax));
	f32vec3 endpoint952 = floor(Quantize9(p1BlockMin));
	f32vec3 endpoint953 = floor(Quantize9(p1BlockMax));

	endpoint761 = endpoint761 - endpoint760;
	endpoint762 = endpoint762 - endpoint760;
	endpoint763 = endpoint763 - endpoint760;

	endpoint951 = endpoint951 - endpoint950;
	endpoint952 = endpoint952 - endpoint950;
	endpoint953 = endpoint953 - endpoint950;

	i32 maxVal76 = 0x1F;
	endpoint761 = clamp(endpoint761, -maxVal76, maxVal76);
	endpoint762 = clamp(endpoint762, -maxVal76, maxVal76);
	endpoint763 = clamp(endpoint763, -maxVal76, maxVal76);

	i32 maxVal95 = 0xF;
	endpoint951 = clamp(endpoint951, -maxVal95, maxVal95);
	endpoint952 = clamp(endpoint952, -maxVal95, maxVal95);
	endpoint953 = clamp(endpoint953, -maxVal95, maxVal95);

	f32vec3 endpoint760Unq = Unquantize7(endpoint760);
	f32vec3 endpoint761Unq = Unquantize7(endpoint760 + endpoint761);
	f32vec3 endpoint762Unq = Unquantize7(endpoint760 + endpoint762);
	f32vec3 endpoint763Unq = Unquantize7(endpoint760 + endpoint763);
	f32vec3 endpoint950Unq = Unquantize9(endpoint950);
	f32vec3 endpoint951Unq = Unquantize9(endpoint950 + endpoint951);
	f32vec3 endpoint952Unq = Unquantize9(endpoint950 + endpoint952);
	f32vec3 endpoint953Unq = Unquantize9(endpoint950 + endpoint953);

	f32 msle76 = 0.0f;
	f32 msle95 = 0.0f;
	for (u32 i = 0; i < 16; ++i)
	{
		u32 paletteID = Pattern(pattern, i);

		f32vec3 tmp760Unq = paletteID == 0 ? endpoint760Unq : endpoint762Unq;
		f32vec3 tmp761Unq = paletteID == 0 ? endpoint761Unq : endpoint763Unq;
		f32vec3 tmp950Unq = paletteID == 0 ? endpoint950Unq : endpoint952Unq;
		f32vec3 tmp951Unq = paletteID == 0 ? endpoint951Unq : endpoint953Unq;

		f32 weight = floor((indices[i] * 64.0f) / 7.0f + 0.5f);
		f32vec3 texelUnc76 = FinishUnquantize(tmp760Unq, tmp761Unq, weight);
		f32vec3 texelUnc95 = FinishUnquantize(tmp950Unq, tmp951Unq, weight);

		msle76 += CalcMSLE(texels[i], texelUnc76);
		msle95 += CalcMSLE(texels[i], texelUnc95);
	}

	SignExtend(endpoint761, 0x1F, 0x20);
	SignExtend(endpoint762, 0x1F, 0x20);
	SignExtend(endpoint763, 0x1F, 0x20);

	SignExtend(endpoint951, 0xF, 0x10);
	SignExtend(endpoint952, 0xF, 0x10);
	SignExtend(endpoint953, 0xF, 0x10);

	// encode block
	f32 p2MSLE = min(msle76, msle95);
	if (p2MSLE < blockMSLE)
	{
		blockMSLE = p2MSLE;
		block = u32vec4(0, 0, 0, 0);

		if (p2MSLE == msle76)
		{
			// 7.6
			block.x = 0x1;
			block.x |= ( u32(endpoint762.y) & 0x20) >> 3;
			block.x |= ( u32(endpoint763.y) & 0x10) >> 1;
			block.x |= ( u32(endpoint763.y) & 0x20) >> 1;
			block.x |=   u32(endpoint760.x) << 5;
			block.x |= ( u32(endpoint763.z) & 0x01) << 12;
			block.x |= ( u32(endpoint763.z) & 0x02) << 12;
			block.x |= ( u32(endpoint762.z) & 0x10) << 10;
			block.x |=   u32(endpoint760.y) << 15;
			block.x |= ( u32(endpoint762.z) & 0x20) << 17;
			block.x |= ( u32(endpoint763.z) & 0x04) << 21;
			block.x |= ( u32(endpoint762.y) & 0x10) << 20;
			block.x |=   u32(endpoint760.z) << 25;
			block.y |= ( u32(endpoint763.z) & 0x08) >> 3;
			block.y |= ( u32(endpoint763.z) & 0x20) >> 4;
			block.y |= ( u32(endpoint763.z) & 0x10) >> 2;
			block.y |=   u32(endpoint761.x) << 3;
			block.y |= ( u32(endpoint762.y) & 0x0F) << 9;
			block.y |=   u32(endpoint761.y) << 13;
			block.y |= ( u32(endpoint763.y) & 0x0F) << 19;
			block.y |=   u32(endpoint761.z) << 23;
			block.y |= ( u32(endpoint762.z) & 0x07) << 29;
			block.z |= ( u32(endpoint762.z) & 0x08) >> 3;
			block.z |=   u32(endpoint762.x) << 1;
			block.z |=   u32(endpoint763.x) << 7;
		}
		else
		{
			// 9.5
			block.x = 0xE;
			block.x |=  u32(endpoint950.x) << 5;
			block.x |= (u32(endpoint952.z) & 0x10) << 10;
			block.x |=  u32(endpoint950.y) << 15;
			block.x |= (u32(endpoint952.y) & 0x10) << 20;
			block.x |=  u32(endpoint950.z) << 25;
			block.y |=  u32(endpoint950.z) >> 7;
			block.y |= (u32(endpoint953.z) & 0x10) >> 2;
			block.y |=  u32(endpoint951.x) << 3;
			block.y |= (u32(endpoint953.y) & 0x10) << 4;
			block.y |= (u32(endpoint952.y) & 0x0F) << 9;
			block.y |=  u32(endpoint951.y) << 13;
			block.y |= (u32(endpoint953.z) & 0x01) << 18;
			block.y |= (u32(endpoint953.y) & 0x0F) << 19;
			block.y |=  u32(endpoint951.z) << 23;
			block.y |= (u32(endpoint953.z) & 0x02) << 27;
			block.y |=  u32(endpoint952.z) << 29;
			block.z |= (u32(endpoint952.z) & 0x08) >> 3;
			block.z |=  u32(endpoint952.x) << 1;
			block.z |= (u32(endpoint953.z) & 0x04) << 4;
			block.z |=  u32(endpoint953.x) << 7;
			block.z |= (u32(endpoint953.z) & 0x08) << 9;
		}

		block.z |= pattern << 13;
		u32 blockFixupID = PatternFixupID(pattern);
		if (blockFixupID == 15)
		{
			block.z |= indices[0] << 18;
			block.z |= indices[1] << 20;
			block.z |= indices[2] << 23;
			block.z |= indices[3] << 26;
			block.z |= indices[4] << 29;
			block.w |= indices[5] << 0;
			block.w |= indices[6] << 3;
			block.w |= indices[7] << 6;
			block.w |= indices[8] << 9;
			block.w |= indices[9] << 12;
			block.w |= indices[10] << 15;
			block.w |= indices[11] << 18;
			block.w |= indices[12] << 21;
			block.w |= indices[13] << 24;
			block.w |= indices[14] << 27;
			block.w |= indices[15] << 30;
		}
		else if (blockFixupID == 2)
		{
			block.z |= indices[0] << 18;
			block.z |= indices[1] << 20;
			block.z |= indices[2] << 23;
			block.z |= indices[3] << 25;
			block.z |= indices[4] << 28;
			block.z |= indices[5] << 31;
			block.w |= indices[5] >> 1;
			block.w |= indices[6] << 2;
			block.w |= indices[7] << 5;
			block.w |= indices[8] << 8;
			block.w |= indices[9] << 11;
			block.w |= indices[10] << 14;
			block.w |= indices[11] << 17;
			block.w |= indices[12] << 20;
			block.w |= indices[13] << 23;
			block.w |= indices[14] << 26;
			block.w |= indices[15] << 29;
		}
		else
		{
			block.z |= indices[0] << 18;
			block.z |= indices[1] << 20;
			block.z |= indices[2] << 23;
			block.z |= indices[3] << 26;
			block.z |= indices[4] << 29;
			block.w |= indices[5] << 0;
			block.w |= indices[6] << 3;
			block.w |= indices[7] << 6;
			block.w |= indices[8] << 9;
			block.w |= indices[9] << 11;
			block.w |= indices[10] << 14;
			block.w |= indices[11] << 17;
			block.w |= indices[12] << 20;
			block.w |= indices[13] << 23;
			block.w |= indices[14] << 26;
			block.w |= indices[15] << 29;
		}
	}
}

layout (local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

void main()
{
	u32vec2 blockCoord = gl_GlobalInvocationID.xy;

	if (all(lessThan(blockCoord, pc.TextureSizeInBlocks)))
	{
		// Gather texels for current 4x4 block
		// 0 1 2 3
		// 4 5 6 7
		// 8 9 10 11
		// 12 13 14 15
		f32vec2 uv = blockCoord * pc.TextureSizeRcp * 4.0f + pc.TextureSizeRcp;
		f32vec2 block0UV = uv;
		f32vec2 block1UV = uv + f32vec2(2.0f * pc.TextureSizeRcp.x, 0.0f);
		f32vec2 block2UV = uv + f32vec2(0.0f, 2.0f * pc.TextureSizeRcp.y);
		f32vec2 block3UV = uv + f32vec2(2.0f * pc.TextureSizeRcp.x, 2.0f * pc.TextureSizeRcp.y);
		f32vec4  block0X = textureGatherX(_src_texture, pc.point_sampler, block0UV);
		f32vec4  block1X = textureGatherX(_src_texture, pc.point_sampler, block1UV);
		f32vec4  block2X = textureGatherX(_src_texture, pc.point_sampler, block2UV);
		f32vec4  block3X = textureGatherX(_src_texture, pc.point_sampler, block3UV);
		f32vec4  block0Y = textureGatherY(_src_texture, pc.point_sampler, block0UV);
		f32vec4  block1Y = textureGatherY(_src_texture, pc.point_sampler, block1UV);
		f32vec4  block2Y = textureGatherY(_src_texture, pc.point_sampler, block2UV);
		f32vec4  block3Y = textureGatherY(_src_texture, pc.point_sampler, block3UV);
		f32vec4  block0Z = textureGatherZ(_src_texture, pc.point_sampler, block0UV);
		f32vec4  block1Z = textureGatherZ(_src_texture, pc.point_sampler, block1UV);
		f32vec4  block2Z = textureGatherZ(_src_texture, pc.point_sampler, block2UV);
		f32vec4  block3Z = textureGatherZ(_src_texture, pc.point_sampler, block3UV);

		f32vec3 texels[16];
		texels[0] = f32vec3(block0X.w, block0Y.w, block0Z.w);
		texels[1] = f32vec3(block0X.z, block0Y.z, block0Z.z);
		texels[2] = f32vec3(block1X.w, block1Y.w, block1Z.w);
		texels[3] = f32vec3(block1X.z, block1Y.z, block1Z.z);

		texels[4] = f32vec3(block0X.x, block0Y.x, block0Z.x);
		texels[5] = f32vec3(block0X.y, block0Y.y, block0Z.y);
		texels[6] = f32vec3(block1X.x, block1Y.x, block1Z.x);
		texels[7] = f32vec3(block1X.y, block1Y.y, block1Z.y);

		texels[8] = f32vec3(block2X.w, block2Y.w, block2Z.w);
		texels[9] = f32vec3(block2X.z, block2Y.z, block2Z.z);
		texels[10] = f32vec3(block3X.w, block3Y.w, block3Z.w);
		texels[11] = f32vec3(block3X.z, block3Y.z, block3Z.z);

		texels[12] = f32vec3(block2X.x, block2Y.x, block2Z.x);
		texels[13] = f32vec3(block2X.y, block2Y.y, block2Z.y);
		texels[14] = f32vec3(block3X.x, block3Y.x, block3Z.x);
		texels[15] = f32vec3(block3X.y, block3Y.y, block3Z.y);

		u32vec4 block = u32vec4(0, 0, 0, 0);
		f32 blockMSLE = 0.0f;

		EncodeP1(block, blockMSLE, texels);

#if ENCODE_P2
		// First find pattern which is a best fit for a current block
		f32 bestScore = EvaluateP2Pattern(0, texels);
		u32 bestPattern = 0;

		for (u32 patternIndex = 1; patternIndex < 32; ++patternIndex)
		{
			f32 score = EvaluateP2Pattern(patternIndex, texels);
			if (score < bestScore)
			{
				bestPattern = patternIndex;
				bestScore = score;
			}
		}

		// Then encode it
		EncodeP2Pattern(block, blockMSLE, bestPattern, texels);
#endif

        imageStore(_dst_texture, i32vec2(blockCoord), block);
	}
}