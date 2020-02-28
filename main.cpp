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

unsigned window_width, window_height;
float aspect_ratio;

void window_size_callback(GLFWwindow*, int width, int height) {
    window_width = static_cast<unsigned int>(width);
    window_height = static_cast<unsigned int>(height);
    aspect_ratio = static_cast<float>(window_height) / window_width;

    glViewport(0, 0, width, height);
}

int main()
{
    GLFWwindow* window;

    if (!glfwInit()) {
        throw runtime_error("Failed to initialize GLFW.");
    }

    glfwWindowHint(GLFW_SRGB_CAPABLE, GLFW_TRUE);
    window = glfwCreateWindow(1280, 720, "IFS Tracer", nullptr, nullptr);

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
    GLuint view_plane_size_uniform;

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
            {"view_plane_size", &view_plane_size_uniform}
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

    {
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        window_size_callback(window, width, height);
    }
    glfwSetWindowSizeCallback(window, &window_size_callback);


    while (!glfwWindowShouldClose(window)) {
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(trace_program);
        glBindVertexArray(quad_array);



        glUniform2f(view_plane_size_uniform, 1.0f, aspect_ratio);

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        glfwSwapBuffers(window);

        glfwPollEvents();
    }

    return 0;
}
