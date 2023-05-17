#version 300 es

in vec2 v_pos;
in vec2 v_tex_coord;

out vec2 texture_coords;

void main() {
    gl_Position = vec4(v_pos.xy, 0.0, 1.0);
    texture_coords = vec2(v_tex_coord.x, -v_tex_coord.y);
}
