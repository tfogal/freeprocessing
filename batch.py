import argparse
import csv
import logging
import os
from paraview.simple import *

args = argparse.ArgumentParser(description='control nbody visualization.')
args.add_argument("-f", "--filename", type=str,
                  help="filename to read")
args.add_argument("-g", "--debug", help="enable debugging",
                  action="store_const", const=1)
args = args.parse_args()
if args.debug:
  logging.getLogger().setLevel(logging.DEBUG)

def mass_column(filename):
  with open(filename, "r") as f:
    rdr = csv.reader(f)
    masses = []
    for line in iter(rdr):
      if line[0] == 'x': continue # skip first (header) line.
      if(len(line) <= 6):
        masses.append(1.0)
      else:
        masses.append(float(line[6]))

  return masses

massmax = max(mass_column(args.filename))
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
  logging.debug("%f %f %f" % (pos[0], pos[1], pos[2]))
  sph = Sphere()
  sph.PhiResolution = 360
  sph.ThetaResolution = 360
  sph.Center = (pos[0], pos[1], pos[2])
  sph.UpdatePipeline()
  sph.Radius = mass / massmax * 4.0
  Show(sph)

timestep = os.path.basename(args.filename)
try:
  timestep = timestep[0:timestep.rindex(".")]
except ValueError: pass
Render()
WriteImage(timestep + ".png")
