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
}
