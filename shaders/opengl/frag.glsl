#version 330 core
out vec4 frag_color;

in vec2 tex_coord;

// uniform vec4 entity_color;
uniform sampler2D entity_texture;

void main() {
    frag_color = texture(entity_texture, tex_coord);
}
