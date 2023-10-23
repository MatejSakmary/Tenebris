#include <shared/shared.inl>

layout (location = 0) out daxa_f32vec2 out_uv;

daxa_f32vec2 vertices[3] = daxa_f32vec2[](
    daxa_f32vec2( 1.0, -3.0),
    daxa_f32vec2( 1.0,  1.0),
    daxa_f32vec2(-3.0,  1.0)
);

void main()
{
    gl_Position = vec4(vertices[gl_VertexIndex], 1.0, 1.0);
    out_uv = (vertices[gl_VertexIndex] + 1.0) * 0.5;
}