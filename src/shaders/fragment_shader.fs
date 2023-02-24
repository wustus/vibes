#version 330 core

out vec4 frag_color;

in vec2 texture_coords;

uniform sampler2D _texture;

void main() {
   frag_color = texture(_texture, texture_coords);
}
