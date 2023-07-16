#include "camera.hpp"

#include <fstream>

auto constexpr static inline daxa_vec3_to_glm(f32vec3 vec) -> glm::vec3 { return glm::vec3(vec.x, vec.y, vec.z); }

Camera::Camera(const CameraInfo & info) : 
    matrix_dirty{true},
    position{daxa_vec3_to_glm(info.position)},
    front{daxa_vec3_to_glm(info.front)},
    up{daxa_vec3_to_glm(info.up)},
    aspect_ratio{info.aspect_ratio},
    fov{info.fov},
    near_plane{info.near_plane},
    speed{100.0f},
    pitch{0.0f},
    sensitivity{0.08f},
    roll_sensitivity{20.0f}
{
}

void Camera::set_position(f32vec3 new_position)
{
    position = daxa_vec3_to_glm(new_position);
    matrix_dirty = true;
}

void Camera::move_camera(f32 delta_time, Direction direction)
{
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
    matrix_dirty = true;
}

void Camera::update_front_vector(f32 x_offset, f32 y_offset)
{
    glm::vec3 front_ = glm::rotate(front, glm::radians(-sensitivity * x_offset), up);
    front_ = glm::rotate(front_, glm::radians(-sensitivity * y_offset), glm::cross(front,up));

    pitch = glm::degrees(glm::angle(front_, up));

    const f32 MAX_PITCH_ANGLE = 179.0f;
    const f32 MIN_PITCH_ANGLE = 1.0f;
    if (pitch < MIN_PITCH_ANGLE || pitch > MAX_PITCH_ANGLE )
    {
        return;
    }

    front = front_;
    matrix_dirty = true;
}

void Camera::recalculate_matrices()
{
    // Infinite far plane
    f32 const tan_half_fovy = 1.0f / glm::tan(fov * 0.5f);

    projection = glm::mat4x4(0.0f);
    projection[0][0] =  tan_half_fovy / aspect_ratio;
    /* GLM is using OpenGL standard where Y coordinate of the clip coordinates is inverted */
    projection[1][1] = -tan_half_fovy;
    projection[2][2] =  0.0f;
    projection[2][3] = -1.0f;
    projection[3][2] =  near_plane;

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

auto Camera::get_inv_view_proj_matrix() -> f32mat4x4
{
    if(matrix_dirty) { recalculate_matrices(); }
    auto inv_proj_view_mat = glm::inverse(projection * view);

    return mat_from_span<f32, 4, 4>(std::span<f32, 4 * 4>{ glm::value_ptr(inv_proj_view_mat), 4 * 4 });
}

auto Camera::get_frustum_info() -> CameraFrustumInfo
{
    glm::vec3 right;
    if(front.x != 0 && front.y != 0)      { right = glm::vec3(front.y, -front.x, 0.0f); }
    else if(front.x != 0 && front.z != 0) { right = glm::vec3(-front.z, 0.0f, front.x); }
    else                                  { right = glm::vec3(0.0f, -front.z, front.y); }

    glm::vec3 up_ = glm::normalize(glm::cross(right, front));
    glm::vec3 right_ = glm::normalize(glm::cross(front, up));

    f32 fov_tan = glm::tan(fov / 2.0f);

    auto right_aspect_fov_correct = right_ * aspect_ratio * fov_tan;
    auto up_fov_correct = glm::normalize(up_) * fov_tan;

    auto glm_vec_to_daxa = [](glm::vec3 v) -> f32vec3 { return {v.x, v.y, v.z}; };

    return {
        .forward = glm_vec_to_daxa(front),
        .top_frustum_offset = glm_vec_to_daxa(-up_fov_correct),
        .right_frustum_offset = glm_vec_to_daxa(right_aspect_fov_correct)
    };
}

auto Camera::get_shadowmap_view_matrix(f32vec3 const sun_direction) -> f32mat4x4
{
    const f32 shadowmap_distance = 7000.0f;
    glm::vec3 glm_sun_dir = glm::normalize(glm::vec3(sun_direction.x, sun_direction.y, sun_direction.z));
    // glm::vec3 sun_shadowmap_position = position + glm_sun_dir * shadowmap_distance;
    glm::vec3 sun_shadowmap_position = glm::vec3(5000, 5000, 0.0) + glm_sun_dir * shadowmap_distance;
    glm::vec3 front = glm::normalize(glm::vec3(5000, 5000, 0.0) - sun_shadowmap_position);
    auto view_mat = glm::lookAt( sun_shadowmap_position, sun_shadowmap_position + front, up);

    return mat_from_span<f32, 4, 4>(std::span<f32, 4 * 4>{ glm::value_ptr(view_mat), 4 * 4});
}

auto Camera::get_shadowmap_projection_matrix(const GetShadowmapProjectionInfo & info) -> f32mat4x4
{
    auto proj_mat = glm::ortho(info.left, info.right , info.bottom, info.top, info.near_plane, info.far_plane);
    /* GLM is using OpenGL standard where Y coordinate of the clip coordinates is inverted */
    proj_mat[1][1] *= -1;

    return mat_from_span<f32, 4, 4>(std::span<f32, 4 * 4>{ glm::value_ptr(proj_mat), 4 * 4 });
}

auto Camera::get_camera_position() const -> f32vec3
{
    return f32vec3{position.x, position.y, position.z};
}