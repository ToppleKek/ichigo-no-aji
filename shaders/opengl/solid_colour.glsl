#version 330 core
out vec4 frag_color;
uniform vec4 colour;

void main() {
    frag_color = colour;
}
