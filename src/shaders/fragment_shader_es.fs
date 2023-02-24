#version 300 es

precision mediump float;

in vec2 texture_coords;

out vec4 frag_color;

uniform sampler2D _texture;

void main() {
    frag_color = texture(_texture, texture_coords);
}
