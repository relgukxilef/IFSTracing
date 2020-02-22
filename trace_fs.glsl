#version 410 core

in vec2 vertex_position;

uniform vec2 view_plane_size;

float ray_sphere_intersect(vec3 center, vec3 direction, float radius_squared) {
    float length_squared = dot(direction, direction);
    // project offset onto direction
    vec3 closest = dot(center, direction) * direction;
    vec3 offset = closest - center * length_squared;
    return radius_squared * length_squared - dot(offset, offset);
}

void main(void)
{
    // determine bounding spheres of recursions
    // trace bounding spheres
    // repeat

    float intersection = ray_sphere_intersect(
        vec3(0, 0, 2), vec3(vertex_position * view_plane_size, 2), 1
    );

    gl_FragColor = vec4(clamp(sign(intersection), 0, 1), 1, 1, 1);
}
