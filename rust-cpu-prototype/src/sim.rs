use crate::field::Field;

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
}
