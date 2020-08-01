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

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"

using namespace std;
using namespace ge1;
using namespace glm;

unsigned window_width, window_height, max_depth;
GLuint framebuffer = 0, color_texture = 0;
mat4 model_matrix;
vec2 rotation;
vec3 translation;

vec2 previous_mouse_position;

struct {
    GLuint model_matrix, light_position, material_coefficients;
    GLuint material_color, material_glossiness;
    GLuint pixel_size, pixel_offset, color, max_depth;
} uniforms;

enum image_units : GLuint {
    color
};

GLuint trace_program;

enum struct operation {
    none, rotate, pan, zoom
} current_operation = operation::none;

ImGuiIO* io;

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
    if (io->WantCaptureMouse) return;

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
        uniforms.pixel_size, 1.0f / window_width, aspect_ratio / window_height
    );
    glUniform2f(
        uniforms.pixel_offset, window_width * 0.5f, window_height * 0.5f
    );

    glDeleteFramebuffers(1, &framebuffer);
    glDeleteTextures(1, &color_texture);
    framebuffer = create_framebuffer(
        window_width, window_height, GL_TEXTURE_2D, {
            {GL_COLOR_ATTACHMENT0, &color_texture, GL_RGBA8}
        }
    );
}

void set_fractal(fractal f) {
    auto maps_inverse_buffer = create_buffer<const mat3x4>(
        GL_SHADER_STORAGE_BUFFER, GL_STATIC_DRAW,
        {f.mappings.begin(), f.mappings.end()}
    );
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, maps_inverse_buffer);

    glUseProgram(trace_program);
    max_depth = 3;

    glUniform1ui(uniforms.max_depth, max_depth);
    glUniform1i(uniforms.color, image_units::color);
    glUniform3fv(uniforms.light_position, 1, glm::value_ptr(f.light_position));
    glUniform3fv(
        uniforms.material_coefficients, 1,
        glm::value_ptr(f.coefficients)
    );
    glUniform3fv(uniforms.material_color, 1,glm::value_ptr(f.color));
    glUniform1f(uniforms.material_glossiness, f.glossiness);
}

int main()
{
    GLFWwindow* window;

    if (!glfwInit()) {
        throw runtime_error("Failed to initialize GLFW.");
    }

    glfwWindowHint(GLFW_SRGB_CAPABLE, GLFW_TRUE);
    window = glfwCreateWindow(800, 800, "IFS Tracer", nullptr, nullptr);

    if (!window) {
        glfwTerminate();
        throw runtime_error("Failed to create window.");
    }

    glfwMakeContextCurrent(window);

    if (glewInit() != GLEW_OK) {
        throw runtime_error("Failed to initilize GLEW.");
    }

    glEnable(GL_FRAMEBUFFER_SRGB);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    io = &ImGui::GetIO();
    io->Fonts->AddFontFromFileTTF("fonts/Lato-Regular.ttf", 20.0f);

    //fractal f("Sierpinski Triangle.json");

    trace_program = compile_program("trace.glsl", {});
    get_uniform_locations(
        trace_program, {
            {"pixel_size", &uniforms.pixel_size},
            {"pixel_offset", &uniforms.pixel_offset},
            {"max_depth", &uniforms.max_depth},
            {"color", &uniforms.color},
            {"model_matrix", &uniforms.model_matrix},
            {"light_position", &uniforms.light_position},
            {"material_coefficients", &uniforms.material_coefficients},
            {"material_color", &uniforms.material_color},
            {"material_glossiness", &uniforms.material_glossiness},
            {"model_matrix", &uniforms.model_matrix},
        }
    );

    set_fractal(fractal("Koch Curve.json"));

    glfwSetCursorPosCallback(window, &cursor_position_callback);
    glfwSetMouseButtonCallback(window, &mouse_button_callback);

    {
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        window_size_callback(window, width, height);
    }

    model_matrix = translate(mat4(1), vec3(0, 0, 4));

    glfwSetWindowSizeCallback(window, &window_size_callback);

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    GLuint time_elapsed_query;
    glGenQueries(1, &time_elapsed_query);

    while (!glfwWindowShouldClose(window)) {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        glBeginQuery(GL_TIME_ELAPSED, time_elapsed_query);

        glBindImageTexture(
            color, color_texture,
            0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8
        );

        glUniformMatrix4fv(
            uniforms.model_matrix, 1, GL_FALSE, value_ptr(model_matrix)
        );

        glDispatchCompute(
            (window_width - 1) / 8 + 1,
            (window_height - 1) / 4 + 1, 1
        );

        glEndQuery(GL_TIME_ELAPSED);

        glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

        glBlitFramebuffer(
            0, 0, window_width, window_height,
            0, 0, window_width, window_height,
            GL_COLOR_BUFFER_BIT, GL_NEAREST
        );

        ImGui::Begin("Settings", nullptr, ImGuiWindowFlags_MenuBar);
        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("Open"))
            {
                for (const char* file : {
                    "Sierpinski Tetrahedron", "Sierpinski Carpet", "Koch Curve",
                    "Dragon Curve"
                }) {
                    if (ImGui::MenuItem(file)) {
                        set_fractal(fractal((string(file) + ".json").c_str()));
                        break;
                    }
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }
        if (ImGui::SliderInt(
            "Recursion Depth", reinterpret_cast<int*>(&max_depth), 0, 10
        )) {
            glUniform1ui(uniforms.max_depth, max_depth);
        }
        ImGui::End();

        GLuint query_result;
        glGetQueryObjectuiv(time_elapsed_query, GL_QUERY_RESULT, &query_result);

        float elapsed_time = query_result * 1e-9f;
        static const unsigned profiling_history_size = 100;
        static float elapsed_time_history[profiling_history_size];
        static float elapsed_time_running_average = 1;
        static unsigned profiling_history_index = 0;
        elapsed_time_history[profiling_history_index] = elapsed_time;
        elapsed_time_running_average +=
            (elapsed_time - elapsed_time_running_average) * 0.1f;
        profiling_history_index =
            (profiling_history_index + 1) % profiling_history_size;

        static bool show_profiler_window = true;
        if (show_profiler_window)
        {
            ImGui::Begin("Profiler", &show_profiler_window);
            ImGui::Text("avg: %.2f ms", elapsed_time_running_average * 1000);
            ImDrawList* draw_list = ImGui::GetWindowDrawList();
            // ImDrawList API uses screen coordinates!
            ImVec2 offset = ImGui::GetCursorScreenPos();
            ImVec2 size = ImGui::GetContentRegionAvail();
            float delta_x = size.x / profiling_history_size;
            float delta_y = -size.y / elapsed_time_running_average * 0.5f;
            offset.y += size.y;

            ImVec2 previous = ImVec2(
                offset.x,
                offset.y +
                elapsed_time_history[profiling_history_size - 1] * delta_y
            );
            for (auto i = 0u; i < profiling_history_size; ++i) {
                offset.x += delta_x;
                ImVec2 point = ImVec2(
                    offset.x,
                    offset.y + elapsed_time_history[i] * delta_y
                );
                if (i != profiling_history_index) {
                    draw_list->AddLine(
                        previous, point, IM_COL32(255, 255, 0, 255), 2.0f
                    );
                }
                previous = point;
            }
            ImGui::End();
        }

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);

        glfwPollEvents();
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);

    return 0;
}
