import os
import itertools
import time
import cupoch as cph
cph.initialize_allocator(cph.PoolAllocation, 1000000000)
import numpy as np

# Graph2D from random nodes
n_nodes = 50
points = np.random.rand(n_nodes, 2)
gp = cph.geometry.Graph2D()
gp.points = cph.utility.Vector2fVector(points)

for c in itertools.combinations(list(range(n_nodes)), 2):
    gp.add_edge(c)
gp.set_edge_weights_from_distance()

gp.remove_edge((0, 30))

path = gp.dijkstra_path(0, 30)
print("Find path: ", path)
for i in range(len(path[:-1])):
    gp.paint_node_color(path[i], (0.0, 1.0, 0.0))
    gp.paint_edge_color((path[i], path[i+1]), (1.0, 0.0, 0.0))
gp.paint_node_color(path[-1], (0.0, 1.0, 0.0))
cph.visualization.draw_geometries([gp])