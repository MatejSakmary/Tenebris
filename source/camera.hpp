#pragma once

#include <string>
#include <variant>

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
#include "renderer/shared/shared.inl"

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
struct PerspectiveInfo
{
    f32 aspect_ratio;
    f32 fov;
    f32 near_plane;
};

struct OrthographicInfo
{
    f32 left;
    f32 right;
    f32 top;
    f32 bottom;
    f32 near;
    f32 far;
};

using ProjectionInfo = std::variant<PerspectiveInfo, OrthographicInfo>;

struct CameraInfo
{
    f32vec3 position;
    f32vec3 front;
    f32vec3 up;
    ProjectionInfo projection_info;
};

struct CameraFrustumInfo
{
    f32vec3 forward;
    f32vec3 top_frustum_offset;
    f32vec3 right_frustum_offset;
};

struct WriteVerticesInfo
{
    std::span<FrustumVertex, 8> vertices_dst;
};

struct Camera
{
    i32vec3 offset;
    ProjectionInfo proj_info;

    Camera() = default;
    explicit Camera(const CameraInfo & info);

    void move_camera(f32 delta_time, Direction direction, bool sped_up);
    void update_front_vector(f32 x_offset, f32 y_offset);
    void set_position(f32vec3 new_position);
    void set_front(f32vec3 new_front);
    [[nodiscard]] auto get_camera_position() const -> f32vec3;
    [[nodiscard]] auto get_view_matrix() -> f32mat4x4;
    [[nodiscard]] auto get_projection_matrix() -> f32mat4x4;
    [[nodiscard]] auto get_projection_view_matrix() -> f32mat4x4;
    [[nodiscard]] auto get_inv_projection_matrix() -> f32mat4x4;
    [[nodiscard]] auto get_inv_view_proj_matrix() -> f32mat4x4;

    [[nodiscard]] auto get_shadowmap_view_matrix(f32vec3 const & sun_direction, i32vec3 const & offset) -> f32mat4x4;
    [[nodiscard]] auto get_frustum_info() -> CameraFrustumInfo;
    void align_clip_to_player(Camera const * player_camera, f32vec3 sun_offset);
    void write_frustum_vertices(WriteVerticesInfo const & info);

    private:
        void recalculate_matrices();

        bool matrix_dirty;
        glm::mat4x4 projection;
        glm::mat4x4 view;

        glm::vec3 position;
        glm::vec3 front;
        glm::vec3 up;
        f32 speed;
        f32 pitch;
        f32 sensitivity;
        f32 roll_sensitivity;
};