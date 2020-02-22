#version 410 core

in vec2 vertex_position;

out vec3 color;

uniform vec2 view_plane_size;

/*
calculates the squared distance from the center of the sphere
to the closest point on the ray
multiplied by the length of the direction vector to the power of 4
*/
float sphere_offset_squared(
    vec3 center, vec3 direction, float direction_squared
) {
    // project center onto direction, divisions are postponed
    vec3 closest = dot(center, direction) * direction;
    vec3 offset = closest - center * direction_squared;
    return dot(offset, offset);
}

// like the step function but smooth using fwidth as width
float soft_step(float x) {
    float width = fwidth(x);
    return clamp(x / width + 0.5, 0, 1);
}

void main(void)
{
    // determine bounding spheres of recursions
    // trace bounding spheres
    // repeat

    vec3 center = vec3(3, 0, 2);
    float radius_squared = 1;

    vec3 direction = vec3(vertex_position * view_plane_size, 1);
    float direction_squared = dot(direction, direction);

    float offset_squared = sphere_offset_squared(
        center, direction, direction_squared
    );

    float intersection =
        radius_squared * direction_squared * direction_squared - offset_squared;

    color = vec3(soft_step(intersection));
    color = pow(color, vec3(1 / 2.2));
}
