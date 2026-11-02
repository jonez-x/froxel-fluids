use crate::field::Field;

const BUOYANCY: f32 = 0.1;
const JACOBI_ITERATIONS: usize = 40;

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
        self.project();
        self.r = self.advect(&self.r, dt);
    }

    pub fn project(&mut self) {
        let div = self.divergence();
        let p = self.pressure(&div);
        for k in 1..self.n - 1 {
            for j in 1..self.n - 1 {
                for i in 1..self.n - 1 {
                    let grad_x = 0.5 * (p.get(i + 1, j, k) - p.get(i - 1, j, k));
                    let grad_y = 0.5 * (p.get(i, j + 1, k) - p.get(i, j - 1, k));
                    let grad_z = 0.5 * (p.get(i, j, k + 1) - p.get(i, j, k - 1));
                    self.u.set(i, j, k, self.u.get(i, j, k) - grad_x);
                    self.v.set(i, j, k, self.v.get(i, j, k) - grad_y);
                    self.w.set(i, j, k, self.w.get(i, j, k) - grad_z);
                }
            }
        }
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
        let mut new_field = Field::new(self.n);
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

    pub fn divergence(&self) -> Field {
        let mut new_field = Field::new(self.n);
        for k in 1..self.n - 1 {
            for j in 1..self.n - 1 {
                for i in 1..self.n - 1 {
                    let div = 0.5
                        * (self.u.get(i + 1, j, k) - self.u.get(i - 1, j, k)
                            + self.v.get(i, j + 1, k)
                            - self.v.get(i, j - 1, k)
                            + self.w.get(i, j, k + 1)
                            - self.w.get(i, j, k - 1));
                    new_field.set(i, j, k, div);
                }
            }
        }
        new_field
    }

    pub fn pressure(&self, div: &Field) -> Field {
        let mut p = Field::new(self.n);
        let mut p_next = Field::new(self.n);
        for _ in 0..JACOBI_ITERATIONS {
            for k in 1..self.n - 1 {
                for j in 1..self.n - 1 {
                    for i in 1..self.n - 1 {
                        let neighbors = p.get(i + 1, j, k)
                            + p.get(i - 1, j, k)
                            + p.get(i, j + 1, k)
                            + p.get(i, j - 1, k)
                            + p.get(i, j, k + 1)
                            + p.get(i, j, k - 1);
                        p_next.set(i, j, k, (neighbors - div.get(i, j, k)) / 6.0);
                    }
                }
            }
            std::mem::swap(&mut p, &mut p_next);
        }
        p
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
