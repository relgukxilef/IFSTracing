#version 460

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;
layout(std430) buffer;

layout(rgba8) uniform writeonly image2D color;

const uint queue_size = 16;

uint recursions[queue_size];
float depths[queue_size];
mat3x4 matrices[queue_size];

uniform vec2 pixel_size;
uniform vec2 pixel_offset;
uniform uint max_depth;

uniform float inverse_radius;

uniform mat4 model_matrix;

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
    vec3 from, vec3 direction, vec3 sphere
) {
    intersection_result result;
    vec3 offset = from - sphere;

    if (dot(offset, offset) < 1.0) {
        // from is inside sphere
        result.hit = true;
        result.depth = 0;
        result.position = from;
        return result;
    }
    if (dot(direction, offset) > 0.0) {
        // ray points away from sphere
        result.hit = false;
        return result;
    }

    // TODO: avoid normalization
    direction = normalize(direction);

    vec3 closest = project(-offset, direction) + offset;
    float closest_squared = dot(closest, closest);
    result.hit = closest_squared < 1.0;

    if (result.hit) {
        // ray hits circle orthogonal to ray
        float distance = length(offset);
        float depth_offset = sqrt(1 - closest_squared);
        float circle_depth = sqrt(distance * distance - closest_squared);

        result.hit = circle_depth - depth_offset > 0;
        result.depth = circle_depth - depth_offset;

        if (result.hit) {
            // TODO: test performance impact of this if
            result.position = from + result.depth * direction;
            result.normal = result.position - sphere;
        }
    }

    return result;
}

uint heap_child(uint parent) {
    return parent * 2 + 1;
}

uint heap_parent(uint child) {
    return (child - 1) / 2;
}

struct element {
    uint recursion;
    float depth;
    mat3x4 matrix;
};

void set(uint i, element e) {
    matrices[i] = e.matrix;
    recursions[i] = e.recursion;
    depths[i] = e.depth;
}

element get(uint i) {
    element e;
    e.matrix = matrices[i];
    e.recursion = recursions[i];
    e.depth = depths[i];
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
        depths[parent] > depths[node]
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
            depths[left] < depths[smallest]
        ) {
            smallest = left;
        }
        if (
            right < size &&
            depths[right] < depths[smallest]
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

struct trace_result {
    element e;
    intersection_result i;
};

trace_result trace(vec3 from, vec3 direction) {
    trace_result result;
    result.i.depth = 1e12;

    size = 0;

    element start;
    start.matrix = mat3x4(1);
    start.recursion = 0;
    start.depth = 0;

    heap_insert(start);

    while (size > 0) {
        element e = heap_pop();

        for (uint m = 0; m < maps_inverse.length(); m++) {
            mat4x3 map = maps_inverse[m];
            element child = e;
            child.recursion++;
            child.matrix = mat3x4(
                transpose(mat4(map) * mat4(transpose(e.matrix)))
            );

            intersection_result i = intersect(
                vec4(from, 1.0) * child.matrix,
                vec4(direction, 0.0) * child.matrix, vec3(0)
            );

            if (i.hit) {
                if (e.recursion < max_depth) {
                    heap_insert(child);
                } else {
                    if (i.depth < result.i.depth) {
                        result.e = child;
                        result.i = i;
                    }
                }
            }
        }
    }

    return result;
}

mat4x3 projective_inverse(mat4x3 m) {
    mat3 a = mat3(m);
    vec3 t = m[3];
    a = inverse(a);
    return mat4x3(a[0], a[1], a[2], -a * t);
}
mat3x4 projective_inverse(mat3x4 m) {
    return transpose(projective_inverse(transpose(m)));
}

void main(void) {
    uvec2 position = gl_GlobalInvocationID.xy;
    vec3 from = (model_matrix * vec4(0, 0, 0, 1)).xyz;
    vec3 direction = (
        model_matrix * vec4((vec2(position) - pixel_offset) * pixel_size, -1, 0)
    ).xyz;
    vec3 light = vec3(0, 1.0, -1.0);

    float max_depth = 1e12;

    trace_result t = trace(from, direction);

    vec3 shade = vec3(1);

    if (t.i.depth < max_depth) {
        mat3x4 inverse_matrix = projective_inverse(t.e.matrix);

        vec3 position = vec4(t.i.position * 1.01, 1.0) * inverse_matrix;

        trace_result shadow = trace(
            position,
            light - position
        );

        vec3 light_direction = normalize(light - position);
        vec3 reflection_direction = reflect(light_direction, t.i.normal);
        float diffuse = max(dot(light_direction, t.i.normal), 0);
        float specular =
            pow(max(dot(normalize(direction), reflection_direction), 0), 100);
        float ambient = 0.05;
        float lighting = mix(diffuse * 0.5 + specular * 0.5, 0, shadow.i.hit);
        shade *= lighting + ambient;

        //shade = position * 0.5 + 0.5;
    }

    shade = pow(shade, vec3(1.0 / 2.2));

    imageStore(color, ivec2(position), vec4(shade, 0));
}
