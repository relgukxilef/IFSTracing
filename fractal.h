#pragma once

#include <ge1/span.h>
#include <glm/vec3.hpp>
#include <glm/mat3x4.hpp>

struct fractal {
    fractal(const char* filename);
    ~fractal();

    ge1::span<glm::mat3x4> mappings;
    glm::vec3 light_position;
    glm::vec3 coefficients;
    glm::vec3 color;
    float glossiness;
};

