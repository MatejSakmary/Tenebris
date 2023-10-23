#include "camera.hpp"

#include <fstream>

auto constexpr static inline daxa_vec3_to_glm(daxa_f32vec3 vec) -> glm::vec3 { return glm::vec3(vec.x, vec.y, vec.z); }

Camera::Camera(const CameraInfo & info) : 
    offset{daxa_i32vec3{0, 0, 0}},
    matrix_dirty{true},
    position{daxa_vec3_to_glm(info.position)},
    front{daxa_vec3_to_glm(info.front)},
    up{daxa_vec3_to_glm(info.up)},
    proj_info{info.projection_info},
    speed{100.0f},
    pitch{10.0f},
    sensitivity{0.08f},
    roll_sensitivity{20.0f}
{
}

void Camera::set_position(daxa_f32vec3 new_position)
{
    position = daxa_vec3_to_glm(new_position);
    offset = daxa_i32vec3{0, 0, 0};
    move_camera(0.0f, Direction::UP, false);
    matrix_dirty = true;
}

void Camera::set_front(daxa_f32vec3 new_front)
{
    front = daxa_vec3_to_glm(new_front);
    matrix_dirty = true;
}

void Camera::move_camera(daxa_f32 delta_time, Direction direction, bool sped_up)
{
    if(sped_up) { speed *= 10.0; }
    switch (direction)
    {
    case Direction::FORWARD:
        position += front * speed * delta_time;
        break;
    case Direction::BACK:
        position -= front * speed * delta_time;
        break;
    case Direction::LEFT:
        position -= glm::normalize(glm::cross(front, up)) * speed * delta_time;
        break;
    case Direction::RIGHT:
        position += glm::normalize(glm::cross(front, up)) * speed * delta_time;
        break;
    case Direction::UP:
        position += glm::normalize(glm::cross(glm::cross(front,up), front)) * speed * delta_time;
        break;
    case Direction::DOWN:
        position -= glm::normalize(glm::cross(glm::cross(front,up), front)) * speed * delta_time;
        break;
    case Direction::ROLL_LEFT:
        up = glm::rotate(up, static_cast<daxa_f32>(glm::radians(-roll_sensitivity * delta_time)), front);
        break;
    case Direction::ROLL_RIGHT:
        up = glm::rotate(up, static_cast<daxa_f32>(glm::radians(roll_sensitivity * delta_time)), front);
        break;
    
    default:
        DEBUG_OUT("[Camera::move_camera()] Unknown enum value");
        break;
    }
    if(sped_up) { speed /= 10.0; }
    if(glm::abs(position.x) > 1.0f || glm::abs(position.y) > 1.0f || glm::abs(position.z) > 1.0f)
    {
        glm::vec3 fract = glm::fract(position);
        glm::ivec3 tmp_pos = glm::ivec3(position - fract);
        offset = daxa_i32vec3(
            offset.x - tmp_pos.x,
            offset.y - tmp_pos.y,
            offset.z - tmp_pos.z
        );
        position = fract;
    }
    matrix_dirty = true;
}

void Camera::update_front_vector(daxa_f32 x_offset, daxa_f32 y_offset)
{
    glm::vec3 front_ = glm::rotate(front, glm::radians(-sensitivity * x_offset), up);
    front_ = glm::rotate(front_, glm::radians(-sensitivity * y_offset), glm::cross(front,up));

    pitch = glm::degrees(glm::angle(front_, up));

    const daxa_f32 MAX_PITCH_ANGLE = 179.9f;
    const daxa_f32 MIN_PITCH_ANGLE = 0.01f;
    if (pitch < MIN_PITCH_ANGLE || pitch > MAX_PITCH_ANGLE )
    {
        return;
    }

    front = front_;
    matrix_dirty = true;
}

