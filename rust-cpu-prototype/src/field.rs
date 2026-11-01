pub struct Field {
    pub n: usize,
    pub data: Vec<f32>,
}

impl Field {
    pub fn new(n: usize) -> Self {
        Field {
            n,
            data: vec![0.0; n * n * n],
        }
    }

    pub fn idx(&self, i: usize, j: usize, k: usize) -> usize {
        i + self.n * (j + self.n * k)
    }

    pub fn get(&self, i: usize, j: usize, k: usize) -> f32 {
        self.data[self.idx(i, j, k)]
    }

    pub fn set(&mut self, i: usize, j: usize, k: usize, value: f32) {
        let id = self.idx(i, j, k);
        self.data[id] = value;
    }

    pub fn sample_at_position(&self, x: f32, y: f32, z: f32) -> f32 {
        // 1.001 so we never take the outer edges of the grid when using floor
        let cl_x = x.clamp(0.0, self.n as f32 - 1.001);
        let cl_y = y.clamp(0.0, self.n as f32 - 1.001);
        let cl_z = z.clamp(0.0, self.n as f32 - 1.001);

        let i0 = cl_x.floor() as usize;
        let j0 = cl_y.floor() as usize;
        let k0 = cl_z.floor() as usize;

        let d_x = cl_x - cl_x.floor();
        let d_y = cl_y - cl_y.floor();
        let d_z = cl_z - cl_z.floor();

        let c00 = lerp(self.get(i0, j0, k0), self.get(i0 + 1, j0, k0), d_x);
        let c10 = lerp(self.get(i0, j0 + 1, k0), self.get(i0 + 1, j0 + 1, k0), d_x);
        let c01 = lerp(self.get(i0, j0, k0 + 1), self.get(i0 + 1, j0, k0 + 1), d_x);
        let c11 = lerp(
            self.get(i0, j0 + 1, k0 + 1),
            self.get(i0 + 1, j0 + 1, k0 + 1),
            d_x,
        );

        let c0 = lerp(c00, c10, d_y);
        let c1 = lerp(c01, c11, d_y);

        lerp(c0, c1, d_z)
    }
}

fn lerp(a: f32, b: f32, t: f32) -> f32 {
    a + (b - a) * t
}
