#define DAXA_SHADER_NO_NAMESPACE
#include <shared/shared.inl>

layout (location = 0) out f32vec2 outUV;

f32vec2 vertices[3] = f32vec2[](
    f32vec2( 1.0, -3.0),
    f32vec2( 1.0,  1.0),
    f32vec2(-3.0,  1.0)
);

void main()
{
    outUV = f32vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(outUV * 2.0 - 1.0, 0.0, 1.0);
}