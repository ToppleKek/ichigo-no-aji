#version 330 core
out vec4 frag_color;

in vec2 tex_coord;

uniform vec4 colour;

void main() {
    frag_color = colour;
}
