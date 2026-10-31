import numpy as np

class WorldGrid:
    def __init__(self):
        self.densitiy = np.zeros((64, 64))
        self.u = np.zeros((64, 64))
        self.v = np.zeros((64, 64))
        self.pressure = np.zeros((64, 64))
        self.density[0][32] = 1
        
    def jacobi_iteration(self,n=40,delta_t=0.1,aplha=2,s=5):
        for _ in range(len(n)):    
            for j in range(len(self.pressure)):
                for i in range(len(self.pressure)):
                    density = self.density[j,i]
                    self.pressure[j,i] += s*delta_t
                    self.u[j,i] += self.u[j,i] + aplha*density*delta_t
                    self.v[j,i]
                    