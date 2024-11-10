#version 330 core
layout (location = 0) in vec3 pos;
layout (location = 1) in vec2 tex;

out vec2 tex_coord;

uniform mat4 camera_transform;
uniform mat4 object_transform;

#define SCREEN_TILE_WIDTH 16.0
#define SCREEN_TILE_HEIGHT 9.0

void main() {
    tex_coord = tex;
    vec4 transformed_pos = camera_transform * object_transform * vec4(pos.xyz, 1.0);
    gl_Position = vec4(transformed_pos.x * (2.0/SCREEN_TILE_WIDTH) - 1.0, transformed_pos.y * (-2.0/SCREEN_TILE_HEIGHT) + 1.0, transformed_pos.z, 1.0);
}
