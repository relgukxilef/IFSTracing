#version 460

// a group size of 32 is optimal for the shaders occupancy
layout(local_size_x = 8, local_size_y = 4, local_size_z = 1) in;
layout(std430) buffer;

layout(rgba8) uniform writeonly image2D color;

// max heap size
const uint queue_size = 16;

// heap structure
// recursion depth to stop
uint recursions[queue_size];
// squared depths of intersection with node to sort elements
float depths[queue_size];
// matrix from world space to node space
mat3x4 matrices[queue_size];
// accumulated normals for "anti-aliasing"
vec3 normals[queue_size];

// size of a pixel on the camera plane
uniform vec2 pixel_size;
// position of the bottom left corner on the camera plane
uniform vec2 pixel_offset;
// max number of recursions
uniform uint max_depth;

// number of elements on heap
uint size;

uniform mat4 model_matrix;

// material properties
uniform vec3 light_position;
uniform vec3 material_coefficients;
uniform vec3 material_color;
uniform float material_glossiness;

// list of affine functions describing the fractal
layout(binding = 1) readonly buffer MapsInverse {
    mat3x4 maps_inverse[];
};

// Counter for number of iteration for debugging
uint steps = 0;

/*
Calculate projection of point onto vector
multiplied by the squared length of vector.
*/
vec3 project(vec3 point, vec3 vector) {
    return dot(point, vector) * vector;
}

/*
Fast inverse for homogenous matrices.
*/
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
    // squared depth is enough to find closest hit but faster to calculate
    float depth_squared;
    bool hit;
};

/*
Calculates the intersection of a ray with a unit sphere at the origin.
*/
intersection_result intersect(
    vec3 from, vec3 direction
) {
    intersection_result result;
    vec3 offset = from;

    if (dot(from, from) < 1.0) {
        // from is inside sphere
        result.hit = true;
        result.depth_squared = 0;
        return result;
    }
    if (dot(direction, from) > 0.0) {
        // ray points away from sphere
        result.hit = false;
        return result;
    }

    // divisions and normalizations are postponed
    float direction_squared = dot(direction, direction);
    float direction_squared_squared = direction_squared * direction_squared;

    // * d^2
    vec3 closest = project(-from, direction) + from * direction_squared;
    float closest_squared = dot(closest, closest); // * d^4
    result.hit = closest_squared < direction_squared_squared;

    if (result.hit) {
        // ray hits circle orthogonal to ray
        // distance between camera and center of sphere
        float distance_squared = dot(from, from);
        float depth_offset_squared = // * d^4
            direction_squared_squared - closest_squared;
        float circle_depth_squared = // * d^4
            distance_squared * direction_squared_squared - closest_squared;

        result.hit = circle_depth_squared > depth_offset_squared;

        if (result.hit) {
            // make depth a ratio of the direction length
            // to avoid non-uniform scaling with non-orthonormal mappings
            result.depth_squared = (
                circle_depth_squared + depth_offset_squared -
                2 * sqrt(circle_depth_squared * depth_offset_squared)
            ) / (direction_squared_squared * direction_squared);
        }
    }

    return result;
}

/*
Get first child of node in heap.
Second child is on next index.
*/
uint heap_child(uint parent) {
    return parent * 2 + 1;
}

/*
Get parent of node in heap.
*/
uint heap_parent(uint child) {
    return (child - 1) / 2;
}

/*
Element on the heap.
*/
struct element {
    uint recursion;
    float depth;
    mat3x4 matrix;
    vec3 normal;
};

/*
Write element into heap.
*/
void set(uint i, element e) {
    matrices[i] = e.matrix;
    normals[i] = e.normal;
    recursions[i] = e.recursion;
    depths[i] = e.depth;
}
/*
Read element into heap.
*/
element get(uint i) {
    element e;
    e.matrix = matrices[i];
    e.normal = normals[i];
    e.recursion = recursions[i];
    e.depth = depths[i];
    return e;
}

/*
Swap two elements on the heap.
*/
void heap_swap(uint a, uint b) {
    element e = get(a);
    set(a, get(b));
    set(b, e);
}

/*
Perform insertion into the heap preserving the heap structure.
*/
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

/*
Take off the closest node from the heap preserving the heap structure.
*/
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

        // replacing ifs with mix isn't worth it
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
    vec3 p;
};

