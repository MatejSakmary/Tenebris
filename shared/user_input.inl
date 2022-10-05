#pragma once

#include <daxa/daxa.inl>

// clang-format off
#define GAME_MOUSE_BUTTON_1         0
#define GAME_MOUSE_BUTTON_2         1
#define GAME_MOUSE_BUTTON_3         2
#define GAME_MOUSE_BUTTON_4         3
#define GAME_MOUSE_BUTTON_5         4
#define GAME_MOUSE_BUTTON_LAST      GAME_MOUSE_BUTTON_5
#define GAME_MOUSE_BUTTON_LEFT      GAME_MOUSE_BUTTON_1
#define GAME_MOUSE_BUTTON_RIGHT     GAME_MOUSE_BUTTON_2
#define GAME_MOUSE_BUTTON_MIDDLE    GAME_MOUSE_BUTTON_3

#define GAME_KEY_MOVE_FORWARD       0
#define GAME_KEY_MOVE_LEFT          1
#define GAME_KEY_MOVE_BACKWARD      2
#define GAME_KEY_MOVE_RIGHT         3
#define GAME_KEY_RELOAD             4
#define GAME_KEY_TOGGLE_FLY         5
#define GAME_KEY_INTERACT1          6
#define GAME_KEY_INTERACT0          7
#define GAME_KEY_JUMP               (GAME_KEY_INTERACT0 + 1)
#define GAME_KEY_CROUCH             (GAME_KEY_INTERACT0 + 2)
#define GAME_KEY_SPRINT             (GAME_KEY_INTERACT0 + 3)
#define GAME_KEY_WALK               (GAME_KEY_INTERACT0 + 4)
#define GAME_KEY_CYCLE_VIEW         (GAME_KEY_INTERACT0 + 5)
#define GAME_KEY_LAST               GAME_KEY_CYCLE_VIEW
// clang-format on

#define GPU_INPUT_FLAG_INDEX_PAUSED 0

struct Settings {
    u32 flags;
    f32 fov;
    f32 jitter_scl;
    f32 sensitivity;
};

struct MouseInput {
    f32vec2 pos;
    f32vec2 pos_delta;
    f32vec2 scroll_delta;
    u32 buttons[GAME_MOUSE_BUTTON_LAST + 1];
};

struct KeyboardInput {
    u32 keys[GAME_KEY_LAST + 1];
};
