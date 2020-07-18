#pragma once

#include <ge1/span.h>
#include <glm/mat3x4.hpp>

struct fractal {
    fractal(const char* filename);
    ~fractal();

    ge1::span<glm::mat3x4> mappings;
};

