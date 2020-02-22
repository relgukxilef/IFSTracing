#pragma once

#include <GL/glew.h>

#include "span.h"

namespace ge1 {

    template<class T>
    GLuint create_buffer(GLenum target, GLenum usage, span<T> data) {
        GLuint name;
        glGenBuffers(1, &name);
        glBindBuffer(target, name);
        glBufferData(target, data.size() * sizeof(T), data.begin(), usage);
        return name;
    }

    template<class T>
    void buffer_sub_data(GLuint buffer, span<T> data) {
        glBindBuffer(GL_COPY_WRITE_BUFFER, buffer);
        glBufferSubData(
            GL_COPY_WRITE_BUFFER, 0,
            data.size() * sizeof(T), data.begin()
        );
    }
}
