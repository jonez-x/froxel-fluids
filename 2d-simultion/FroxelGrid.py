import numpy as np
from WorldGrid import WorldGrid

class FroxelGrid:
    def __init__(self,world_grid:WorldGrid):
        self.froxe_grid = np.zeros((12, 12))
        self.world_grid = world_grid
        
    