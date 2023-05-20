#version 300 es

precision mediump float;

in vec4 rgb_color;

out vec4 frag_color;

void main() {
    frag_color = rgb_color;
}
