// Camera. Right-handed world, +Y up; the camera looks along -Z in view space.
#pragma once

#include <glm/glm.hpp>

namespace ff::camera {

struct Camera {
    glm::vec3 position{0.0f};
    float yaw = 0.0f;    // radians, rotation around +Y; 0 looks along -Z
    float pitch = 0.0f;  // radians in [-pi/2, pi/2], positive looks up; yaw fixes the basis at the poles
    float fovY = glm::radians(60.0f);
    float aspect = 16.0f / 9.0f;
    float zNear = 0.1f;  // named zNear/zFar: <windows.h> defines near/far macros
    float zFar = 100.0f;

    // Reject non-finite or degenerate parameters with std::invalid_argument.
    void validate() const;
    glm::vec3 forward() const;
    glm::mat4 view() const;     // world -> view
    glm::mat4 invView() const;  // view -> world
};

}  // namespace ff::camera
