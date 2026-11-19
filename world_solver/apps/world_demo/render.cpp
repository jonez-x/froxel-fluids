#include "render.hpp"

#include <raylib.h>

#include <cmath>
#include <cstddef>

void draw_front_view(const ff::world::Sim& sim) {
    std::size_t n = sim.n;
    float cell_size = static_cast<float>(GetScreenHeight()) / static_cast<float>(n);
    for (std::size_t j = 0; j < n; ++j) {
        for (std::size_t i = 0; i < n; ++i) {
            float sum = 0.0f;
            for (std::size_t k = 0; k < n; ++k) {
                sum += sim.r.get(i, j, k);
            }
            float brightness = 1.0f - std::exp(-0.5f * sum);
            DrawRectangleRec(
                Rectangle{
                    static_cast<float>(i) * cell_size,
                    static_cast<float>(n - 1 - j) * cell_size,
                    cell_size,
                    cell_size,
                },
                ColorFromNormalized(Vector4{brightness, brightness, brightness, 1.0f}));
        }
    }
}
