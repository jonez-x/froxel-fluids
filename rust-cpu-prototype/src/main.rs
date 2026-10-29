mod field;
use field::Field;

const N: usize = 48;

fn main() {
    let mut density = Field::new(N);
    density.set(1, 2, 3, 5.0);
    println!("len={}", density.data.len());
}
