#define DAXA_ENABLE_SHADER_NO_NAMESPACE 1
#include <shared/shared.inl>

DAXA_USE_PUSH_CONSTANT(DrawTerrainPC)
daxa_BufferPtr(TerrainVertex) terrain_vertices = daxa_push_constant.vertices;
daxa_BufferPtr(CameraParameters) camera_parameters = daxa_push_constant.camera_parameters;

#if defined(_VERTEX)
// ===================== VERTEX SHADER ===============================
void main()
{
    f32vec3 scale = daxa_push_constant.terrain_scale;
    const f32vec4 pre_trans_pos = f32vec4(deref(terrain_vertices[gl_VertexIndex]).position, 1.0);
    const f32vec4 pre_trans_scaled_pos = f32vec4(pre_trans_pos.xyz * scale, 1.0);

    f32mat4x4 m_proj_view_model = deref(camera_parameters).projection * deref(camera_parameters).view;
    gl_Position = m_proj_view_model * pre_trans_scaled_pos;
}

#elif defined(_FRAGMENT)
// ===================== FRAGMENT SHADER ===============================
layout (location = 0) out f32vec4 out_color;

void main()
{
    out_color = f32vec4(1.0, 0.0, 0.0, 1.0);
}
#endif