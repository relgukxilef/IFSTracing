#version 430
layout(local_size_x = 4, local_size_y = 4, local_size_z = 1) in;

void main(void) {
    uvec2 position = gl_GlobalInvocationID.xy;
}
