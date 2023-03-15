#define DAXA_ENABLE_SHADER_NO_NAMESPACE 1
#include <shared/shared.inl>

layout (location = 0) out f32vec2 out_uv;

f32vec2 vertices[3] = f32vec2[](
    f32vec2( 1.0, -3.0),
    f32vec2( 1.0,  1.0),
    f32vec2(-3.0,  1.0)
);

void main()
{
    gl_Position = vec4(vertices[gl_VertexIndex], 1.0, 1.0);
    out_uv = (vertices[gl_VertexIndex] + 1.0) * 0.5;
}