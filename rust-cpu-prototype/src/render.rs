use crate::sim::Sim;
use macroquad::prelude::*;

pub fn draw_front_view(sim: &Sim) {
    let n = sim.n;
    let cell_size = screen_height() / n as f32;
    for j in 0..n {
        for i in 0..n {
            let mut sum = 0.0;
            for k in 0..n {
                sum += sim.r.get(i, j, k);
            }
            let brightness = 1.0 - (-0.5 * sum).exp();
            draw_rectangle(
                i as f32 * cell_size,
                (n - 1 - j) as f32 * cell_size,
                cell_size,
                cell_size,
                Color::new(brightness, brightness, brightness, 1.0),
            );
        }
    }
}
