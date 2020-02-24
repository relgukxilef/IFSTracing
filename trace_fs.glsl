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

struct intersection_parameters {
    vec3 center, direction;
    float direction_squared; // Dot product of direction with itself.
    float radius, radius_squared;
};

/*
Stores the result of an intersection test.
depth_offset_squared is positive if there is an intersection,
otherwise it's negative.
*/
struct test_result {
    /*
    Vector from eye to closest point on the ray to the center of the sphere,
    multiplied by direction_squared.
    */
    vec3 closest;
    /*
    Vector from the center of the sphere to the closest point on the ray,
    multiplied by direction_squared.
    */
    vec3 offset;
    float offset_squared; // Dot product of offset with itself.
    /*
    Squared distance between the depth of the center and the depth
    of the intersection, multiplied by direction_squared squared.
    */
    float depth_offset_squared;
};

/*
Calculates the squared difference between the depth of the center and the depth
of the intersection, a positive value if the ray intersects the sphere,
a negative value otherwise.
*/
test_result test(
    intersection_parameters p
) {
    test_result r;
    // project center onto direction vector
    // devisions are postponed
    r.closest = project(p.center, p.direction);
    r.offset = r.closest - p.center * p.direction_squared;
    // distance between closest point and center
    r.offset_squared = dot(r.offset, r.offset);
    r.depth_offset_squared =
        p.radius_squared * p.direction_squared * p.direction_squared -
        r.offset_squared;
    return r;
}

/*

*/
struct depth_result {
    float closest_squared; // Dot product between closest and itself.
    /*
    Squared depth multipled by direction_squared
    */
    float depth_squared;
};

/*
Returns the squared depth of the intersection for depth test.
Assumes test has already been used and there is an intersection.
*/
depth_result depth(
    test_result t
) {
    depth_result d;
    d.closest_squared = dot(t.closest, t.closest);

    float clamped_depth_offset_squared = max(t.depth_offset_squared, 0);

    // calculating squared depth only requires one sqrt, instead of two
    d.depth_squared =
        -2 * sqrt(d.closest_squared * clamped_depth_offset_squared) +
        d.closest_squared + clamped_depth_offset_squared;

    return d;
}

struct intersection_result {
    vec3 position;
    vec3 normal;
};

intersection_result intersection (
    intersection_parameters p, depth_result d
) {
    intersection_result i;
    i.position =
        p.direction * sqrt(d.depth_squared) /
        (p.direction_squared * sqrt(p.direction_squared));
    i.normal = (i.position - p.center) / p.radius;
    return i;
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

    intersection_parameters p;
    p.center = vec3(0, 0, 3);
    p.radius = 1;
    p.direction = vec3(vertex_position * view_plane_size, 1);

    p.radius_squared = p.radius * p.radius;
    p.direction_squared = dot(p.direction, p.direction);

    color = vec3(0);

    test_result t = test(p);
    if (t.depth_offset_squared >= 0) {
        depth_result d = depth(t);
        intersection_result i = intersection(p, d);
        color = vec3(phong_shading(i.normal, i.position, p.direction));
    }
}
