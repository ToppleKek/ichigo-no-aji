#version 460 core
layout (location = 0) in vec3 pos;
layout (location = 1) in vec2 tex;

out vec2 tex_coord;
uniform mat4 object_transform;

void main() {
    tex_coord = tex;
    vec4 transformed_pos = object_transform * vec4(pos.xyz, 1.0);
    gl_Position = vec4(transformed_pos.x * 2 - 1, transformed_pos.y * -2 + 1, 0.0, 1.0);
}
