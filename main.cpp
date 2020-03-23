#include <iostream>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>

#include "ge1/program.h"
#include "ge1/vertex_buffer.h"

using namespace std;
using namespace ge1;
using namespace glm;

initializer_list<const vec2> quad_positions = {
    {-1, -1}, {-1, 1}, {1, -1}, {1, 1}
};

unsigned window_width, window_height, max_depth;
GLuint view_plane_size_uniform, scanline_stride_uniform, image_stride_uniform;
GLuint max_depth_uniform;

GLuint recursion_depth_buffer, depth_buffer, ray_buffer;

void window_size_callback(GLFWwindow*, int width, int height) {
    window_width = static_cast<unsigned int>(width);
    window_height = static_cast<unsigned int>(height);
    float aspect_ratio = static_cast<float>(window_height) / window_width;

    glViewport(0, 0, width, height);

    // TODO: align
    unsigned scanline_stride = window_width;
    unsigned image_stride = scanline_stride * window_height;
    unsigned element_count = image_stride * max_depth;

    glUniform2f(view_plane_size_uniform, 1.0f, aspect_ratio);
    glUniform1ui(scanline_stride_uniform, scanline_stride);
    glUniform1ui(image_stride_uniform, image_stride);

    glBindBuffer(GL_COPY_WRITE_BUFFER, recursion_depth_buffer);
    glBufferData(
        GL_COPY_WRITE_BUFFER, element_count * sizeof(unsigned),
        nullptr, GL_STREAM_COPY
    );
    glBindBuffer(GL_COPY_WRITE_BUFFER, depth_buffer);
    glBufferData(
        GL_COPY_WRITE_BUFFER, element_count * sizeof(float),
        nullptr, GL_STREAM_COPY
    );
    glBindBuffer(GL_COPY_WRITE_BUFFER, ray_buffer);
    glBufferData(
        GL_COPY_WRITE_BUFFER, element_count * 3 * 4 * sizeof(float),
        nullptr, GL_STREAM_COPY
    );

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, recursion_depth_buffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, depth_buffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, ray_buffer);
}

int main()
{
    GLFWwindow* window;

    if (!glfwInit()) {
        throw runtime_error("Failed to initialize GLFW.");
    }

    glfwWindowHint(GLFW_SRGB_CAPABLE, GLFW_TRUE);
    window = glfwCreateWindow(100, 100, "IFS Tracer", nullptr, nullptr);

    if (!window) {
        glfwTerminate();
        throw runtime_error("Failed to create window.");
    }

    glfwMakeContextCurrent(window);

    if (glewInit() != GLEW_OK) {
        throw runtime_error("Failed to initilize GLEW.");
    }

    glEnable(GL_FRAMEBUFFER_SRGB);

    enum attributes : GLuint {
        position
    };

    // Sierpiński triangle
    array<const mat3x4, 3> maps{{
        {
            0.5, 0.0, 0.0, -0.25,
            0.0, 0.5, 0.0, -0.25,
            0.0, 0.0, 0.5, 0.0
        }, {
            0.5, 0.0, 0.0, 0.25,
            0.0, 0.5, 0.0, -0.25,
            0.0, 0.0, 0.5, 0.0
        }, {
            0.5, 0.0, 0.0, 0.0,
            0.0, 0.5, 0.0, 0.25,
            0.0, 0.0, 0.5, 0.0
        },
    }};

    array<mat3x4, 3> maps_inverse;
    for (auto i = 0u; i < 3; i++) {
        mat4 m = mat4(maps[i]);
        m = inverse(m);
        maps_inverse[i] = mat3x4(m);
    }

    auto trace_program = compile_program(
        "trace_vs.glsl", nullptr, nullptr, nullptr, "trace_fs.glsl", {},
        {{"position", position}}
    );
    get_uniform_locations(
        trace_program, {
            {"view_plane_size", &view_plane_size_uniform},
            {"scanline_stride", &scanline_stride_uniform},
            {"image_stride", &image_stride_uniform},
            {"max_depth", &max_depth_uniform},
        }
    );

    auto maps_buffer = create_buffer<const mat3x4>(
        GL_SHADER_STORAGE_BUFFER, GL_STATIC_DRAW,
        {maps.begin(), maps.end()}
    );
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, maps_buffer);
    auto maps_inverse_buffer = create_buffer<const mat3x4>(
        GL_SHADER_STORAGE_BUFFER, GL_STATIC_DRAW,
        {maps_inverse.begin(), maps_inverse.end()}
    );
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, maps_inverse_buffer);
    glGenBuffers(1, &recursion_depth_buffer);
    glGenBuffers(1, &depth_buffer);
    glGenBuffers(1, &ray_buffer);

    auto quad_buffer = create_buffer<const vec2>(
        GL_ARRAY_BUFFER, GL_STATIC_DRAW, quad_positions
    );
    GLuint quad_array;
    glCreateVertexArrays(1, &quad_array);
    glBindVertexArray(quad_array);
    glEnableVertexAttribArray(position);
    glBindBuffer(GL_ARRAY_BUFFER, quad_buffer);
    glVertexAttribPointer(
        position, 2, GL_FLOAT, GL_FALSE, sizeof(vec2), nullptr
    );

    glUseProgram(trace_program);
    max_depth = 5;

    glUniform1ui(max_depth_uniform, max_depth);

    {
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        window_size_callback(window, width, height);
    }

    glfwSetWindowSizeCallback(window, &window_size_callback);

    while (!glfwWindowShouldClose(window)) {
        glClear(GL_COLOR_BUFFER_BIT);

        glBindVertexArray(quad_array);

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        glfwSwapBuffers(window);

        glfwPollEvents();
    }

    return 0;
}
