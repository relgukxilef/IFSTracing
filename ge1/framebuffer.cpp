#include "framebuffer.h"

namespace ge1 {

    GLuint create_framebuffer(
        unsigned width, unsigned height, bool multisample, unsigned samples,
        GLenum target,
        span<const framebuffer_attachment_parameter> textures,
        span<const framebuffer_attachment_parameter> renderbuffers
    ) {
        GLuint name;
        glGenFramebuffers(1, &name);
        glBindFramebuffer(GL_FRAMEBUFFER, name);

        if (textures.size() > 8) {
            throw std::runtime_error("more than 8 outputs are not supported");
        }

        GLuint drawBuffers[8]{GL_NONE};

        for (unsigned int i = 0; i < textures.size(); i++) {
            auto& attachment = *(textures.begin() + i);

            if (
                attachment.attachment_point != GL_DEPTH_ATTACHMENT &&
                attachment.attachment_point != GL_STENCIL_ATTACHMENT &&
                attachment.attachment_point != GL_DEPTH_STENCIL_ATTACHMENT
            ) {
                drawBuffers[i] = attachment.attachment_point;
            }

            glGenTextures(1, attachment.name);
            glBindTexture(target, *attachment.name);

            if (!multisample) {
                // setting sampler parameters is illegal with multisample
                glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            }

            if (
                attachment.attachment_point == GL_DEPTH_ATTACHMENT ||
                attachment.attachment_point == GL_DEPTH_STENCIL_ATTACHMENT
            ) {
                glTexParameteri(
                    GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL
                );
                glTexParameteri(
                    GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE
                );
            }

            if (multisample) {
                glTexStorage2DMultisample(
                    target, samples, attachment.internal_format,
                    width, height, true
                );
            } else {
                glTexStorage2D(
                    target, 1, attachment.internal_format, width, height
                );
            }

            glFramebufferTexture(
                GL_FRAMEBUFFER, attachment.attachment_point, *attachment.name, 0
            );
        }

        glDrawBuffers(textures.size(), drawBuffers);

        for (auto& attachment : renderbuffers) {
            glGenRenderbuffers(1, attachment.name);
            glBindRenderbuffer(GL_RENDERBUFFER, *attachment.name);

            if (multisample) {
                glRenderbufferStorageMultisample(
                    GL_RENDERBUFFER, samples, attachment.internal_format,
                    width, height
                );
            } else {
                glRenderbufferStorage(
                    GL_RENDERBUFFER, attachment.internal_format, width, height
                );
            }

            glFramebufferRenderbuffer(
                GL_FRAMEBUFFER, attachment.attachment_point,
                GL_RENDERBUFFER, *attachment.name
            );
        }

        auto status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status == GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT) {
            throw std::runtime_error("framebuffer has incomplete attachment");
        } else if (status == GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT) {
            throw std::runtime_error(
                "framebuffer needs at least one attachment"
            );
        } else if (status == GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER) {
            throw std::runtime_error("framebuffer has no draw buffers");
        } else if (status == GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE) {
            throw std::runtime_error(
                "framebuffer attachments must have same number and "
                "location of samples"
            );
        } else if (status == GL_FRAMEBUFFER_UNSUPPORTED) {
            throw std::runtime_error(
                "framebuffer internal format not supported"
            );
        } else if (status == GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS) {
            throw std::runtime_error(
                "framebuffer attachments must have same target and number "
                "of layers"
            );
        } else if (status != GL_FRAMEBUFFER_COMPLETE) {
            throw std::runtime_error(
                "framebuffer is incomplete for unknown reason"
            );
        }

        return name;
    }

    GLuint create_framebuffer(
        unsigned width, unsigned height, GLenum target,
        span<const framebuffer_attachment_parameter> textures,
        span<const framebuffer_attachment_parameter> renderbuffers
    ) {
        return create_framebuffer(
            width, height, false, 0, target, textures, renderbuffers
        );
    }

    GLuint create_framebuffer_multisample(
        unsigned width, unsigned height, unsigned samples,
        span<const framebuffer_attachment_parameter> textures,
        span<const framebuffer_attachment_parameter> renderbuffers
    ) {
        return create_framebuffer(
            width, height, true, samples, GL_TEXTURE_2D_MULTISAMPLE,
            textures, renderbuffers
        );
    }

}