void Camera::recalculate_matrices()
{
    if(std::holds_alternative<OrthographicInfo>(proj_info))
    {
        const auto & ortho_info = std::get<OrthographicInfo>(proj_info);
        projection = glm::ortho(
            ortho_info.left, 
            ortho_info.right,
            ortho_info.bottom,
            ortho_info.top,
            ortho_info.near,
            ortho_info.far
        );
        /* GLM is using OpenGL standard where Y coordinate of the clip coordinates is inverted */
        projection[1][1] *= -1.0;
    } 
    else 
    {
        const auto & persp_info = std::get<PerspectiveInfo>(proj_info);
        // Infinite far plane
        daxa_f32 const tan_half_fovy = 1.0f / glm::tan(persp_info.fov * 0.5f);

        projection = glm::mat4x4(0.0f);
        projection[0][0] =  tan_half_fovy / persp_info.aspect_ratio;
        /* GLM is using OpenGL standard where Y coordinate of the clip coordinates is inverted */
        projection[1][1] = -tan_half_fovy;
        projection[2][2] =  0.0f;
        projection[2][3] = -1.0f;
        projection[3][2] =  persp_info.near_plane;
    }

    view = glm::lookAt(position, position + front, up);

    matrix_dirty = false;
}

auto Camera::get_view_matrix() -> daxa_f32mat4x4
{
    if(matrix_dirty) { recalculate_matrices(); }
    return daxa_f32mat4x4(
        daxa_f32vec4{view[0][0], view[0][1], view[0][2], view[0][3]},
        daxa_f32vec4{view[1][0], view[1][1], view[1][2], view[1][3]},
        daxa_f32vec4{view[2][0], view[2][1], view[2][2], view[2][3]},
        daxa_f32vec4{view[3][0], view[3][1], view[3][2], view[3][3]}
    );
}

auto Camera::get_projection_matrix() -> daxa_f32mat4x4
{
    if(matrix_dirty) { recalculate_matrices(); }
    return daxa_f32mat4x4(
        daxa_f32vec4{projection[0][0], projection[0][1], projection[0][2], projection[0][3]},
        daxa_f32vec4{projection[1][0], projection[1][1], projection[1][2], projection[1][3]},
        daxa_f32vec4{projection[2][0], projection[2][1], projection[2][2], projection[2][3]},
        daxa_f32vec4{projection[3][0], projection[3][1], projection[3][2], projection[3][3]}
    );
}

auto Camera::get_projection_view_matrix() -> daxa_f32mat4x4
{
    if(matrix_dirty) { recalculate_matrices(); }
    glm::mat4x4 projection_view = projection * view;
    return daxa_f32mat4x4(
        daxa_f32vec4{projection_view[0][0], projection_view[0][1], projection_view[0][2], projection_view[0][3]},
        daxa_f32vec4{projection_view[1][0], projection_view[1][1], projection_view[1][2], projection_view[1][3]},
        daxa_f32vec4{projection_view[2][0], projection_view[2][1], projection_view[2][2], projection_view[2][3]},
        daxa_f32vec4{projection_view[3][0], projection_view[3][1], projection_view[3][2], projection_view[3][3]}
    );
}

auto Camera::get_inv_projection_matrix() -> daxa_f32mat4x4
{
    if(matrix_dirty) { recalculate_matrices(); }
    auto inv_projection = glm::inverse(projection);
    return daxa_f32mat4x4(
        daxa_f32vec4{inv_projection[0][0], inv_projection[0][1], inv_projection[0][2], inv_projection[0][3]},
        daxa_f32vec4{inv_projection[1][0], inv_projection[1][1], inv_projection[1][2], inv_projection[1][3]},
        daxa_f32vec4{inv_projection[2][0], inv_projection[2][1], inv_projection[2][2], inv_projection[2][3]},
        daxa_f32vec4{inv_projection[3][0], inv_projection[3][1], inv_projection[3][2], inv_projection[3][3]}
    );
}

auto Camera::get_inv_view_proj_matrix() -> daxa_f32mat4x4
{
    if(matrix_dirty) { recalculate_matrices(); }
    auto inv_projection_view = glm::inverse(projection * view);

    return daxa_f32mat4x4(
        daxa_f32vec4{inv_projection_view[0][0], inv_projection_view[0][1], inv_projection_view[0][2], inv_projection_view[0][3]},
        daxa_f32vec4{inv_projection_view[1][0], inv_projection_view[1][1], inv_projection_view[1][2], inv_projection_view[1][3]},
        daxa_f32vec4{inv_projection_view[2][0], inv_projection_view[2][1], inv_projection_view[2][2], inv_projection_view[2][3]},
        daxa_f32vec4{inv_projection_view[3][0], inv_projection_view[3][1], inv_projection_view[3][2], inv_projection_view[3][3]}
    );
}

