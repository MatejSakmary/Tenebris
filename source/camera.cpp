#include "camera.hpp"

#include <fstream>

auto constexpr static inline daxa_vec3_to_glm(f32vec3 vec) -> glm::vec3 { return glm::vec3(vec.x, vec.y, vec.z); }

Camera::Camera(const CameraInfo & info) : 
    offset{i32vec3{0, 0, 0}},
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

void Camera::set_position(f32vec3 new_position)
{
    position = daxa_vec3_to_glm(new_position);
    offset = i32vec3{0, 0, 0};
    move_camera(0.0f, Direction::UP, false);
    matrix_dirty = true;
}

void Camera::set_front(f32vec3 new_front)
{
    front = daxa_vec3_to_glm(new_front);
    matrix_dirty = true;
}

void Camera::move_camera(f32 delta_time, Direction direction, bool sped_up)
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
        up = glm::rotate(up, static_cast<f32>(glm::radians(-roll_sensitivity * delta_time)), front);
        break;
    case Direction::ROLL_RIGHT:
        up = glm::rotate(up, static_cast<f32>(glm::radians(roll_sensitivity * delta_time)), front);
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
        offset = offset - vec_from_span<i32, 3>(std::span<i32, 3>{glm::value_ptr(tmp_pos), 3});
        position = fract;
    }
    matrix_dirty = true;
}

void Camera::update_front_vector(f32 x_offset, f32 y_offset)
{
    glm::vec3 front_ = glm::rotate(front, glm::radians(-sensitivity * x_offset), up);
    front_ = glm::rotate(front_, glm::radians(-sensitivity * y_offset), glm::cross(front,up));

    pitch = glm::degrees(glm::angle(front_, up));

    const f32 MAX_PITCH_ANGLE = 179.9f;
    const f32 MIN_PITCH_ANGLE = 0.01f;
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
        f32 const tan_half_fovy = 1.0f / glm::tan(persp_info.fov * 0.5f);

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

auto Camera::get_view_matrix() -> f32mat4x4
{
    if(matrix_dirty) { recalculate_matrices(); }
    return mat_from_span<f32, 4, 4>(std::span<f32, 4 * 4>{ glm::value_ptr(view), 4 * 4 });
}

auto Camera::get_projection_matrix() -> f32mat4x4
{
    if(matrix_dirty) { recalculate_matrices(); }
    return mat_from_span<f32, 4, 4>(std::span<f32, 4 * 4>{ glm::value_ptr(projection), 4 * 4 });
}

auto Camera::get_projection_view_matrix() -> f32mat4x4
{
    if(matrix_dirty) { recalculate_matrices(); }
    glm::mat4x4 tmp_result = projection * view;
    return mat_from_span<f32, 4, 4>(std::span<f32, 4 * 4>{ glm::value_ptr(tmp_result), 4 * 4 });
}

auto Camera::get_inv_projection_matrix() -> f32mat4x4
{
    if(matrix_dirty) { recalculate_matrices(); }
    auto inv_projection = glm::inverse(projection);
    return mat_from_span<f32, 4, 4>(std::span<f32, 4 * 4>{ glm::value_ptr(inv_projection), 4 * 4 });
}

auto Camera::get_inv_view_proj_matrix() -> f32mat4x4
{
    if(matrix_dirty) { recalculate_matrices(); }
    auto inv_proj_view_mat = glm::inverse(projection * view);

    return mat_from_span<f32, 4, 4>(std::span<f32, 4 * 4>{ glm::value_ptr(inv_proj_view_mat), 4 * 4 });
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

    f32 fov_tan = glm::tan(persp_info.fov / 2.0f);

    auto right_aspect_fov_correct = right_ * persp_info.aspect_ratio * fov_tan;
    auto up_fov_correct = glm::normalize(up_) * fov_tan;

    auto glm_vec_to_daxa = [](glm::vec3 v) -> f32vec3 { return {v.x, v.y, v.z}; };

    return {
        .forward = glm_vec_to_daxa(front),
        .top_frustum_offset = glm_vec_to_daxa(-up_fov_correct),
        .right_frustum_offset = glm_vec_to_daxa(right_aspect_fov_correct)
    };
}

void Camera::write_frustum_vertices(WriteVerticesInfo const & info)
{

    auto glm_vec_to_daxa = [](glm::vec3 v) -> f32vec3 { return {v.x, v.y, v.z}; };
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

        const f32 max_dist = 20'000.0f;
        const auto camera_pos_world = position - glm::vec3(offset.x, offset.y, offset.z);

        for(int i = 0; i < 8; i++)
        {
            glm::vec3 dir_vec = glm_front + f32(offsets[i].x) * (-glm_right) + f32(offsets[i].y) * glm_top;
            f32 multiplier = i < 4 ? persp_info.near_plane : max_dist;
            info.vertices_dst[i].vertex = glm_vec_to_daxa(camera_pos_world + dir_vec * multiplier);
        }
    }
};

auto Camera::get_camera_position() const -> f32vec3
{
    return f32vec3{position.x, position.y, position.z};
}

