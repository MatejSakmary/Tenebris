#define DAXA_ENABLE_SHADER_NO_NAMESPACE 1
#include <shared/shared.inl>

DAXA_USE_PUSH_CONSTANT(GeneratePoissonPC)
daxa_BufferPtr(PoissonPoint) poisson_point = daxa_push_constant.poisson_points;
daxa_BufferPtr(PoissonHeader) poisson_header = daxa_push_constant.poisson_header;

#if defined(_VERTEX)
// ===================== VERTEX SHADER ===============================
layout (location = 0) out flat f32vec3 pos;

u32 pcg_hash(u32 input_)
{
    u32 state = input_ * 747796405u + 2891336453u;
    u32 word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

void main()
{
    gl_PointSize = 10.0;
    u32 rand_1 = pcg_hash(gl_VertexIndex);
    u32 rand_2 = pcg_hash(rand_1);
    u32 rand_3 = pcg_hash(rand_2);

    pos = f32vec3(rand_1, rand_2, rand_3) / 0xFFFFFFFFu;
    pos.xy *= 2.0;
    pos.xy -= 1.0;
    gl_Position = f32vec4(pos, 1.0);
}

#elif defined(_FRAGMENT)
// ===================== FRAGMENT SHADER ===============================
layout (location = 0) in flat f32vec3 pos;
layout (location = 0) out f32 out_color;

void main()
{
    f32vec2 curr_frag_pos = gl_FragCoord.xy;
    f32vec3 pos_ = ((pos + 1.0) / 2.0) * 1000.0;
    f32 dist = distance(curr_frag_pos, pos_.xy);
    if(dist > 5)
    {
        discard;
    }
    out_color = f32(1.0);
}
#endif