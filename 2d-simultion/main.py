import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation

from WorldGrid import WorldGrid

DELTA_T = 0.1

grid = WorldGrid(n=64)

fig, ax = plt.subplots()
image = ax.imshow(grid.density, origin="lower", cmap="gray", vmin=0, vmax=1)


def update(frame):
    grid.step(DELTA_T)
    image.set_data(grid.density)
    return [image]


anim = FuncAnimation(fig, update, frames=500, interval=30)
plt.show()