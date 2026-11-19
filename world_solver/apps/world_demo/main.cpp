#include "render.hpp"
#include "world/sim.hpp"

#include <raylib.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iostream>

using ff::world::Sim;

constexpr std::size_t N = 48;

int main() {
    InitWindow(800, 600, "Froxel Fluids CPU Prototype");
    Sim sim(N);

    while (!WindowShouldClose()) {
        sim.step(1.0f);
        float max_div = 0.0f;
        for (float data : sim.divergence().data) {
            max_div = std::max(max_div, std::abs(data));
        }
        std::cout << "max div:" << max_div << '\n';
        BeginDrawing();
        ClearBackground(BLACK);
        draw_front_view(sim);
        EndDrawing();
    }

    CloseWindow();
}
