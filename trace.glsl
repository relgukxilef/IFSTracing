#version 460

layout(local_size_x = 128, local_size_y = 2, local_size_z = 1) in;
layout(std430) buffer;

layout(rgba8) uniform writeonly image2D color;

layout(r8ui) uniform uimage3D recursions;
layout(r32f) uniform image3D depths;
layout(rgba32f) uniform image3D froms, directions, lights;

uniform vec2 pixel_size;
uniform vec2 pixel_offset;
uniform uint max_depth;

uniform float inverse_radius;

uniform mat4 view_matrix;

layout(row_major, binding = 1) readonly buffer MapsInverse {
    mat4x3 maps_inverse[];
};

uint size;

/*
Calculate projection of point onto vector
multiplied by the squared length of vector.
*/
vec3 project(vec3 point, vec3 vector) {
    return dot(point, vector) * vector;
}

struct intersection_result {
    vec3 position, normal;
    float depth;
    bool hit;
};

intersection_result intersect(
    vec3 from, vec3 direction, vec3 sphere, float radius
) {
    intersection_result result;
    vec3 offset = (from - sphere) / radius;
    direction = normalize(direction);

    vec3 closest = project(-offset, direction) + offset;
    float closest_squared = dot(closest, closest);
    result.hit = closest_squared < 1.0;

    if (result.hit) {
        // ray hits circle orthogonal to ray
        float distance = length(offset);
        float depth_offset = sqrt(1 - closest_squared);
        result.depth = (
            sqrt(
                distance * distance -
                closest_squared
            ) - depth_offset
        ) * radius;
        result.hit = result.depth > 0;

        if (result.hit) {
            // sphere is not around or behind from
            result.position = from + result.depth * direction;
            result.normal = (result.position - sphere) / radius;
        }
    }

    return result;
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

struct element {
    vec3 from, direction, light;
    uint recursion;
    float depth;
};

void set(uint i, element e) {
    imageStore(froms, ivec3(gl_GlobalInvocationID.xy, i), vec4(e.from, 0));
    imageStore(
        directions, ivec3(gl_GlobalInvocationID.xy, i), vec4(e.direction, 0)
    );
    imageStore(lights, ivec3(gl_GlobalInvocationID.xy, i), vec4(e.light, 0));
    imageStore(
        recursions, ivec3(gl_GlobalInvocationID.xy, i), uvec4(e.recursion)
    );
    imageStore(depths, ivec3(gl_GlobalInvocationID.xy, i), vec4(e.depth));
}

element get(uint i) {
    element e;
    e.from = imageLoad(froms, ivec3(gl_GlobalInvocationID.xy, i)).rgb;
    e.direction = imageLoad(directions, ivec3(gl_GlobalInvocationID.xy, i)).rgb;
    e.light = imageLoad(lights, ivec3(gl_GlobalInvocationID.xy, i)).rgb;
    e.recursion = imageLoad(recursions, ivec3(gl_GlobalInvocationID.xy, i)).r;
    e.depth = imageLoad(depths, ivec3(gl_GlobalInvocationID.xy, i)).r;
    return e;
}

void heap_swap(uint a, uint b) {
    element e = get(a);
    set(a, get(b));
    set(b, e);
}

void heap_insert(element e) {
    uint last = size;

    set(last, e);

    // heapify up
    uint node = last;
    uint parent = heap_parent(node);
    while (
        node > 0 &&
        imageLoad(depths, ivec3(gl_GlobalInvocationID.xy, parent)).r >
        imageLoad(depths, ivec3(gl_GlobalInvocationID.xy, node)).r
    ) {
        heap_swap(parent, node);
        node = parent;
        parent = heap_parent(node);
    }

    size++;
}

element heap_pop() {
    element e = get(0);

    size--;
    set(0, get(size));

    // heapify down
    uint root = 0;
    uint smallest = root;
    while (true) {
        uint left = heap_child(root);
        uint right = left + 1;

        if (
            left < size &&
            imageLoad(depths, ivec3(gl_GlobalInvocationID.xy, left)).r <
            imageLoad(depths, ivec3(gl_GlobalInvocationID.xy, smallest)).r
        ) {
            smallest = left;
        }
        if (
            right < size &&
            imageLoad(depths, ivec3(gl_GlobalInvocationID.xy, right)).r <
            imageLoad(depths, ivec3(gl_GlobalInvocationID.xy, smallest)).r
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

void main(void) {
    uvec2 position = gl_GlobalInvocationID.xy;
    vec3 from = vec3(0, 0, -4);
    vec3 direction = vec3((vec2(position) - pixel_offset) * pixel_size, 1);
    vec3 light_position = vec3(0, 0.5, -0.5);

    size = 0;

    element start;
    start.from = from;
    start.direction = direction;
    start.light = light_position;
    start.recursion = 0;
    start.depth = 0;

    heap_insert(start);

    float depth = 1e12;
    float shade = 1.0;

    while (size > 0) {
        element e = heap_pop();

        for (uint m = 0; m < maps_inverse.length(); m++) {
            mat4x3 map = maps_inverse[m];
            element child = e;
            child.recursion++;
            child.from = map * vec4(e.from, 1);
            child.direction = map * vec4(e.direction, 0);
            child.light = map * vec4(e.light, 1);

            intersection_result result = intersect(
                child.from, child.direction, vec3(0), 1
            );

            if (result.hit) {
                if (e.recursion < max_depth) {
                    heap_insert(child);
                } else {
                    if (result.depth < depth) {
                        depth = result.depth;
                        shade = phong_shading(
                            result.normal, result.position,
                            child.direction, child.light
                        );
                    }
                }
            }
        }
    }

    shade = pow(shade, 1.0 / 2.2);

    imageStore(color, ivec2(position), vec4(vec3(shade), 0));
}
