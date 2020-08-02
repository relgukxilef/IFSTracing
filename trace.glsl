#version 460

layout(local_size_x = 8, local_size_y = 4, local_size_z = 1) in;
layout(std430) buffer;

layout(rgba8) uniform writeonly image2D color;

const uint queue_size = 16;

uint recursions[queue_size];
float depths[queue_size];
mat3x4 matrices[queue_size];
vec3 normals[queue_size];

uniform vec2 pixel_size;
uniform vec2 pixel_offset;
uniform uint max_depth;

uniform mat4 model_matrix;

uniform vec3 light_position;
uniform vec3 material_coefficients;
uniform vec3 material_color;
uniform float material_glossiness;

layout(binding = 1) readonly buffer MapsInverse {
    mat3x4 maps_inverse[];
};

uint size;

uint steps = 0;

/*
Calculate projection of point onto vector
multiplied by the squared length of vector.
*/
vec3 project(vec3 point, vec3 vector) {
    return dot(point, vector) * vector;
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

struct intersection_result {
    vec3 position;
    float depth;
    bool hit;
};

/*
Calculates the intersection of a ray with a unit sphere at the origin.
*/
intersection_result intersect(
    vec3 from, vec3 direction
) {
    // TODO: maybe split up into intersection_test (for shadow),
    // intersection_depth (for nodes) and intersection_position
    intersection_result result;
    vec3 offset = from;

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
    float direction_length_inverse = inversesqrt(dot(direction, direction));
    direction *= direction_length_inverse;

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
            result.position = from + result.depth * direction;
            // make depth a ratio of the direction length
            // to avoid non-uniform scaling with non-orthonormal mappings
            result.depth *= direction_length_inverse;
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
    vec3 normal;
};

void set(uint i, element e) {
    matrices[i] = e.matrix;
    normals[i] = e.normal;
    recursions[i] = e.recursion;
    depths[i] = e.depth;
}

element get(uint i) {
    element e;
    e.matrix = matrices[i];
    e.normal = normals[i];
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

        if (left < size) {
            if (
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

    element e;
    e.matrix = mat3x4(1);
    e.recursion = 0;
    e.depth = 0;
    e.normal = vec3(0);

    heap_insert(e);

    while (size > 0 && e.depth < result.i.depth) {
        e = heap_pop();
        //steps++;

        for (uint m = 0; m < maps_inverse.length(); m++) {
            // TODO: maybe swap splitting and intersection
            mat3x4 map = maps_inverse[m];
            element child = e;
            child.recursion++;
            child.matrix = mat3x4(mat4(e.matrix) * mat4(map));
            vec3 child_direction = direction * mat3(child.matrix);

            intersection_result i = intersect(
                vec4(from, 1.0) * child.matrix,
                child_direction
            );

            if (i.hit) {
                child.depth = i.depth;
                if (e.recursion + 5 > max_depth) {
                    // average normals
                    // TODO: make efficient
                    child.normal +=
                        normalize(i.position * inverse(mat3(child.matrix)));
                }
                if (e.recursion < max_depth && size < queue_size) {
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

    result.e.normal = normalize(result.e.normal);
    return result;
}

void main(void) {
    // TODO: load light position and material from file
    uvec2 position = gl_GlobalInvocationID.xy;
    vec3 from = (model_matrix * vec4(0, 0, 0, 1)).xyz;
    vec3 direction = (
        model_matrix * vec4((vec2(position) - pixel_offset) * pixel_size, -1, 0)
    ).xyz;
    vec3 light = vec3(1, 1.0, 1.0);

    float max_depth = 1e12;

    trace_result t = trace(from, direction);

    vec3 shade = vec3(1);

    if (t.i.depth < max_depth) {
        shade = material_color;
        mat3x4 inverse_matrix = projective_inverse(t.e.matrix);

        vec3 position = vec4(t.i.position * 1.01, 1.0) * inverse_matrix;

        vec3 light_direction = normalize(light - position);
        vec3 reflection_direction = reflect(light_direction, t.e.normal);
        float diffuse = dot(light_direction, t.e.normal);
        float specular = pow(
            max(dot(normalize(direction), reflection_direction), 0),
            material_glossiness
        );

        float lighting = 0;

        if (diffuse > 0) {
            trace_result shadow = trace(
                position,
                light - position
            );
            lighting = mix(
                diffuse * material_coefficients.x +
                specular * material_coefficients.y,
                0, shadow.i.depth < max_depth
            );
        }

        shade *= lighting + material_coefficients.z;
    }

    //shade = vec3(min(float(steps), 20) / 20);

    shade = pow(shade, vec3(1.0 / 2.2));

    imageStore(color, ivec2(position), vec4(shade, 0));
}
