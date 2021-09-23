#version 400
in vec4 v_color;

layout (location = 0) out vec4 o_pixel;
void main() {
	o_pixel = v_color;
}