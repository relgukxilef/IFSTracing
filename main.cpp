#include <iostream>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "ge1/program.h"
#include "ge1/vertex_buffer.h"
#include "ge1/framebuffer.h"

using namespace std;
using namespace ge1;
using namespace glm;

unsigned window_width, window_height, max_depth, max_queue_depth;
GLuint pixel_size_uniform, pixel_offset_uniform;
GLuint color_uniform;
GLuint max_depth_uniform, inverse_radius_uniform;
GLuint framebuffer = 0, color_texture = 0;
mat4 view_matrix;

struct {
    GLuint recursions, depths, froms, directions, lights, view_matrix;
} uniforms;

union {
    struct {
        GLuint recursions, depths, froms, directions, lights;
    };
    GLuint names[5] = {0};
} textures;

enum struct operation {
    none, rotate, pan, zoom
} current_operation;

void cursor_position_callback(GLFWwindow*, double x, double y) {
    switch (current_operation) {
    case operation::rotate:

        break;
    case operation::pan:
        break;
    case operation::zoom:
        break;
    case operation::none:
        break;
    }
}

void mouse_button_callback(
    GLFWwindow* window, int button, int action, int modifiers
) {
    switch (current_operation) {
    case operation::rotate:

        break;
    case operation::pan:
        break;
    case operation::zoom:
        break;
    case operation::none:
        if (action == GLFW_PRESS) {
            double x, y;
            glfwGetCursorPos(window, &x, &y);

            if (button == GLFW_MOUSE_BUTTON_LEFT) {
                current_operation = operation::rotate;
            } else if (button == GLFW_MOUSE_BUTTON_MIDDLE) {
                current_operation = operation::pan;
            } else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
                current_operation = operation::zoom;
            }
        }
    }
}

void window_size_callback(GLFWwindow*, int width, int height) {
    window_width = static_cast<unsigned int>(width);
    window_height = static_cast<unsigned int>(height);
    float aspect_ratio = static_cast<float>(window_height) / window_width;

    glViewport(0, 0, width, height);

    glUniform2f(
        pixel_size_uniform, 1.0f / window_width, aspect_ratio / window_height
    );
    glUniform2f(
        pixel_offset_uniform, window_width * 0.5f, window_height * 0.5f
    );

    glDeleteFramebuffers(1, &framebuffer);
    glDeleteTextures(1, &color_texture);
    framebuffer = create_framebuffer(
        window_width, window_height, GL_TEXTURE_2D, {
            {GL_COLOR_ATTACHMENT0, &color_texture, GL_RGBA8}
        }
    );

    glDeleteTextures(5, textures.names);
    glGenTextures(5, textures.names);

    glBindTexture(GL_TEXTURE_3D, textures.recursions);
    glTexStorage3D(GL_TEXTURE_3D, 1, GL_R8UI, width, height, max_queue_depth);

    glBindTexture(GL_TEXTURE_3D, textures.depths);
    glTexStorage3D(GL_TEXTURE_3D, 1, GL_R32F, width, height, max_queue_depth);

    glBindTexture(GL_TEXTURE_3D, textures.froms);
    glTexStorage3D(
        GL_TEXTURE_3D, 1, GL_RGBA32F, width, height, max_queue_depth
    );

    glBindTexture(GL_TEXTURE_3D, textures.directions);
    glTexStorage3D(
        GL_TEXTURE_3D, 1, GL_RGBA32F, width, height, max_queue_depth
    );

    glBindTexture(GL_TEXTURE_3D, textures.lights);
    glTexStorage3D(
        GL_TEXTURE_3D, 1, GL_RGBA32F, width, height, max_queue_depth
    );
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

    enum image_units : GLuint {
        color, depths, recursions, froms, directions, lights
    };

    // Sierpiński triangle
    array<const mat3x4, 4> maps{{
        {
            0.5, 0.0, 0.0, 0,
            0.0, 0.5, 0.0, 0.5,
            0.0, 0.0, 0.5, 0.0
        }, {
            0.5, 0.0, 0.0, 0.0,
            0.0, 0.5, 0.0, -0.25,
            0.0, 0.0, 0.5, sqrt(3)/2
        }, {
            0.5, 0.0, 0.0, 0.1875*2,
            0.0, 0.5, 0.0, -0.25,
            0.0, 0.0, 0.5, -sqrt(3)/8
        }, {
            0.5, 0.0, 0.0, -0.1875*2,
            0.0, 0.5, 0.0, -0.25,
            0.0, 0.0, 0.5, -sqrt(3)/8
        },
    }};

    array<mat3x4, 4> maps_inverse;
    for (auto i = 0u; i < 4; i++) {
        mat4 m = mat4(maps[i]);
        m = inverse(m);
        maps_inverse[i] = mat3x4(m);
    }

    auto trace_program = compile_program("trace.glsl", {});
    get_uniform_locations(
        trace_program, {
            {"pixel_size", &pixel_size_uniform},
            {"pixel_offset", &pixel_offset_uniform},
            {"max_depth", &max_depth_uniform},
            {"inverse_radius", &inverse_radius_uniform},
            {"color", &color_uniform},
            {"recursions", &uniforms.recursions},
            {"depths", &uniforms.depths},
            {"froms", &uniforms.froms},
            {"directions", &uniforms.directions},
            {"lights", &uniforms.lights},
            {"view_matrix", &uniforms.view_matrix},
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

    glUseProgram(trace_program);
    max_depth = 8;
    max_queue_depth = 10;

    float radius = 0.5;

    glUniform1ui(max_depth_uniform, max_depth);
    glUniform1f(inverse_radius_uniform, 1.0f / radius);
    glUniform1i(color_uniform, color);
    glUniform1i(uniforms.recursions, recursions);
    glUniform1i(uniforms.depths, depths);
    glUniform1i(uniforms.froms, froms);
    glUniform1i(uniforms.directions, directions);
    glUniform1i(uniforms.lights, lights);

    {
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        window_size_callback(window, width, height);
    }

    view_matrix = lookAt(vec3(0, -4, 0), vec3(0), vec3(0, 0, 1));

    glfwSetWindowSizeCallback(window, &window_size_callback);

    while (!glfwWindowShouldClose(window)) {
        glBindImageTexture(
            color, color_texture,
            0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8
        );

        glBindImageTexture(
            recursions, textures.recursions,
            0, GL_FALSE, 0, GL_READ_WRITE, GL_R8UI
        );

        glBindImageTexture(
            depths, textures.depths,
            0, GL_FALSE, 0, GL_READ_WRITE, GL_R32F
        );

        glBindImageTexture(
            froms, textures.froms,
            0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F
        );

        glBindImageTexture(
            directions, textures.directions,
            0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F
        );

        glBindImageTexture(
            lights, textures.lights,
            0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F
        );

        glUniformMatrix4fv(
            uniforms.view_matrix, 1, GL_FALSE, value_ptr(view_matrix)
        );

        glDispatchCompute(
            (window_width - 1) / 128 + 1,
            (window_height - 1) / 2 + 1, 1
        );

        glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

        glBlitFramebuffer(
            0, 0, window_width, window_height,
            0, 0, window_width, window_height,
            GL_COLOR_BUFFER_BIT, GL_NEAREST
        );

        glfwSwapBuffers(window);

        glfwPollEvents();
    }

    return 0;
}
