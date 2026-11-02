use crate::{N, field::Field};

const BUOYANCY: f32 = 0.2;

///Sim struct that contains all data fields
pub struct Sim {
    ///grid size
    pub n: usize,
    ///density field
    pub r: Field,
    /// velocity in x
    pub u: Field,
    ///velocity in y
    pub v: Field,
    ///velocity in z
    pub w: Field,
}

impl Sim {
    pub fn new(n: usize) -> Self {
        Sim {
            n,
            r: Field::new(n),
            u: Field::new(n),
            v: Field::new(n),
            w: Field::new(n),
        }
    }

    pub fn step(&mut self, dt: f32) {
        self.add_smoke_source();
        self.add_buoyancy(dt);
        let new_u = self.advect(&self.u, dt);
        let new_v = self.advect(&self.v, dt);
        let new_w = self.advect(&self.w, dt);
        self.u = new_u;
        self.v = new_v;
        self.w = new_w;
        self.r = self.advect(&self.r, dt);
    }

    pub fn add_smoke_source(&mut self) {
        //center coordinate of every direction of the grid
        let c = self.n / 2;
        //radius of the source basically half the sidelenght
        let rad = 4;
        //.. is for range
        for k in c - rad..c + rad {
            for j in 1..5 {
                for i in c - rad..c + rad {
                    self.r.set(i, j, k, 1.0);
                    self.v.set(i, j, k, 1.0);
                }
            }
        }
    }

    pub fn advect(&self, field: &Field, dt: f32) -> Field {
        let mut new_field = Field::new(N);
        for k in 0..self.n {
            for j in 0..self.n {
                for i in 0..self.n {
                    let x_back = i as f32 - dt * self.u.get(i, j, k);
                    let y_back = j as f32 - dt * self.v.get(i, j, k);
                    let z_back = k as f32 - dt * self.w.get(i, j, k);
                    let new_sample = field.sample_at_position(x_back, y_back, z_back);
                    new_field.set(i, j, k, new_sample);
                }
            }
        }
        new_field
    }

    pub fn add_buoyancy(&mut self, dt: f32) {
        for k in 0..self.n {
            for j in 0..self.n {
                for i in 0..self.n {
                    self.v.set(
                        i,
                        j,
                        k,
                        self.v.get(i, j, k) + dt * BUOYANCY * self.r.get(i, j, k),
                    );
                }
            }
        }
    }
}
