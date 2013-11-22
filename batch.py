import argparse
import csv
import os
from paraview.simple import *

args = argparse.ArgumentParser(description='control nbody visualization.')
args.add_argument('filename', type=str,
                  help="filename to read")
args = args.parse_args()

f = open(args.filename, "r")
rdr = csv.reader(f)
for line in iter(rdr):
  if line[0] == 'x': continue
  assert(len(line) >= 6)
  pos = (float(line[0]), float(line[1]), float(line[2]))
  vel = (float(line[3]), float(line[4]), float(line[5]))
  mass = 1.0
  if(len(line) > 6):
    mass = float(line[6])
  #print("%f %f %f  %f %f %f" % (pos[0],pos[1],pos[2], vel[0],vel[1],vel[2]))
  sph = Sphere()
  sph.PhiResolution = 360
  sph.ThetaResolution = 360
  sph.Center = (pos[0], pos[1], pos[2])
  sph.UpdatePipeline()
  sph.Radius = mass / 2.0
  Show(sph)

timestep = os.path.basename(args.filename)
try:
  timestep = timestep[0:timestep.rindex(".")]
except ValueError: pass
Render()
WriteImage(timestep + ".png")
