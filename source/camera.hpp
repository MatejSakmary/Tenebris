#pragma once

#include <string>
#include <variant>

#include <daxa/types.hpp>

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
    daxa_f32 aspect_ratio;
    daxa_f32 fov;
    daxa_f32 near_plane;
};

struct OrthographicInfo
{
    daxa_f32 left;
    daxa_f32 right;
    daxa_f32 top;
    daxa_f32 bottom;
    daxa_f32 near;
    daxa_f32 far;
};

using ProjectionInfo = std::variant<PerspectiveInfo, OrthographicInfo>;

struct CameraInfo
{
    daxa_f32vec3 position;
    daxa_f32vec3 front;
    daxa_f32vec3 up;
    ProjectionInfo projection_info;
};

struct CameraFrustumInfo
{
    daxa_f32vec3 forward;
    daxa_f32vec3 top_frustum_offset;
    daxa_f32vec3 right_frustum_offset;
};

struct WriteVerticesInfo
{
    std::span<FrustumVertex, 8> vertices_dst;
};

struct ClipAlignInfo
{
    daxa_f32vec2 per_page_depth_offset;
    daxa_i32vec2 page_offset;
    daxa_i32 sun_height_offset;
};

struct Camera
{
    daxa_i32vec3 offset;
    ProjectionInfo proj_info;

    Camera() = default;
    explicit Camera(const CameraInfo & info);

    void move_camera(daxa_f32 delta_time, Direction direction, bool sped_up);
    void update_front_vector(daxa_f32 x_offset, daxa_f32 y_offset);
    void set_position(daxa_f32vec3 new_position);
    void set_front(daxa_f32vec3 new_front);
    [[nodiscard]] auto get_camera_position() const -> daxa_f32vec3;
    [[nodiscard]] auto get_view_matrix() -> daxa_f32mat4x4;
    [[nodiscard]] auto get_projection_matrix() -> daxa_f32mat4x4;
    [[nodiscard]] auto get_projection_view_matrix() -> daxa_f32mat4x4;
    [[nodiscard]] auto get_inv_projection_matrix() -> daxa_f32mat4x4;
    [[nodiscard]] auto get_inv_view_proj_matrix() -> daxa_f32mat4x4;

    [[nodiscard]] auto get_shadowmap_view_matrix(daxa_f32vec3 const & sun_direction, daxa_i32vec3 const & offset) -> daxa_f32mat4x4;
    [[nodiscard]] auto get_frustum_info() -> CameraFrustumInfo;
    auto align_clip_to_player(
        Camera const * player_camera,
        daxa_f32vec3 sun_offset,
        std::span<FrustumVertex, VSM_PAGE_TABLE_RESOLUTION * VSM_PAGE_TABLE_RESOLUTION * 8> vertices_space,
        bool view_page_frusti,
        daxa_i32 sun_offset_factor
    ) -> ClipAlignInfo;
    void write_frustum_vertices(WriteVerticesInfo const & info);

    private:
        void recalculate_matrices();

        bool matrix_dirty;
        glm::mat4x4 projection;
        glm::mat4x4 view;

        glm::vec3 position;
        glm::vec3 front;
        glm::vec3 up;
        daxa_f32 speed;
        daxa_f32 pitch;
        daxa_f32 sensitivity;
        daxa_f32 roll_sensitivity;
};