#define DAXA_ENABLE_SHADER_NO_NAMESPACE 1
#define DAXA_ENABLE_IMAGE_OVERLOADS_BASIC 1
#include <shared/shared.inl>
#include "tasks/debug_draw_frustum.inl"

#if DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_VERTEX
layout (location = 0) out f32vec3 color;
void main()
{
    const u32 instance = gl_InstanceIndex; 
    const u32 offset = instance * FRUSTUM_VERTEX_COUNT;
    const u32 vertex_index = gl_VertexIndex + offset;

    const f32vec3 position = deref(_frustum_vertices[vertex_index]).vertex;
    const f32vec4 offset_position = f32vec4(position.xyz + deref(_globals).offset.xyz, 1.0);

    const f32mat4x4 projection_view = deref(_globals).projection * deref(_globals).view;

    color = deref(_frustum_colors[instance]).color;
    gl_Position = projection_view * offset_position;

}
#elif DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_FRAGMENT
layout (location = 0) in f32vec3 color;
layout (location = 0) out f32vec4 color_out;

void main()
{
    color_out = f32vec4(color, 1.0);
}
#endif