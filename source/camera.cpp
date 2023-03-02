#include "camera.hpp"

#include <fstream>

auto constexpr static inline daxa_vec3_to_glm(f32vec3 vec) -> glm::vec3 { return glm::vec3(vec.x, vec.y, vec.z); }

Camera::Camera(const CameraInfo & info) : 
    position{daxa_vec3_to_glm(info.position)},
    front{daxa_vec3_to_glm(info.front)},
    up{daxa_vec3_to_glm(info.up)},
    aspect_ratio{info.aspect_ratio},
    fov{info.fov},
    speed{100.0f},
    pitch{0.0f},
    yaw{-90.0f},
    sensitivity{0.08f}, 
    roll_sensitivity{20.0f},
    roll{0.0f}
{
}

void Camera::set_info(const CameraInfo & info)
{
    position = daxa_vec3_to_glm(info.position);
    front = daxa_vec3_to_glm(info.front);
    up = daxa_vec3_to_glm(info.up);
    fov = info.fov;
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
        DEBUG_OUT("[Camera::move_camera()] Unkown enum value");
        break;
    }
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
}

auto Camera::get_view_matrix() const -> f32mat4x4
{
    auto view_mat = glm::lookAt(position, position + front, up);

    return mat_from_span<f32, 4, 4>(std::span<f32, 4 * 4>{ glm::value_ptr(view_mat), 4 * 4 });
}

auto Camera::get_projection_matrix(const GetProjectionInfo & info) const -> f32mat4x4
{
    auto proj_mat = glm::perspective(fov, aspect_ratio, info.near_plane, info.far_plane);
    /* GLM is using OpenGL standard where Y coordinate of the clip coordinates is inverted */
    proj_mat[1][1] *= -1;

    return mat_from_span<f32, 4, 4>(std::span<f32, 4 * 4>{ glm::value_ptr(proj_mat), 4 * 4 });
}

// cope because I use daxa types - TODO(msakmary) switch to glm types for all internals
auto Camera::get_inv_view_proj_matrix(const GetProjectionInfo & info) const -> f32mat4x4
{
    auto view_mat = glm::lookAt(position, position + front, up);
    auto proj_mat = glm::perspective(fov, aspect_ratio, info.near_plane, info.far_plane);
    /* GLM is using OpenGL standard where Y coordinate of the clip coordinates is inverted */
    proj_mat[1][1] *= -1;

    auto inv_proj_view_mat = glm::inverse(proj_mat * view_mat);

    auto res = inv_proj_view_mat * (proj_mat * view_mat);
    return mat_from_span<f32, 4, 4>(std::span<f32, 4 * 4>{ glm::value_ptr(inv_proj_view_mat), 4 * 4 });
}

auto Camera::get_camera_position() const -> f32vec3
{
    return f32vec3{position.x, position.y, position.z};
}