#pragma once

#include "camera/history.hpp"

#include <span>
#include <vector>

namespace ff::transport {

struct Settings {
    float dt = 1.0f / 60.0f;
    float decay = 1.0f;
};

struct FroxelFields {
    std::span<const float> velocity[3];
};

std::vector<float> transportStep(const camera::CameraHistory& history, const Settings& s,
                                 const FroxelFields& fields, std::span<const float> previous);

}
