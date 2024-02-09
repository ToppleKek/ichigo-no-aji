#version 330 core
layout (location = 0) in vec3 pos;

uniform mat4 proj;

void main() {
    gl_Position = proj * vec4(pos.xyz, 1.0);
}
