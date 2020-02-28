#version 430 core

in vec2 vertex_position;

out vec3 fragment_color;

uniform vec2 view_plane_size;

layout(std430, row_major, binding = 1) readonly buffer MapsInverse {
    mat4x3 maps_inverse[];
};

/*
Calculate projection of point onto vector
multiplied by the squared length of vector.
*/
vec3 project(vec3 point, vec3 vector) {
    return dot(point, vector) * vector;
}

struct intersection_parameters {
    vec3 origin, direction;
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
    Vector from the origin to point on the ray closest to the sphere center
    multiplied by direction_squared.
    */
    vec3 closest;
    /*
    Vector from the center of the sphere to the closest point on the ray
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
    // project origin onto direction vector
    // devisions are postponed
    r.closest = project(-p.origin, p.direction);
    r.offset = r.closest + p.origin * p.direction_squared;
    // distance between origin point and center
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
    /*
    Vector from origin to intersection.
    */
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
    i.normal = (i.position + p.origin) / p.radius;
    return i;
}

// like the step function but smooth using fwidth as width
float soft_step(float x) {
    float width = fwidth(x);
    return clamp(x / width + 0.5, 0, 1);
}

float phong_shading(
    vec3 normal, vec3 position, vec3 direction, vec3 light_position
) {
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
    p.origin = -vec3(1, 0, 4);
    p.radius = 0.5;
    p.direction = vec3(vertex_position * view_plane_size, 1);

    p.radius_squared = p.radius * p.radius;
    p.direction_squared = dot(p.direction, p.direction);

    float depth_squared = 10000;

    vec3 light_position = vec3(1, 2, 3); // relative to origin
    vec3 normal, position, direction;

    for (uint m = 0; m < maps_inverse.length(); m++) {
        mat4x3 map = maps_inverse[m];

        intersection_parameters p2 = p;
        p2.origin = map * vec4(p.origin, 1);
        p2.direction = map * vec4(p.direction, 0);
        p2.direction_squared = dot(p2.direction, p2.direction);
        vec3 light_position2 = map * vec4(light_position, 1);

        test_result t = test(p2);
        if (t.depth_offset_squared >= 0) {
            depth_result d = depth(t);
            if (d.depth_squared < depth_squared) {
                depth_squared = d.depth_squared;
                intersection_result i = intersection(p2, d);
                normal = i.normal;
                position = i.position;
                direction = p2.direction;
                fragment_color = vec3(
                    phong_shading(normal, position, direction, light_position2)
                );
            }
        }
    }
}
