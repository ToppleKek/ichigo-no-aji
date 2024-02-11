#version 330 core
out vec4 frag_color;

uniform vec4 entity_color;

void main() {
    frag_color = entity_color;
}
