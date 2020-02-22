#version 410 core

in vec2 position;

out vec2 vertex_position;

void main(void)
{
    gl_Position = vec4(position, 1, 1);
    vertex_position = position;
}
