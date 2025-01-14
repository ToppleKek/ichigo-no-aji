#version 460 core
out vec4 frag_colour;

in vec2 tex_coord;

uniform vec3 colour;
uniform float alpha_adjust;
uniform sampler2D font_atlas;

void main() {
    frag_colour = vec4(colour, texture(font_atlas, tex_coord).r * alpha_adjust);
}
