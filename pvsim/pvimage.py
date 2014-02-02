
try: paraview.simple
except: from paraview.simple import *

from paraview import coprocessing


#--------------------------------------------------------------
# Code generated from cpstate.py to create the CoProcessor.


# ----------------------- CoProcessor definition -----------------------

def CreateCoProcessor():
  def _CreatePipeline(coprocessor, datadescription):
    class Pipeline:
      a1_needsaname_PiecewiseFunction = CreatePiecewiseFunction( Points=[0.0, 0.0, 0.5, 0.0, 105.0, 1.0, 0.5, 0.0] )
      
      a3_cellNormals_PiecewiseFunction = CreatePiecewiseFunction( Points=[0.999999982885729, 0.0, 0.5, 0.0, 0.9999999828857291, 1.0, 0.5, 0.0] )
      
      a2_TextureCoordinates_PiecewiseFunction = CreatePiecewiseFunction( Points=[0.0, 0.0, 0.5, 0.0, 1.4142135623730951, 1.0, 0.5, 0.0] )
      
      a3_Normals_PiecewiseFunction = CreatePiecewiseFunction( Points=[0.999999982885729, 0.0, 0.5, 0.0, 0.9999999828857291, 1.0, 0.5, 0.0] )
      
      a1_needsaname_PVLookupTable = GetLookupTableForArray( "needsaname", 1, RGBPoints=[0.0, 0.23, 0.299, 0.754, 52.5, 0.865, 0.865, 0.865, 105.0, 0.706, 0.016, 0.15], VectorMode='Magnitude', NanColor=[0.25, 0.0, 0.0], ScalarOpacityFunction=a1_needsaname_PiecewiseFunction, ColorSpace='Diverging', ScalarRangeInitialized=1.0 )
      
      a3_cellNormals_PVLookupTable = GetLookupTableForArray( "cellNormals", 3, RGBPoints=[0.999999982885729, 0.23, 0.299, 0.754, 0.9999999828857291, 0.865, 0.865, 0.865, 0.9999999828857291, 0.706, 0.016, 0.15], VectorMode='Magnitude', NanColor=[0.25, 0.0, 0.0], ScalarOpacityFunction=a3_cellNormals_PiecewiseFunction, ColorSpace='Diverging', ScalarRangeInitialized=1.0 )
      
      a2_TextureCoordinates_PVLookupTable = GetLookupTableForArray( "TextureCoordinates", 2, RGBPoints=[0.0, 0.23, 0.299, 0.754, 0.7071067811865476, 0.865, 0.865, 0.865, 1.4142135623730951, 0.706, 0.016, 0.15], VectorMode='Magnitude', NanColor=[0.25, 0.0, 0.0], ScalarOpacityFunction=a2_TextureCoordinates_PiecewiseFunction, ColorSpace='Diverging', ScalarRangeInitialized=1.0 )
      
      a3_Normals_PVLookupTable = GetLookupTableForArray( "Normals", 3, RGBPoints=[0.999999982885729, 0.23, 0.299, 0.754, 0.9999999828857291, 0.865, 0.865, 0.865, 0.9999999828857291, 0.706, 0.016, 0.15], VectorMode='Magnitude', NanColor=[0.25, 0.0, 0.0], ScalarOpacityFunction=a3_Normals_PiecewiseFunction, ColorSpace='Diverging', ScalarRangeInitialized=1.0 )
      
      tmp_vti = coprocessor.CreateProducer( datadescription, "input" )
      
      Histogram1 = Histogram( guiName="Histogram1", CustomBinRanges=[0.0, 105.0], SelectInputArray=['POINTS', 'needsaname'] )
      
      SetActiveSource(tmp_vti)
      RenderView1 = coprocessor.CreateView( CreateRenderView, "image_0_%t.png", 1, 0, 1, 420, 546 )
      RenderView1.CameraViewUp = [-0.6001400955297163, -0.799702535069662, 0.017541982235173254]
      RenderView1.CacheKey = 0.0
      RenderView1.StereoType = 0
      RenderView1.StereoRender = 0
      RenderView1.CameraPosition = [-8.345336051586441, 18.37412911870887, -38.86607527525541]
      RenderView1.StereoCapableWindow = 0
      RenderView1.Background = [0.31999694819562063, 0.3400015259021897, 0.4299992370489052]
      RenderView1.CameraFocalPoint = [7.5, 7.5000000000000036, 7.500000000000024]
      RenderView1.CameraParallelScale = 12.99038105676658
      RenderView1.CenterOfRotation = [7.5, 7.5, 7.5]
      
      DataRepresentation1 = Show()
      DataRepresentation1.EdgeColor = [0.0, 0.0, 0.5000076295109483]
      DataRepresentation1.Slice = 7
      DataRepresentation1.SelectionPointFieldDataArrayName = 'needsaname'
      DataRepresentation1.ScalarOpacityFunction = a1_needsaname_PiecewiseFunction
      DataRepresentation1.ColorArrayName = ('POINT_DATA', 'needsaname')
      DataRepresentation1.ScalarOpacityUnitDistance = 1.7320508075688776
      DataRepresentation1.LookupTable = a1_needsaname_PVLookupTable
      DataRepresentation1.Representation = 'Points'
      DataRepresentation1.ScaleFactor = 1.5
      
      SetActiveSource(Histogram1)
      XYBarChartView1 = coprocessor.CreateView( CreateBarChartView, "image_1_%t.png", 1, 0, 1, 419, 546 )
      XYBarChartView1.CacheKey = 0.0
      XYBarChartView1.BottomAxisRange = [-7.933884143829346, 112.06611585617065]
      XYBarChartView1.TopAxisRange = [0.0, 6.66]
      XYBarChartView1.ChartTitle = ''
      XYBarChartView1.AxisTitle = ['', '', '', '']
      XYBarChartView1.LeftAxisRange = [-2.9880478382110596, 747.0119521617889]
      XYBarChartView1.RightAxisRange = [0.0, 6.66]
      
      DataRepresentation3 = Show()
      DataRepresentation3.XArrayName = 'bin_extents'
      DataRepresentation3.SeriesVisibility = ['bin_extents', '0', 'vtkOriginalIndices', '0', 'bin_values', '1']
      DataRepresentation3.AttributeType = 'Row Data'
      DataRepresentation3.UseIndexForXAxis = 0
      
    return Pipeline()

  class CoProcessor(coprocessing.CoProcessor):
    def CreatePipeline(self, datadescription):
      self.Pipeline = _CreatePipeline(self, datadescription)

  coprocessor = CoProcessor()
  freqs = {'input': [1]}
  coprocessor.SetUpdateFrequencies(freqs)
  return coprocessor

