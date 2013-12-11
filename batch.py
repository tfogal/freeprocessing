#!python
from __future__ import with_statement
import argparse
import os
import logging
from paraview.simple import *
paraview.simple._DisableFirstRenderCameraReset()

# guts of a programmable filter
normalizeMass="""
input = self.GetPolyDataInput();
output = self.GetPolyDataOutput();
output.CopyStructure(input)
output.CopyAttributes(input)

mass = input.GetPointData().GetArray('mass');
if mass is not None:
  ntups = mass.GetNumberOfTuples();
  massNorm = mass.NewInstance();
  massNorm.SetNumberOfTuples(ntups);
  massNorm.SetName('MassNorm');

  minRadius = 100.00
  minMax = mass.GetRange();
  drange = (minMax[1] - minMax[0]) / (minRadius*10)

  for i in range(ntups):
    curmass = mass.GetTuple1(i)
    massNorm.SetTuple1(i, minRadius + (curmass - minMax[0])/drange)

  output.GetPointData().AddArray(massNorm);
"""

args = argparse.ArgumentParser(description='control nbody visualization.')
args.add_argument("-f", "--filename", type=str, help="filename to read")
args.add_argument("-g", "--debug", action="store_const", const=1,
                  help="enable debugging")
args.add_argument("--axes", action="store_const", const=1, help="axes")
args.add_argument("--eye", nargs=3, type=float,
                  default=[-2000.0, 100.0, 70000],
                  help="location of the camera")
args.add_argument("--ref", nargs=3, type=float,
                  default=[-4000.246, 600.739, -523.349],
                  help="location the camera focuses on")
args.add_argument("--vup", nargs=3, type=float,
                  default=[0.0, 1.0, 0.0],
                  help="view 'up' direction")
args = args.parse_args()
if args.debug:
  logging.getLogger().setLevel(logging.DEBUG)

reader = CSVReader(FileName=args.filename)

geomFilter = TableToPoints()
geomFilter.XColumn = 'x'
geomFilter.YColumn = 'y'
geomFilter.ZColumn = 'z'

marray = geomFilter.PointData.GetArray('mass')
if marray is None or marray.GetNumberOfTuples() == 0:
  print("Mass array is missing!")
  exit(1)

pyFilter = ProgrammableFilter()
pyFilter.Script = normalizeMass

glyphFilter = Glyph(pyFilter, GlyphType="Sphere")
glyphFilter.Scalars = ['POINTS', 'MassNorm']
glyphFilter.ScaleMode = 'scalar'
glyphFilter.GlyphType.ThetaResolution = 180
glyphFilter.GlyphType.PhiResolution = 90
glyphFilter.UpdatePipeline()

glyphRep = Show(glyphFilter)
glyphRep.Scaling = True
glyphRep.Orient = False

# assign lut by name is only supported in PV 4.1
#massNorm = glyphFilter.PointData.GetArray('MassNorm')
#glyphRep.ColorArrayName = 'MassNorm'
#glyphRep.Representation = 'Surface'
#glyphRep.LookupTable = AssignLookupTable(massNorm,'Rainbow Desaturated')

rview = GetRenderView()
rview.CameraPosition = args.eye
rview.CameraFocalPoint = args.ref
rview.CameraViewUp = args.vup
if not args.axes:
  rview.CenterAxesVisibility = 0
paraview.simple._DisableFirstRenderCameraReset()

def outname(infilename):
  timestep = os.path.basename(infilename)
  # lop off any existing extension.  But don't fail if there is no extension.
  try:
    timestep = timestep[0:timestep.rindex(".")]
  except ValueError: pass
  return timestep + ".png"

Render()
WriteImage(outname(args.filename), Magnification=3)
#eye = GetRenderView().CameraPosition
#ref = GetRenderView().CameraFocalPoint
#vup = GetRenderView().CameraViewUp
#print("eye: %7.3f %7.3f %7.3f" % (eye[0], eye[1], eye[2]))
#print("ref: %7.3f %7.3f %7.3f" % (ref[0], ref[1], ref[2]))
#print("vup: %7.3f %7.3f %7.3f" % (vup[0], vup[1], vup[2]))

Delete(glyphRep)
Delete(glyphFilter)
Delete(pyFilter)
Delete(geomFilter)
Delete(reader)
del glyphRep
del glyphFilter
del pyFilter
del geomFilter
del reader
