#version 330 core

layout (location = 0) in vec2 v_pos;
layout (location = 1) in vec4 v_color;

uniform float u_aspect_ratio;
uniform bool u_use_ratio;

out vec4 rgb_color;

void main() {
    if (u_use_ratio) {
        gl_Position = vec4(v_pos.x * u_aspect_ratio, v_pos.y, 0.0, 1.0);
    } else {
        gl_Position = vec4(v_pos.xy, 0.0, 1.0);
    }
    
    rgb_color = v_color;
}
