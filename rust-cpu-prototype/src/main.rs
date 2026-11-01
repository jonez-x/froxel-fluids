mod field;
use field::Field;
mod render;
mod sim;
use macroquad::prelude::*;
use sim::Sim;

const N: usize = 48;

#[macroquad::main("Froxel Fluids CPU Prototype")]
async fn main() {
    let mut sim = Sim::new(N);
    sim.v.data.fill(0.3);

    loop {
        sim.add_smoke_source();
        sim.r = sim.advect(&sim.r, 1.0);

        clear_background(BLACK);
        render::draw_front_view(&sim);
        next_frame().await;
    }
}
