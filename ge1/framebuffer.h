#pragma once

#include <GL/glew.h>

#include "resources.h"
#include "span.h"

namespace ge1 {

    inline void delete_framebuffer(GLuint framebuffer) {
        glDeleteFramebuffers(1, &framebuffer);
    }

    typedef unique_object<delete_framebuffer> unique_framebuffer;

    struct framebuffer_attachment_parameter {
        GLenum attachment_point;
        GLuint *name;
        GLint internal_format;
    };

    GLuint create_framebuffer(
        unsigned width, unsigned height, GLenum target,
        span<const framebuffer_attachment_parameter> textures,
        span<const framebuffer_attachment_parameter> renderbuffers = {}
    );

    GLuint create_framebuffer_multisample(
        unsigned width, unsigned height, unsigned samples,
        span<const framebuffer_attachment_parameter> textures,
        span<const framebuffer_attachment_parameter> renderbuffers = {}
    );
}