auto Camera::get_frustum_info() -> CameraFrustumInfo
{
    if(std::holds_alternative<OrthographicInfo>(proj_info))
    {
        throw std::runtime_error(
            "[Camera::get_frustum_info()] Unable to get frustum info for orthographic camera"
        );
        return {};
    }

    const auto & persp_info = std::get<PerspectiveInfo>(proj_info);

    glm::vec3 right;
    if(front.x != 0 && front.y != 0)      { right = glm::vec3(front.y, -front.x, 0.0f); }
    else if(front.x != 0 && front.z != 0) { right = glm::vec3(-front.z, 0.0f, front.x); }
    else                                  { right = glm::vec3(0.0f, -front.z, front.y); }

    glm::vec3 up_ = glm::normalize(glm::cross(right, front));
    glm::vec3 right_ = glm::normalize(glm::cross(front, up));

    daxa_f32 fov_tan = glm::tan(persp_info.fov / 2.0f);

    auto right_aspect_fov_correct = right_ * persp_info.aspect_ratio * fov_tan;
    auto up_fov_correct = glm::normalize(up_) * fov_tan;

    auto glm_vec_to_daxa = [](glm::vec3 v) -> daxa_f32vec3 { return {v.x, v.y, v.z}; };

    return {
        .forward = glm_vec_to_daxa(front),
        .top_frustum_offset = glm_vec_to_daxa(-up_fov_correct),
        .right_frustum_offset = glm_vec_to_daxa(right_aspect_fov_correct)
    };
}

void Camera::write_frustum_vertices(WriteVerticesInfo const & info)
{

    auto glm_vec_to_daxa = [](glm::vec3 v) -> daxa_f32vec3 { return {v.x, v.y, v.z}; };
    static constexpr std::array offsets = {
        glm::ivec2(-1,  1), glm::ivec2(-1, -1), glm::ivec2( 1, -1), glm::ivec2( 1,  1),
        glm::ivec2( 1,  1), glm::ivec2(-1,  1), glm::ivec2(-1, -1), glm::ivec2( 1, -1)
    };

    if(matrix_dirty) { recalculate_matrices(); }

    // Orthographic camera
    if(std::holds_alternative<OrthographicInfo>(proj_info))
    {
        const auto & ortho_info = std::get<OrthographicInfo>(proj_info);

        for(int i = 0; i < 8; i++)
        {
            const auto ndc_pos = glm::vec4(offsets[i], i < 4 ? 0.0f : 1.0f, 1.0);
            const glm::vec4 unproj_world_space =  glm::inverse(projection * view) * ndc_pos;
            const glm::vec3 world_space = glm::vec3(
                unproj_world_space.x / unproj_world_space.w,
                unproj_world_space.y / unproj_world_space.w,
                unproj_world_space.z / unproj_world_space.w
            );
            info.vertices_dst[i].vertex = glm_vec_to_daxa(world_space - glm::vec3(offset.x, offset.y, offset.z));
        }
    }
    // Perspective camera
    else 
    {
        const auto & persp_info = std::get<PerspectiveInfo>(proj_info);

        auto [front, top, right] = get_frustum_info();
        const auto glm_front = glm::vec3(front.x, front.y, front.z);
        const auto glm_right = glm::vec3(right.x, right.y, right.z);
        const auto glm_top = glm::vec3(top.x, top.y, top.z);

        const daxa_f32 max_dist = 20'000.0f;
        const auto camera_pos_world = position - glm::vec3(offset.x, offset.y, offset.z);

        for(int i = 0; i < 8; i++)
        {
            glm::vec3 dir_vec = glm_front + daxa_f32(offsets[i].x) * (-glm_right) + daxa_f32(offsets[i].y) * glm_top;
            daxa_f32 multiplier = i < 4 ? persp_info.near_plane : max_dist;
            info.vertices_dst[i].vertex = glm_vec_to_daxa(camera_pos_world + dir_vec * multiplier);
        }
    }
};

auto Camera::get_camera_position() const -> daxa_f32vec3
{
    return daxa_f32vec3{position.x, position.y, position.z};
}

