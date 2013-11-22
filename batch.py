import argparse
import csv
import logging
import math
import os
from paraview.simple import *

class SansHeader:
  """A file object that skips lines that start with 'x', as those are assumed
  to be header lines."""
  def __init__(self, f, comment="x"):
    self.f = f
    self.comment = comment

  def next(self):
    line = self.f.next()
    while(self.f and line.startswith(self.comment) or line.strip()==""):
      line = self.f.next()
    return line

  def __iter__(self): return self

  def __enter__(self): return self
  def __exit__(self, type, value, callback):
    self.f.__exit__(f, type, value, callback)

def mass_column(filename):
  """If present, the seventh column is mass.  This returns the mass of every
  particle in a list."""
  with open(filename, "r") as f:
    rdr = csv.reader(SansHeader(f))
    masses = []
    for line in iter(rdr):
      if(len(line) <= 6):
        masses.append(1.0)
      else:
        masses.append(float(line[6]))

  return masses

def defaultview():
  view = GetActiveView()
  if not view:
    view = CreateRenderView()
  view.CameraViewUp = [0, 1, 0]
  view.CameraFocalPoint = [0, 0, 0]
  view.CameraPosition = [0.001, 0.001, -2200]
  view.CameraViewAngle = 70
  view.Background = [ 0.1, 0.1, 0.3 ]
  return view

def printcam(cam):
  print("cam: ", cam.CameraPosition)
  print("ref: ", cam.CameraFocalPoint)
  print("vup: ", cam.CameraViewUp)

def get_bounds(filename):
  bounds = [ (-1,1), (-1,1), (-1,1) ]
  with open(filename, "r") as f:
    rdr = csv.reader(SansHeader(f))
    masses = []
    for line in iter(rdr):
      pos = (float(line[0]), float(line[1]), float(line[2]))
      bounds[0] = (min(bounds[0][0],pos[0]), max(bounds[0][1],pos[0]))
      bounds[1] = (min(bounds[1][0],pos[1]), max(bounds[1][1],pos[1]))
      bounds[2] = (min(bounds[2][0],pos[2]), max(bounds[2][1],pos[2]))
  return bounds

def average(values): return sum(values, 0.0) / len(values)

args = argparse.ArgumentParser(description='control nbody visualization.')
args.add_argument("-f", "--filename", type=str,
                  help="filename to read")
args.add_argument("-g", "--debug", help="enable debugging",
                  action="store_const", const=1)
args = args.parse_args()
if args.debug:
  logging.getLogger().setLevel(logging.DEBUG)

def spheres(filename, masseqz):
  """Returns a list of ParaView 'Sphere' objects."""
  sphlist = []
  f = open(args.filename, "r")
  rdr = csv.reader(SansHeader(f))
  for line in iter(rdr):
    if(len(line) < 3):
      logging.error("Not enough elements found per line.")
    pos = (float(line[0]), float(line[1]), float(line[2]))
    vel = (0.0, 0.0, 0.0)
    mass = 1.0

    if(len(line) > 3):
      vel = (float(line[3]), float(line[4]), float(line[5]))
    if(len(line) > 6):
      mass = float(line[6])
    logging.debug("%f %f %f" % (pos[0], pos[1], pos[2]))
    sph = Sphere()
    sph.PhiResolution = 360
    sph.ThetaResolution = 360
    sph.Center = (pos[0], pos[1], pos[2])
    sph.Radius = mass / (masseqz/1000)
    sphlist.append(sph)
  return sphlist

sphlist = spheres(args.filename, max(mass_column(args.filename)))

#bounds = get_bounds(args.filename)
#box = Box()
#box.XLength = math.fabs(bounds[0][1] - bounds[0][0])
#box.YLength = math.fabs(bounds[1][1] - bounds[1][0])
#box.ZLength = math.fabs(bounds[2][1] - bounds[2][0])
#box.Center = (average(bounds[0]), average(bounds[1]), average(bounds[2]))
#boxdrep = Show(box, view=view)
#boxdrep.Opacity = 0.2

view = defaultview()
for s in sphlist:
  s.UpdatePipeline()
  drep = Show(s, view=view)
  drep.Opacity = 0.75

timestep = os.path.basename(args.filename)
try:
  timestep = timestep[0:timestep.rindex(".")]
except ValueError: pass

fn = timestep + ".png"
SetActiveView(view)
Render(view=view)
Render(view=view)
WriteImage(fn, view=view, Magnification=3)
print "Saved", fn
#printcam(view)
