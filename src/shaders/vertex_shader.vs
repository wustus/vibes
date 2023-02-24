#version 330 core

layout (location = 0) in vec3 v_pos;
layout (location = 1) in vec2 v_tex_coord;

out vec2 texture_coords;

void main() {
    gl_Position = vec4(v_pos.xyz, 1.0);
    texture_coords = vec2(v_tex_coord.x, -v_tex_coord.y);
}
