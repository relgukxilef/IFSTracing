
#include "program.h"

#include <iostream>
#include <fstream>
#include <exception>
#include <string>

namespace ge1 {

    using namespace std::literals::string_literals;

    GLuint compile_shader_from_source(GLenum type, const char* source_code) {
        GLuint name = glCreateShader(type);
        glShaderSource(name, 1, &source_code, nullptr);
        glCompileShader(name);

        GLint success;
        GLchar info_log[1024];
        glGetShaderiv(name, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(name, 1024, nullptr, info_log);
            glDeleteShader(name);
            throw std::runtime_error(std::string(info_log));
        }

        return name;
    }

    GLuint compile_shader(GLenum type, const char* path) {
        std::ifstream file(path);

        if (!file.is_open()) {
            throw std::runtime_error("Couldn't open "s + path);
        }

        std::string source(
            (std::istreambuf_iterator<char>(file)),
            std::istreambuf_iterator<char>()
        );
        file.close();

        char const* source_c_str = source.c_str();

        try {
            return compile_shader_from_source(type, source_c_str);
        } catch (std::runtime_error e) {
            throw std::runtime_error(
                "Could not compile "s + path + "\n"s + e.what()
            );
        }
    }

    GLuint compile_program(
        const char* vertex_shader, const char* tesselation_control_shader,
        const char* tesselation_evaluation_shader, const char* geometry_shader,
        const char* fragment_shader, const char* compute_shader,
        span<const GLuint> libraries,
        span<const program_attribute_parameter> attributes
    ) {
        unique_shader shaders[6] = {
            vertex_shader == nullptr ?
                0 : compile_shader(GL_VERTEX_SHADER, vertex_shader),
            tesselation_control_shader == nullptr ?
                0 : compile_shader(
                    GL_TESS_CONTROL_SHADER, tesselation_control_shader
                ),
            tesselation_evaluation_shader == nullptr ?
                0 : compile_shader(
                    GL_TESS_EVALUATION_SHADER, tesselation_evaluation_shader
                ),
            geometry_shader == nullptr ?
                0 : compile_shader(GL_GEOMETRY_SHADER, geometry_shader),
            fragment_shader == nullptr ?
                0 : compile_shader(GL_FRAGMENT_SHADER, fragment_shader),
            compute_shader == nullptr ?
                0 : compile_shader(GL_COMPUTE_SHADER, compute_shader)
        };

        GLuint name = glCreateProgram();

        for (auto& s : shaders) {
            if (s.get_name() != 0) {
                glAttachShader(name, s.get_name());
            }
        }

        for (GLuint s : libraries) {
            if (s != 0) {
                glAttachShader(name, s);
            }
        }

        for (auto& attribute : attributes) {
            glBindAttribLocation(name, attribute.location, attribute.name);
        }

        glLinkProgram(name);

        GLint success;
        GLchar info_log[1024];
        glGetProgramiv(name, GL_LINK_STATUS, &success);
        if (!success) {
            glGetProgramInfoLog(name, 1024, nullptr, info_log);
            glDeleteProgram(name);
            throw std::runtime_error("Linking failed:\n"s + info_log);
        }

        for (auto& s : shaders) {
            if (s.get_name() != 0) {
                glDetachShader(name, s.get_name());
            }
        }

        return name;
    }

    GLuint compile_program(
        const char* vertex_shader,
        const char* tesselation_control_shader,
        const char* tesselation_evaluation_shader,
        const char* geometry_shader,
        const char* fragment_shader,
        span<const GLuint> libraries,
        span<const program_attribute_parameter> attributes,
        span<const program_uniform_parameter> uniforms,
        span<const program_uniform_block_parameter> uniform_blocks
    ) {
        auto name = compile_program(
            vertex_shader, tesselation_control_shader,
            tesselation_evaluation_shader, geometry_shader, fragment_shader,
            nullptr,
            libraries, attributes
        );

        get_uniform_locations(name, uniforms);

        bind_uniform_blocks(name, uniform_blocks);

        return name;
    }

    GLuint compile_program(
        const char* vertex_shader, const char* tesselation_control_shader,
        const char* tesselation_evaluation_shader, const char* geometry_shader,
        const char* fragment_shader,
        span<const GLuint> libraries,
        span<const program_attribute_parameter> attributes
    ) {
        return compile_program(
            vertex_shader, tesselation_control_shader,
            tesselation_evaluation_shader, geometry_shader, fragment_shader,
            nullptr,
            libraries, attributes
        );
    }

    GLuint compile_program(
        const char* compute_shader, span<const GLuint> libraries
    ) {
        return compile_program(
            nullptr, nullptr, nullptr, nullptr, nullptr, compute_shader,
            libraries, {}
        );
    }

    void get_uniform_locations(
        GLuint program,
        span<const program_uniform_parameter> uniforms
    ) {
        for (auto& uniform : uniforms) {
            *uniform.location = static_cast<unsigned>(
                glGetUniformLocation(program, uniform.name)
            );
        }
    }

    void bind_uniform_blocks(
        GLuint program,
        span<const program_uniform_block_parameter> uniform_blocks
    ) {
        for (auto& uniform_block : uniform_blocks) {
            GLuint index = glGetUniformBlockIndex(program, uniform_block.name);
            if (index > 128) {
                throw std::runtime_error("large uniform block index");
            }
            glUniformBlockBinding(program, index, uniform_block.binding);
        }
    }

    void bind_shader_storage_blocks(
        GLuint program,
        span<const program_shader_storage_block_parameter> shader_storage_blocks
    ) {
        for (auto& shader_storage_block : shader_storage_blocks) {
            GLuint index = glGetProgramResourceIndex(
                program, GL_SHADER_STORAGE_BLOCK, shader_storage_block.name
            );
            if (index > 128) {
                throw std::runtime_error("large shader storage block index");
            }
            glShaderStorageBlockBinding(
                program, index, shader_storage_block.binding
            );
        }
    }

    void bind_buffer_bases(
        GLuint target, span<const program_buffer_base_parameter> buffers
    ) {
        for (auto buffer : buffers) {
            glBindBufferBase(target, buffer.binding_point, buffer.buffer);
        }
    }

}