/*
Trace fractal, calculating depth of intersection and accumulated normals.
*/
trace_result trace(vec3 from, vec3 direction) {
    trace_result result;
    result.i.depth_squared = 1e12;

    size = 0;

    element e;
    e.matrix = mat3x4(1);
    e.recursion = 0;
    e.depth = 0;
    e.normal = vec3(0);

    heap_insert(e);

    while (size > 0 && e.depth < result.i.depth_squared) {
        e = heap_pop();
        //steps++;

        for (uint m = 0; m < maps_inverse.length(); m++) {
            // TODO: maybe swap splitting and intersection
            mat3x4 map = maps_inverse[m];
            element child = e;
            child.recursion++;
            child.matrix = mat3x4(mat4(e.matrix) * mat4(map));
            vec3 child_from = vec4(from, 1.0) * child.matrix;
            vec3 child_direction = direction * mat3(child.matrix);

            intersection_result i = intersect(
                child_from, child_direction
            );

            if (i.hit) {
                child.depth = i.depth_squared;
                vec3 position =
                    child_from + child_direction * sqrt(i.depth_squared);
                if (e.recursion + 5 > max_depth) {
                    // average normals
                    // reformulating it to not need the inverse matrix is slower
                    child.normal +=
                        normalize(position * inverse(mat3(child.matrix)));
                }
                if (e.recursion < max_depth && size < queue_size) {
                    heap_insert(child);
                } else {
                    if (i.depth_squared < result.i.depth_squared) {
                        result.e = child;
                        result.i = i;
                        result.p = position;
                    }
                }
            }
        }
    }

    result.e.normal = normalize(result.e.normal);
    return result;
}

/*
Test whether there is an intersection without calculating depths and
without sorting.
Uses the heap as a stack, but that's ok because this function is not called
simultaniously with trace.
*/
bool shadow_trace(vec3 from, vec3 direction) {
    size = 1;

    matrices[0] = mat3x4(1);
    recursions[0] = 0;

    while (size > 0) {
        size--;
        uint recursion = recursions[size];
        mat3x4 matrix = matrices[size];
        for (uint m = 0; m < maps_inverse.length(); m++) {
            mat3x4 map = maps_inverse[m];
            uint child_recursion = recursion + 1;
            mat3x4 child_matrix = mat3x4(mat4(matrix) * mat4(map));
            vec3 child_from = vec4(from, 1.0) * child_matrix;
            vec3 child_direction = direction * mat3(child_matrix);

            intersection_result i = intersect(
                child_from, child_direction
            );

            if (i.hit) {
                if (recursion < max_depth && size < queue_size) {
                    recursions[size] = child_recursion;
                    matrices[size] = child_matrix;
                    size++;
                } else {
                    return true;
                }
            }
        }
    }

    return false;
}

void main(void) {
    // calculate ray parameters from screen position
    uvec2 position = gl_GlobalInvocationID.xy;
    vec3 from = (model_matrix * vec4(0, 0, 0, 1)).xyz;
    vec3 direction = (
        model_matrix * vec4((vec2(position) - pixel_offset) * pixel_size, -1, 0)
    ).xyz;

    float max_depth = 1e12;

    // performing shadow_trace before trace is not worth it

    trace_result t = trace(from, direction);

    vec3 shade = vec3(1);

    if (t.i.depth_squared < max_depth) {
        // fractal was hit, perform shading
        shade = material_color;
        mat3x4 inverse_matrix = projective_inverse(t.e.matrix);

        // TODO: instead of inverse transforming the point one can forward
        // transform the fractal
        vec3 position = vec4(t.p * 1.01, 1.0) * inverse_matrix;

        vec3 light_direction = normalize(light_position - position);
        vec3 reflection_direction = reflect(light_direction, t.e.normal);
        float diffuse = dot(light_direction, t.e.normal);
        float specular = pow(
            max(dot(normalize(direction), reflection_direction), 0),
            material_glossiness
        );

        float lighting = 0;

        if (diffuse > 0) {
            // surface is facing the light, perform shadow trace
            bool shadow = shadow_trace(
                position,
                light_position - position
            );
            lighting = mix(
                diffuse * material_coefficients.x +
                specular * material_coefficients.y,
                0, shadow
            );
        }

        shade *= lighting + material_coefficients.z;
    }

    // to visualize number of iterations
    //shade = vec3(min(float(steps), 20) / 20);

    // gamma correction
    shade = pow(shade, vec3(1.0 / 2.2));

    imageStore(color, ivec2(position), vec4(shade, 0));
}