auto Camera::align_clip_to_player(Camera const * player_camera, f32vec3 sun_offset, std::span<FrustumVertex, 128> vertices_space) -> ClipAlignInfo
{
    const f32vec3 foffset_player_position = player_camera->get_camera_position(); 

    const i32vec3 iplayer_offset = player_camera->offset;
    const f32vec3 fplayer_offset = f32vec3 { 
        static_cast<f32>(iplayer_offset.x),
        static_cast<f32>(iplayer_offset.y),
        static_cast<f32>(iplayer_offset.z) 
    };

    const f32vec3 fplayer_position = foffset_player_position - fplayer_offset;
    set_position(f32vec3{0.0f, 0.0f, 0.0f});
    recalculate_matrices();

    const glm::vec4 glm_player_position = glm::vec4(
        fplayer_position.x - offset.x,
        fplayer_position.y - offset.y,
        fplayer_position.z - offset.z,
        1.0
    );

    const auto projected_player_position = projection * view * glm_player_position;
    const auto ndc_player_position = glm::vec3(
        projected_player_position.x / glm_player_position.w,
        projected_player_position.y / glm_player_position.w,
        projected_player_position.z / glm_player_position.w
    );
    // NOTE(msakmary) We need to multiply by two because we were converting from NDC space which is [-1, 1] and not uv spcae
    const f32 ndc_page_size = static_cast<f32>(VSM_PAGE_SIZE * 2) / static_cast<f32>(VSM_TEXTURE_RESOLUTION);
    const auto ndc_page_scaled_player_position = glm::vec2(
        ndc_player_position.x / ndc_page_size,
        ndc_player_position.y / ndc_page_size
    );
    const auto ndc_page_scaled_aligned_player_position = glm::vec2(
        std::ceil(ndc_page_scaled_player_position.x), 
        std::ceil(ndc_page_scaled_player_position.y)
    );

    const auto ortho_info = std::get<OrthographicInfo>(proj_info);

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


    const f32 x_offset = -(ndc_u_in_world.z / sun_offset.z);
    const auto x_offset_vector = x_offset * glm::vec3(sun_offset.x, sun_offset.y, sun_offset.z);
    const auto on_plane_ndc_u_in_world = ndc_u_in_world + x_offset_vector;

    const f32 y_offset = -(ndc_v_in_world.z / sun_offset.z);
    const auto y_offset_vector = y_offset * glm::vec3(sun_offset.x, sun_offset.y, sun_offset.z);
    const auto on_plane_ndc_v_in_world = ndc_v_in_world + y_offset_vector;

    const auto new_on_plane_position = glm::vec3(
        ndc_page_scaled_aligned_player_position.x * on_plane_ndc_u_in_world +
        ndc_page_scaled_aligned_player_position.y * on_plane_ndc_v_in_world
    );

    const auto scaled_sun_offset = 1000.0f * glm::vec3(sun_offset.x, sun_offset.y, sun_offset.z);
    const auto new_position = new_on_plane_position + scaled_sun_offset;

    const auto modified_info = OrthographicInfo{
        .left   = ortho_info.left   / 4.0f,
        .right  = ortho_info.right  / 4.0f,
        .top    = ortho_info.top    / 4.0f,
        .bottom = ortho_info.bottom / 4.0f,
        .near   = ortho_info.near,
        .far    = ortho_info.far,
    };

    set_position(f32vec3{new_position.x, new_position.y, new_position.z});
    recalculate_matrices();

    const auto origin_shift = (projection * view * glm::vec4(0.0, 0.0, 0.0, 1.0)).z;
    const auto page_x_depth_offset = ((projection * view) * glm::vec4(x_offset_vector, 1.0)).z - origin_shift;
    const auto page_y_depth_offset = ((projection * view) * glm::vec4(y_offset_vector, 1.0)).z - origin_shift;

    const f32 page_uv_size = ndc_page_size / 2.0;
    for(i32 page_tex_u = 0; page_tex_u < 4; page_tex_u++)
    {
        for(int page_tex_v = 0; page_tex_v < 4; page_tex_v++)
        {
            const auto corner_virtual_uv = page_uv_size * glm::vec2(page_tex_u, page_tex_v);
            const auto page_center_virtual_uv_offset = glm::vec2(page_uv_size * 0.5f);
            const auto virtual_uv = corner_virtual_uv + page_center_virtual_uv_offset;

            const auto page_index = glm::ivec2(virtual_uv * f32(VSM_PAGE_TABLE_RESOLUTION));

            const f32 depth = 
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
            set_position(f32vec3{_new_position.x, _new_position.y, _new_position.z});
            write_frustum_vertices({
                .vertices_dst = std::span<FrustumVertex, 8>{&vertices_space[(page_tex_u * 4 + page_tex_v) * 8], 8}
            });

            proj_info = ortho_info;
            set_position(f32vec3{new_position.x, new_position.y, new_position.z});
            recalculate_matrices();
        }
    }

    proj_info = ortho_info;
    set_position(f32vec3{new_position.x, new_position.y, new_position.z});
    return ClipAlignInfo{
        .per_page_depth_offset = f32vec2{page_x_depth_offset, page_y_depth_offset},
        .page_offset = i32vec2{
            -static_cast<i32>(ndc_page_scaled_aligned_player_position.x),
            -static_cast<i32>(ndc_page_scaled_aligned_player_position.y)
        }
    };
}