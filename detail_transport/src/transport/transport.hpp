#pragma once

#include "camera/history.hpp"

#include <span>
#include <vector>

namespace ff::transport {

struct Settings {
    float dt = 1.0f / 60.0f;      
    float relax = 0.0f;           
    float boundRelative = 0.5f;   
    float boundAbsolute = 0.0f;   


struct FroxelFields {
    std::span<const float> density;
    std::span<const float> velocity[3];  


float boundLow(float rho, const Settings& s);
float boundHigh(float rho, const Settings& s);


std::vector<float> transportStep(const camera::CameraHistory& history, const Settings& s,
                                 const FroxelFields& fields, std::span<const float> previous);

}  // namespace ff::transport
