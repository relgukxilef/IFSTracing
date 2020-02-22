#pragma once

#include <initializer_list>
#include <utility>

#include <GL/glew.h>

#include "resources.h"
#include "span.h"

namespace ge1 {

    inline void delete_program(GLuint program) {
        glDeleteProgram(program);
    }

    inline void delete_shader(GLuint shader) {
        glDeleteShader(shader);
    }

    typedef unique_object<delete_program> unique_program;
    typedef unique_object<delete_shader> unique_shader;

    GLuint compile_shader(GLenum type, const char* path);
    GLuint compile_shader_from_source(GLenum type, const char* source_code);

    struct program_attribute_parameter {
        const char* name;
        GLuint location;
    };

    struct program_uniform_parameter {
        const char* name;
        GLuint* location;
    };

    struct program_uniform_block_parameter {
        const char* name;
        GLuint binding;
    };

    struct program_shader_storage_block_parameter {
        const char* name;
        GLuint binding;
    };

    struct program_buffer_base_parameter {
        GLuint binding_point;
        GLuint buffer;
    };

    [[deprecated]]
    GLuint compile_program(
        const char* vertex_shader,
        const char* tesselation_control_shader,
        const char* tesselation_evaluation_shader,
        const char* geometry_shader,
        const char* fragment_shader,
        span<const GLuint> libraries,
        span<const program_attribute_parameter> attributes,
        span<const program_uniform_parameter> uniforms,
        span<const program_uniform_block_parameter> uniform_blocks = {}
    );

    GLuint compile_program(
        const char* vertex_shader,
        const char* tesselation_control_shader,
        const char* tesselation_evaluation_shader,
        const char* geometry_shader,
        const char* fragment_shader,
        span<const GLuint> libraries,
        span<const program_attribute_parameter> attributes
    );

    GLuint compile_program(
        const char* compute_shader, span<const GLuint> libraries = {}
    );

    void get_uniform_locations(
        GLuint program,
        span<const program_uniform_parameter> uniforms
    );

    void bind_uniform_blocks(
        GLuint program,
        span<const program_uniform_block_parameter> uniform_blocks
    );

    void bind_shader_storage_blocks(
        GLuint program,
        span<const program_shader_storage_block_parameter> shader_storage_blocks
    );

    void bind_buffer_bases(
        GLuint target, span<const program_buffer_base_parameter> buffers
    );
}