auto Camera::align_clip_to_player(
        Camera const * player_camera,
        daxa_f32vec3 sun_offset,
        std::span<FrustumVertex, VSM_PAGE_TABLE_RESOLUTION * VSM_PAGE_TABLE_RESOLUTION * 8> vertices_space,
        bool view_page_frusti,
        daxa_i32 sun_offset_factor
    ) -> ClipAlignInfo
{
    const daxa_f32vec3 foffset_player_position = player_camera->get_camera_position(); 

    const daxa_i32vec3 iplayer_offset = player_camera->offset;
    const daxa_f32vec3 fplayer_offset = daxa_f32vec3 { 
        static_cast<daxa_f32>(iplayer_offset.x),
        static_cast<daxa_f32>(iplayer_offset.y),
        static_cast<daxa_f32>(iplayer_offset.z) 
    };

    set_position(daxa_f32vec3{0.0f, 0.0f, 0.0f});
    recalculate_matrices();

    const glm::vec4 glm_player_position = glm::vec4(
        foffset_player_position.x - fplayer_offset.x - offset.x,
        foffset_player_position.y - fplayer_offset.y - offset.y,
        foffset_player_position.z - fplayer_offset.z - offset.z,
        1.0
    );

    const auto projected_player_position = projection * view * glm_player_position;
    const auto ndc_player_position = glm::vec3(
        projected_player_position.x / glm_player_position.w,
        projected_player_position.y / glm_player_position.w,
        projected_player_position.z / glm_player_position.w
    );
    // NOTE(msakmary) We need to multiply by two because we were converting from NDC space which is [-1, 1] and not uv spcae
    const daxa_f32 ndc_page_size = static_cast<daxa_f32>(VSM_PAGE_SIZE * 2) / static_cast<daxa_f32>(VSM_TEXTURE_RESOLUTION);
    const auto ndc_page_scaled_player_position = glm::vec2(
        ndc_player_position.x / ndc_page_size,
        ndc_player_position.y / ndc_page_size
    );
    const auto ndc_page_scaled_aligned_player_position = glm::vec2(
        std::ceil(ndc_page_scaled_player_position.x), 
        std::ceil(ndc_page_scaled_player_position.y)
    );

    /*const*/ auto ortho_info = std::get<OrthographicInfo>(proj_info);

    const auto near_offset_ndc_u_in_world = glm::inverse(projection * view) * glm::vec4(ndc_page_size, 0.0, 0.0, 1.0);
    const auto near_offset_ndc_v_in_world = glm::inverse(projection * view) * glm::vec4(0.0, ndc_page_size, 0.0, 1.0);

    const auto ndc_u_in_world = glm::vec3(
        near_offset_ndc_u_in_world.x + ortho_info.near * sun_offset.x,
        near_offset_ndc_u_in_world.y + ortho_info.near * sun_offset.y,
        near_offset_ndc_u_in_world.z + ortho_info.near * sun_offset.z
    );

    const auto ndc_v_in_world = glm::vec3(
        near_offset_ndc_v_in_world.x + ortho_info.near * sun_offset.x,
        near_offset_ndc_v_in_world.y + ortho_info.near * sun_offset.y,
        near_offset_ndc_v_in_world.z + ortho_info.near * sun_offset.z
    );


    const daxa_f32 x_offset = -(ndc_u_in_world.z / sun_offset.z);
    const auto x_offset_vector = x_offset * glm::vec3(sun_offset.x, sun_offset.y, sun_offset.z);
    const auto on_plane_ndc_u_in_world = ndc_u_in_world + x_offset_vector;

    const daxa_f32 y_offset = -(ndc_v_in_world.z / sun_offset.z);
    const auto y_offset_vector = y_offset * glm::vec3(sun_offset.x, sun_offset.y, sun_offset.z);
    const auto on_plane_ndc_v_in_world = ndc_v_in_world + y_offset_vector;

    const auto new_on_plane_position = glm::vec3(
        ndc_page_scaled_aligned_player_position.x * on_plane_ndc_u_in_world +
        ndc_page_scaled_aligned_player_position.y * on_plane_ndc_v_in_world
    );

    
    const auto sun_height_offset = static_cast<daxa_i32>(glm::floor(glm_player_position.z / (sun_offset.z)) + sun_offset_factor);
    const auto scaled_sun_offset = static_cast<daxa_f32>(sun_height_offset) * glm::vec3(sun_offset.x, sun_offset.y, sun_offset.z);
    const auto new_position = new_on_plane_position + scaled_sun_offset;

    const auto modified_info = OrthographicInfo{
        .left   = ortho_info.left   / VSM_PAGE_TABLE_RESOLUTION,
        .right  = ortho_info.right  / VSM_PAGE_TABLE_RESOLUTION,
        .top    = ortho_info.top    / VSM_PAGE_TABLE_RESOLUTION,
        .bottom = ortho_info.bottom / VSM_PAGE_TABLE_RESOLUTION,
        .near   = ortho_info.near,
        .far    = ortho_info.far,
    };

    set_position(daxa_f32vec3{new_position.x, new_position.y, new_position.z});
    recalculate_matrices();

    const auto origin_shift = (projection * view * glm::vec4(0.0, 0.0, 0.0, 1.0)).z;
    const auto per_height_unit_depth_offset = 
        ((projection * view) * glm::vec4(sun_offset.x * 1000.0f, sun_offset.y * 1000.0f, sun_offset.z * 1000.0f, 1.0)).z - origin_shift;
    const auto page_x_depth_offset = ((projection * view) * glm::vec4(x_offset_vector, 1.0)).z - origin_shift;
    const auto page_y_depth_offset = ((projection * view) * glm::vec4(y_offset_vector, 1.0)).z - origin_shift;

    const daxa_f32 page_uv_size = ndc_page_size / 2.0;
    if(view_page_frusti)
    {
        for(daxa_i32 page_tex_u = 0; page_tex_u < VSM_PAGE_TABLE_RESOLUTION; page_tex_u++)
        {
            for(int page_tex_v = 0; page_tex_v < VSM_PAGE_TABLE_RESOLUTION; page_tex_v++)
            {
                const auto corner_virtual_uv = page_uv_size * glm::vec2(page_tex_u, page_tex_v);
                const auto page_center_virtual_uv_offset = glm::vec2(page_uv_size * 0.5f);
                const auto virtual_uv = corner_virtual_uv + page_center_virtual_uv_offset;

                const auto page_index = glm::ivec2(virtual_uv * daxa_f32(VSM_PAGE_TABLE_RESOLUTION));

                const daxa_f32 depth = 
                    ((VSM_PAGE_TABLE_RESOLUTION - 1) - page_index.x) * page_x_depth_offset +
                    ((VSM_PAGE_TABLE_RESOLUTION - 1) - page_index.y) * page_y_depth_offset;
                const auto virtual_page_ndc = (virtual_uv * 2.0f) - glm::vec2(1.0f);

                const auto page_ndc_position = glm::vec4(virtual_page_ndc, -depth, 1.0); 
                const auto offset_new_position = inverse(projection * view) * page_ndc_position;
                const auto _new_position = glm::vec3(
                    offset_new_position.x - offset.x + ortho_info.near * sun_offset.x,
                    offset_new_position.y - offset.y + ortho_info.near * sun_offset.y,
                    offset_new_position.z - offset.z + ortho_info.near * sun_offset.z);

                proj_info = modified_info;
                set_position(daxa_f32vec3{_new_position.x, _new_position.y, _new_position.z});
                write_frustum_vertices({
                    .vertices_dst = std::span<FrustumVertex, 8>{&vertices_space[(page_tex_u * VSM_PAGE_TABLE_RESOLUTION + page_tex_v) * 8], 8}
                });

                proj_info = ortho_info;
                set_position(daxa_f32vec3{new_position.x, new_position.y, new_position.z});
                recalculate_matrices();
            }
        }
    }

    proj_info = ortho_info;
    set_position(daxa_f32vec3{new_position.x, new_position.y, new_position.z});
    return ClipAlignInfo{
        .per_page_depth_offset = daxa_f32vec2{page_x_depth_offset, page_y_depth_offset},
        .page_offset = daxa_i32vec2{
            -static_cast<daxa_i32>(ndc_page_scaled_aligned_player_position.x),
            -static_cast<daxa_i32>(ndc_page_scaled_aligned_player_position.y)
        },
        .sun_height_offset = sun_height_offset,
        .per_height_unit_depth_offset = per_height_unit_depth_offset
    };
}