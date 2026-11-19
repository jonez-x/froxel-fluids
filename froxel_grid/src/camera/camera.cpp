#include "camera/camera.hpp"

#include <glm/gtc/constants.hpp>

#include <cmath>
#include <stdexcept>

namespace ff::camera {
namespace {

struct Basis {
    glm::vec3 right, up, forward;
};

Basis basis(const Camera& cam) {
    cam.validate();
    const float cy = std::cos(cam.yaw), sy = std::sin(cam.yaw);
    const float cp = std::cos(cam.pitch), sp = std::sin(cam.pitch);
    // Unlike lookAt(position, position + forward, worldUp), this remains defined at
    // the pitch poles and does not lose the direction when position + forward rounds to position.
    return {{cy, 0.0f, sy}, {-sy * sp, cp, cy * sp}, {cp * sy, sp, -cp * cy}};
}

}  // namespace

void Camera::validate() const {
    if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z) ||
        !std::isfinite(yaw) || !std::isfinite(pitch) || pitch < -glm::half_pi<float>() ||
        pitch > glm::half_pi<float>()) {
        throw std::invalid_argument("camera position/orientation must be finite and pitch in [-pi/2, pi/2]");
    }
    if (!std::isfinite(fovY) || fovY <= 0.0f || fovY >= glm::pi<float>() || !std::isfinite(aspect) ||
        aspect <= 0.0f || !std::isfinite(zNear) || !std::isfinite(zFar) || zNear <= 0.0f || zFar <= zNear) {
        throw std::invalid_argument("camera projection needs 0 < fovY < pi, aspect > 0, and 0 < near < far");
    }
    const float t = std::tan(fovY * 0.5f);
    const float ratio = zFar / zNear;
    if (!std::isfinite(t) || t <= 0.0f || !std::isfinite(t * aspect) || t * aspect <= 0.0f ||
        !std::isfinite(ratio) || ratio <= 1.0f) {
        throw std::invalid_argument("camera projection must be representable by finite float mapping parameters");
    }
}

glm::vec3 Camera::forward() const {
    return basis(*this).forward;
}

glm::mat4 Camera::view() const {
    const Basis b = basis(*this);
    const glm::vec3 translation(-glm::dot(b.right, position), -glm::dot(b.up, position), glm::dot(b.forward, position));
    if (!std::isfinite(translation.x) || !std::isfinite(translation.y) || !std::isfinite(translation.z)) {
        throw std::overflow_error("camera view translation is not representable as finite float");
    }
    return {glm::vec4(b.right.x, b.up.x, -b.forward.x, 0.0f),
            glm::vec4(b.right.y, b.up.y, -b.forward.y, 0.0f),
            glm::vec4(b.right.z, b.up.z, -b.forward.z, 0.0f), glm::vec4(translation, 1.0f)};
}

glm::mat4 Camera::invView() const {
    const Basis b = basis(*this);
    return {glm::vec4(b.right, 0.0f), glm::vec4(b.up, 0.0f), glm::vec4(-b.forward, 0.0f),
            glm::vec4(position, 1.0f)};
}

}  // namespace ff::camera
