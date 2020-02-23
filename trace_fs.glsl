#version 410 core

in vec2 vertex_position;

out vec3 color;

uniform vec2 view_plane_size;

/*
Calculate projection of point onto vector
multiplied by the squared length of vector.
*/
vec3 project(vec3 point, vec3 vector) {
    return dot(point, vector) * vector;
}

/*
Returns a positive value if the ray intersects the sphere,
a negative value otherwise.
*/
float intersect(
    vec3 center, vec3 direction, float radius_squared, float direction_squared
) {
    // project center onto direction vector
    // devisions are postponed
    vec3 closest = project(center, direction);
    vec3 offset = closest - center * direction_squared;
    // distance between closest point and center
    float offset_squared = dot(offset, offset);
    return
        radius_squared * direction_squared * direction_squared - offset_squared;
}

/*
Returns the point of intersection between a ray and a sphere.
*/
vec3 intersection(
    vec3 center, vec3 direction, float direction_squared, float radius_squared
) {
    float center_squared = dot(center, center);
    vec3 closest = project(center, direction);
    float closest_squared = dot(closest, closest);
    vec3 offset = closest - center * direction_squared;
    float offset_squared = dot(offset, offset);

    float depth_offset_squared =
        radius_squared * direction_squared * direction_squared - offset_squared;

    float depth = sqrt(closest_squared) - sqrt(max(depth_offset_squared, 0));

    return depth * direction / sqrt(direction_squared) / direction_squared;
}

// like the step function but smooth using fwidth as width
float soft_step(float x) {
    float width = fwidth(x);
    return clamp(x / width + 0.5, 0, 1);
}

float phong_shading(vec3 normal, vec3 position, vec3 direction) {
    vec3 light_position = vec3(-2, 2, 0);
    vec3 light_direction = normalize(light_position - position);
    vec3 reflection_direction = reflect(light_direction, normal);
    float diffuse = max(dot(light_direction, normal), 0);
    float specular =
        pow(max(dot(normalize(direction), reflection_direction), 0), 100);
    float ambient = 0.05;
    return diffuse * 0.5 + specular * 0.5 + ambient;
}

void main(void)
{
    // determine bounding spheres of recursions
    // trace bounding spheres
    // repeat

    vec3 center = vec3(-1, 0, 3);
    float radius = 1;
    float radius_squared = radius * radius;

    vec3 direction = vec3(vertex_position * view_plane_size, 1);
    float direction_squared = dot(direction, direction);

    vec3 closest = project(center, direction);
    float closest_squared = dot(closest, closest);
    vec3 offset = closest - center * direction_squared;
    float offset_squared = dot(offset, offset);

    float depth_offset_squared =
        radius_squared * direction_squared * direction_squared - offset_squared;

    vec3 position = intersection(
        center, direction, direction_squared, radius_squared
    );
    vec3 normal = (position - center) / radius;

    color = vec3(soft_step(depth_offset_squared));
    color *= phong_shading(normal, position, direction);

    color = pow(color, vec3(1 / 2.2));
}
