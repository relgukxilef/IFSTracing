#include "fractal.h"

#include <fstream>
#include <json/json.hpp>
#include <glm/gtc/matrix_transform.hpp>

using namespace glm;
using namespace nlohmann;

namespace glm {
    // json calls this to deserialize vec3
    void from_json(const json& j, vec3& v) {
        v.x = j.at(0).get<float>();
        v.y = j.at(1).get<float>();
        v.z = j.at(2).get<float>();
    }
}

fractal::fractal(const char *filename) {
    json json;
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

    light_position = json["light"].get<vec3>();

    auto json_material = json["material"];
    coefficients = json_material["coefficients"].get<vec3>();
    color = json_material["color"].get<vec3>();
    glossiness = json_material["glossiness"].get<float>();
}

fractal::~fractal() {
    delete[] mappings.begin();
}
