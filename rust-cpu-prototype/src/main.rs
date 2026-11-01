mod field;
use field::Field;
mod sim;
use sim::Sim;

const N: usize = 48;

fn main() {
    let mut density = Field::new(N);
    density.set(11, 10, 10, 2.0);
    let test1 = density.sample_at_position(10.0, 10.0, 10.0);
    let test2 = density.sample_at_position(10.5, 10.0, 10.0);
    println!("{}, {}", test1, test2);

    let mut sim = Sim::new(N);
    sim.add_smoke_source();
    let sum_d: f32 = sim.r.data.iter().sum();
    println!("Density Sum={}", sum_d)
}
