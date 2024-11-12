#version 330 core
out vec4 frag_colour;

in vec2 tex_coord;

uniform vec4 colour_tint;
uniform sampler2D entity_texture;

void main() {
    frag_colour = texture(entity_texture, tex_coord) * colour_tint;
}
