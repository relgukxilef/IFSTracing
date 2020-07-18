#include <iostream>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/euler_angles.hpp>

#include "ge1/program.h"
#include "ge1/vertex_buffer.h"
#include "ge1/framebuffer.h"

#include "fractal.h"

using namespace std;
using namespace ge1;
using namespace glm;

unsigned window_width, window_height, max_depth;
GLuint pixel_size_uniform, pixel_offset_uniform;
GLuint color_uniform;
GLuint max_depth_uniform, inverse_radius_uniform;
GLuint framebuffer = 0, color_texture = 0;
mat4 model_matrix;
vec2 rotation;
vec3 translation;

vec2 previous_mouse_position;

struct {
    GLuint recursions, depths, froms, directions, lights, model_matrix;
} uniforms;

union {
    struct {
        GLuint recursions, depths, froms, directions, lights;
    };
    GLuint names[5] = {0};
} textures;

enum struct operation {
    none, rotate, pan, zoom
} current_operation = operation::none;

void cursor_position_callback(GLFWwindow*, double x, double y) {
    vec2 mouse_position(x, y);
    vec2 delta = mouse_position - previous_mouse_position;
    switch (current_operation) {
    case operation::rotate:
    {
        rotation += delta * -0.005f;
        model_matrix = translate(
            eulerAngleYXZ(rotation.x, rotation.y, 0.f), vec3(0, 0, 4)
        );
    }
        break;
    case operation::pan:
        break;
    case operation::zoom:
        break;
    case operation::none:
        break;
    }

    previous_mouse_position = mouse_position;
}

void mouse_button_callback(
    GLFWwindow* window, int button, int action, int
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
    if (action == GLFW_RELEASE) {
        current_operation = operation::none;
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

    fractal f("Sierpinski Triangle.json");

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
            {"model_matrix", &uniforms.model_matrix},
        }
    );

    auto maps_inverse_buffer = create_buffer<const mat3x4>(
        GL_SHADER_STORAGE_BUFFER, GL_STATIC_DRAW,
        {f.mappings.begin(), f.mappings.end()}
    );
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, maps_inverse_buffer);

    glUseProgram(trace_program);
    max_depth = 8;

    float radius = 0.5;

    glUniform1ui(max_depth_uniform, max_depth);
    glUniform1f(inverse_radius_uniform, 1.0f / radius);
    glUniform1i(color_uniform, color);
    glUniform1i(uniforms.recursions, recursions);
    glUniform1i(uniforms.depths, depths);
    glUniform1i(uniforms.froms, froms);
    glUniform1i(uniforms.directions, directions);
    glUniform1i(uniforms.lights, lights);

    glfwSetCursorPosCallback(window, &cursor_position_callback);
    glfwSetMouseButtonCallback(window, &mouse_button_callback);

    {
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        window_size_callback(window, width, height);
    }

    model_matrix = translate(mat4(1), vec3(0, 0, 4));

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
            uniforms.model_matrix, 1, GL_FALSE, value_ptr(model_matrix)
        );

        glDispatchCompute(
            (window_width - 1) / 32 + 1,
            (window_height - 1) / 32 + 1, 1
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
