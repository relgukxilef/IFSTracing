#include "fractal.h"

#include <fstream>
#include <json/json.hpp>
#include <glm/gtc/matrix_transform.hpp>

using namespace glm;

fractal::fractal(const char *filename) {
    nlohmann::json json;
    {
        std::ifstream file(filename);
        file >> json;
    }

    auto json_mappings = json["mappings"];
    auto begin = new mat3x4[json_mappings.size()];
    mappings = ge1::span<mat3x4>(begin, begin + json_mappings.size());

    for (auto m = 0u; m < mappings.size(); ++m) {
        auto &json_mapping = json_mappings[m];
        auto &mapping = mappings.begin()[m];

        for (auto c = 0u; c < 3; ++c) {
            auto &json_column = json_mapping[c];
            auto &column = mapping[c];

            for (auto v = 0u; v < 4; ++v) {
                column[v] = json_column[v].get<float>();
            }
        }

        mapping = mat3x4(inverse(mat4(mapping)));
    }
}

fractal::~fractal() {
    delete[] mappings.begin();
}
