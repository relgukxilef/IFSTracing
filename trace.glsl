#version 460

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;
layout(std430) buffer;

layout(rgba8) uniform writeonly image2D color;

const uint queue_size = 16;

uint recursions[queue_size];
float depths[queue_size];
vec3 froms[queue_size];
vec3 directions[queue_size];
vec3 lights[queue_size];

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
    direction = normalize(direction);

    vec3 closest = project(-offset, direction) + offset;
    float closest_squared = dot(closest, closest);
    result.hit = closest_squared < 1.0;

    if (result.hit) {
        // ray hits circle orthogonal to ray
        float distance = length(offset);
        float depth_offset = sqrt(1 - closest_squared);
        float circle_depth = sqrt(distance * distance - closest_squared);
        if (circle_depth - depth_offset > 0) {
            // hit outside of sphere
            result.hit = true;
            result.depth = circle_depth - depth_offset;

        } else if (circle_depth + depth_offset > 0) {
            // hit inside of sphere
            result.hit = true;
            result.depth = circle_depth + depth_offset;
        } else {
            result.hit = false;
        }

        if (result.hit) {
            result.position = from + result.depth * direction;
            result.normal = result.position - sphere;
        }
    }

    return result;
}

float phong_shading(
    vec3 normal, vec3 position, vec3 direction, vec3 light_position
) {
    // TODO: add shadows
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
    froms[i] = e.from;
    directions[i] = e.direction;
    lights[i] = e.light;
    recursions[i] = e.recursion;
    depths[i] = e.depth;
}

element get(uint i) {
    element e;
    e.from = froms[i];
    e.direction = directions[i];
    e.light = lights[i];
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

trace_result trace(vec3 from, vec3 direction, vec3 light_position) {
    trace_result result;
    result.i.depth = 1e12;

    size = 0;

    element start;
    start.from = from;
    start.direction = direction;
    start.light = light_position;
    start.recursion = 0;
    start.depth = 0;

    heap_insert(start);

    while (size > 0) {
        element e = heap_pop();

        for (uint m = 0; m < maps_inverse.length(); m++) {
            mat4x3 map = maps_inverse[m];
            element child = e;
            child.recursion++;
            child.from = map * vec4(e.from, 1);
            child.direction = map * vec4(e.direction, 0);
            child.light = map * vec4(e.light, 1);

            intersection_result i = intersect(
                child.from, child.direction, vec3(0)
            );

            if (i.hit) {
                if (e.recursion < max_depth) {
                    heap_insert(child);
                } else {
                    if (i.depth < result.i.depth) {
                        result.e = e;
                        result.i = i;
                    }
                }
            }
        }
    }

    return result;
}

void main(void) {
    uvec2 position = gl_GlobalInvocationID.xy;
    vec3 from = (model_matrix * vec4(0, 0, 0, 1)).xyz;
    vec3 direction = (
        model_matrix * vec4((vec2(position) - pixel_offset) * pixel_size, -1, 0)
    ).xyz;
    vec3 light_position = vec3(0, 0.5, -0.5);

    float depth = 1e12;

    trace_result t = trace(from, direction, light_position);

    vec3 shade = vec3(1);

    if (t.i.depth < depth) {
        // TODO: i.position is relative to sphere
        // shadow
        //trace_result shadow = trace(
        //    t.i.position, light_position - t.i.position, vec3(0)
        //);

        shade = t.i.position * 0.5 + 0.5;

        depth = t.i.depth;
        shade *= phong_shading(
            t.i.normal, t.i.position,
            t.e.direction, t.e.light
        );

        //if (shadow.i.depth < depth) {
        //    shade = 0.0;
        //}
    }

    shade = pow(shade, vec3(1.0 / 2.2));

    imageStore(color, ivec2(position), vec4(shade, 0));
}
