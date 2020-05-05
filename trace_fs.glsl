#version 460

in vec2 vertex_position;

out vec3 fragment_color;

uniform vec2 view_plane_size;
uniform uint scanline_stride;
uniform uint image_stride;
uniform uint max_depth;

uniform float inverse_radius;

uint index;
uint size;

layout(std430) buffer;

layout(row_major, binding = 1) readonly buffer MapsInverse {
    mat4x3 maps_inverse[];
};

// TODO: try using textures for these to allow smaller types and better locality
layout(binding = 2) buffer RecursionDepths {
    uint recursion_depths[];
};

layout(binding = 3) buffer PixelDepths {
    float depths[];
};

struct ray {
    vec3 origin, direction, light;
};

layout(binding = 4) buffer Rays {
    ray rays[];
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
        p.direction_squared * p.direction_squared -
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
    // TODO: depths are at wrong scale
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
    i.normal = i.position + p.origin;
    return i;
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

uint heap_child(uint parent) {
    return parent * 2 + 1;
}

uint heap_parent(uint child) {
    return (child - 1) / 2;
}

void heap_swap(uint a, uint b) {
    a = a * image_stride + index;
    b = b * image_stride + index;

    ray r = rays[a];
    rays[a] = rays[b];
    rays[b] = r;

    float d = depths[a];
    depths[a] = depths[b];
    depths[b] = d;

    uint rd = recursion_depths[a];
    recursion_depths[a] = recursion_depths[b];
    recursion_depths[b] = rd;
}

struct element {
    ray r;
    uint recursion_depth;
    float depth;
};

void heap_insert(element e) {
    uint last = size * image_stride + index;
    rays[last] = e.r;
    recursion_depths[last] = e.recursion_depth;
    depths[last] = e.depth;

    // heapify up
    uint node = size;
    uint parent = heap_parent(node);
    while (
        node > 0 &&
        depths[parent * image_stride + index] >
        depths[node * image_stride + index]
    ) {
        heap_swap(parent, node);
        node = parent;
        parent = heap_parent(node);
    }

    size++;
}

element heap_pop() {
    element e;
    e.r = rays[index];
    e.recursion_depth = recursion_depths[index];
    e.depth = depths[index];

    size--;
    rays[0] = rays[size];
    depths[0] = depths[size];
    recursion_depths[0] = recursion_depths[size];

    // heapify down
    uint root = 0;
    uint smallest = root;
    while (true) {
        uint left = heap_child(root);
        uint right = left + 1;

        if (
            left < size &&
            depths[left * image_stride + index] <
            depths[smallest * image_stride + index]
        ) {
            smallest = left;
        }
        if (
            right < size &&
            depths[right * image_stride + index] <
            depths[smallest * image_stride + index]
        ) {
            smallest = right;
        }

        if (smallest != root) {
            heap_swap(root, smallest);
            root = smallest;
        } else {
            break;
        }
    }

    return e;
}

void main(void)
{
    ivec2 screen_position = ivec2(gl_FragCoord.xy);
    index = screen_position.y * scanline_stride + screen_position.x;
    size = 0;

    /*
    while there are spheres left to test
        pick the closest
        if we're at the depth limit
            return the closest intersecting child
        else
            queue all intersecting children (up to number of transformations)
    */

    intersection_parameters p;
    p.origin = -vec3(0, 0, 1) * inverse_radius;
    p.direction = vec3(vertex_position * view_plane_size, 1);

    p.direction_squared = dot(p.direction, p.direction);

    float depth_squared = 1e12;

    vec3 light_position = vec3(-1, 2, 0); // relative to origin
    vec3 normal, position, direction;

    element e;
    ray r;
    r.origin = vec3(0, 0, -1);
    r.direction = vec3(vertex_position * view_plane_size, 1);
    r.light = light_position;
    e.r = r;
    e.recursion_depth = 0;
    e.depth = 3; // TODO
    heap_insert(e);

    uint counter = 0;

    while (size > 0 && counter < 100) {
        counter++;
        e = heap_pop();
        // trace children
        for (uint m = 0; m < maps_inverse.length(); m++) {
            mat4x3 map = maps_inverse[m];
            element child = e;
            child.recursion_depth++;
            child.r.origin = map * vec4(e.r.origin, 1);
            child.r.direction = map * vec4(e.r.direction, 0);
            child.r.light = map * vec4(e.r.light, 1);

            intersection_parameters p;
            p.origin = child.r.origin * inverse_radius;
            p.direction = child.r.direction;
            p.direction_squared = dot(p.direction, p.direction);

            test_result t = test(p);
            if (t.depth_offset_squared >= 0) {
                depth_result d = depth(t);
                child.depth = d.depth_squared;
                if (child.recursion_depth < max_depth) {
                    heap_insert(child);
                } else {
                    if (d.depth_squared < depth_squared) {
                        depth_squared = d.depth_squared;
                        intersection_result i = intersection(p, d);
                        fragment_color = vec3(
                            phong_shading(
                                i.normal, i.position, p.direction, child.r.light
                            )
                        );
                    }
                }
            }
        }
    }
}
