#pragma once

#include <string>

#include <daxa/types.hpp>
#include <daxa/utils/math_operators.hpp>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/vector_angle.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "utils.hpp"

using namespace daxa::types;
using namespace daxa::math_operators;

enum Direction
{
    FORWARD,
    BACK,
    LEFT,
    RIGHT,
    UP,
    DOWN,
    ROLL_LEFT,
    ROLL_RIGHT,
    UNKNOWN [[maybe_unused]]
};

struct GetProjectionInfo
{
    f32 near_plane;
    f32 far_plane;
};

struct CameraInfo
{
    f32vec3 position;
    f32vec3 front;
    f32vec3 up;
    f32 aspect_ratio;
    f32 fov;
};

struct CameraFrustumInfo
{
    f32vec3 forward;
    f32vec3 top_frustum_offset;
    f32vec3 right_frustum_offset;
};

struct Camera
{
    f32 aspect_ratio;
    f32 fov;

    explicit Camera(const CameraInfo & info);

    void move_camera(f32 delta_time, Direction direction);
    void update_front_vector(f32 x_offset, f32 y_offset);
    void set_position(f32vec3 new_position);
    [[nodiscard]] auto get_camera_position() const -> f32vec3;
    [[nodiscard]] auto get_view_matrix() const -> f32mat4x4;
    [[nodiscard]] auto get_projection_matrix(const GetProjectionInfo & info) const -> f32mat4x4;
    [[nodiscard]] auto get_inv_view_proj_matrix(const GetProjectionInfo & info) const -> f32mat4x4;
    [[nodiscard]] auto get_frustum_info() const -> CameraFrustumInfo;

    private:
        glm::vec3 position;
        glm::vec3 front;
        glm::vec3 up;
        f32 pitch;
        f32 speed;
        f32 sensitivity;
        f32 roll_sensitivity;
};