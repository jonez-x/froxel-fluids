import numpy as np


class WorldGrid:

    def __init__(self, n=64, h=1.0, alpha=2.0, iterations=40):
        self.n = n
        self.h = h
        self.alpha = alpha
        self.iterations = iterations

        self.density = np.zeros((n, n))
        self.u = np.zeros((n, n))
        self.v = np.zeros((n, n))
        self.pressure = np.zeros((n, n))

    def step(self, delta_t):
        self.add_source()
        self.add_buoyancy(delta_t)

        new_u = self.advect(self.u, delta_t)
        new_v = self.advect(self.v, delta_t)
        self.u, self.v = new_u, new_v

        self.project()

        self.density = self.advect(self.density, delta_t)

    # Quelle und Auftrieb
    def add_source(self):
        mid = self.n // 2
        self.density[2:5, mid - 3:mid + 3] = 1.0

    def add_buoyancy(self, delta_t):
        self.v += self.alpha * self.density * delta_t

    # Advektion
    def advect(self, field, delta_t):
        new_field = np.zeros_like(field)
        for j in range(self.n):
            for i in range(self.n):
                x_back = i - delta_t / self.h * self.u[j, i]
                y_back = j - delta_t / self.h * self.v[j, i]
                new_field[j, i] = self.bilinear_interpolate(field, x_back, y_back)
        return new_field

    @staticmethod
    def bilinear_interpolate(field, x_back, y_back):
        height, width = field.shape
        x_back = np.clip(x_back, 0, width - 1)
        y_back = np.clip(y_back, 0, height - 1)

        x0 = int(np.floor(x_back))
        y0 = int(np.floor(y_back))
        x1 = min(x0 + 1, width - 1)
        y1 = min(y0 + 1, height - 1)

        tx = x_back - x0
        ty = y_back - y0

        row0 = field[y0, x0] * (1 - tx) + field[y0, x1] * tx
        row1 = field[y1, x0] * (1 - tx) + field[y1, x1] * tx
        return row0 * (1 - ty) + row1 * ty

    # Projektion
    def project(self):
        div = self.compute_divergence()
        self.solve_pressure(div)
        self.subtract_gradient()

    def compute_divergence(self):
        n, h = self.n, self.h
        div = np.zeros((n, n))
        for j in range(1, n - 1):
            for i in range(1, n - 1):
                div[j, i] = (
                    self.u[j, i + 1] - self.u[j, i - 1]
                    + self.v[j + 1, i] - self.v[j - 1, i]
                ) / (2 * h)
        return div

    def solve_pressure(self, div):
        n, h = self.n, self.h
        self.pressure[:] = 0.0
        for _ in range(self.iterations):
            p = self.pressure
            p_new = p.copy()
            for j in range(1, n - 1):
                for i in range(1, n - 1):
                    p_new[j, i] = (
                        p[j, i + 1] + p[j, i - 1]
                        + p[j + 1, i] + p[j - 1, i]
                        - h * h * div[j, i]
                    ) / 4.0
            self.pressure = p_new

    def subtract_gradient(self):
        n, h, p = self.n, self.h, self.pressure
        for j in range(1, n - 1):
            for i in range(1, n - 1):
                self.u[j, i] -= (p[j, i + 1] - p[j, i - 1]) / (2 * h)
                self.v[j, i] -= (p[j + 1, i] - p[j - 1, i]) / (2 * h)