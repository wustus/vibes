#version 300 es

in vec2 v_pos;
in vec3 v_color;

uniform float u_aspect_ratio;

out vec3 rgb_color;

void main() {
    gl_Position = vec4(v_pos.x * u_aspect_ratio, v_pos.y, 0.0, 1.0);
    rgb_color = v_color;
}