#--------------------------------------------------------------
# Global variables that will hold the pipeline for each timestep
# Creating the CoProcessor object, doesn't actually create the ParaView pipeline.
# It will be automatically setup when coprocessor.UpdateProducers() is called the
# first time.
coprocessor = CreateCoProcessor()

#--------------------------------------------------------------
# Enable Live-Visualizaton with ParaView
coprocessor.EnableLiveVisualization(False)


# ---------------------- Data Selection method ----------------------

def RequestDataDescription(datadescription):
    "Callback to populate the request for current timestep"
    global coprocessor
    if datadescription.GetForceOutput() == True:
        # We are just going to request all fields and meshes from the simulation
        # code/adaptor.
        for i in range(datadescription.GetNumberOfInputDescriptions()):
            datadescription.GetInputDescription(i).AllFieldsOn()
            datadescription.GetInputDescription(i).GenerateMeshOn()
        return

    # setup requests for all inputs based on the requirements of the
    # pipeline.
    coprocessor.LoadRequestedData(datadescription)

# ------------------------ Processing method ------------------------

def DoCoProcessing(datadescription):
    "Callback to do co-processing for current timestep"
    global coprocessor

    # Update the coprocessor by providing it the newly generated simulation data.
    # If the pipeline hasn't been setup yet, this will setup the pipeline.
    coprocessor.UpdateProducers(datadescription)

    # Write output data, if appropriate.
    coprocessor.WriteData(datadescription);

    # Write image capture (Last arg: rescale lookup table), if appropriate.
    coprocessor.WriteImages(datadescription, rescale_lookuptable=False)

    # Live Visualization, if enabled.
    coprocessor.DoLiveVisualization(datadescription, "localhost", 22222)
