import numpy as np
from yt.mods import *
from yt.utilities.physical_constants import cm_per_kpc, cm_per_mpc
import logging

logging.getLogger().setLevel(logging.WARNING)
data = dict(ZVelocity = freeprocessing.stream)
pf = load_uniform_grid(data, data["ZVelocity"].shape, cm_per_mpc,
                       nprocs=1)

print "[%d] velocity of ts: %s" % (freeprocessing.rank, freeprocessing.timestep)
slc = SlicePlot(pf, 2, ["ZVelocity"])
slc.set_cmap("ZVelocity", "Reds")
slc.annotate_grids(cmap=None)
fname = "%03u.xvel.%u.%u-%u-%u.png" % (freeprocessing.timestep,
                                       freeprocessing.rank,
                                       data["ZVelocity"].shape[0],
                                       data["ZVelocity"].shape[1],
                                       data["ZVelocity"].shape[2])
print "fname: ", fname
if freeprocessing.timestep > 0:
  slc.save(fname)
