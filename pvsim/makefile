MPICC=mpicc
MPICXX=mpic++
PV=$(HOME)/sw/paraview
PVINC:=$(PV)/include/paraview-4.1
PVLIB:=$(PV)/lib/paraview-4.1
WARN=-Wall -Wextra
CFLAGS=-std=c99 -fPIC $(WARN) -ggdb
CXXFLAGS=-fPIC $(WARN) -ggdb -I$(PVINC)
FC=gfortran
FFLAGS=$(WARN) -fPIC -ggdb
LDFLAGS:=-Wl,--no-undefined -L$(PVLIB) -Wl,-rpath,$(PVLIB)
PVLIBRARIES:=-lvtkPVCatalyst-pv4.1 -lvtkPVPythonCatalyst-pv4.1 \
  -lvtkCommonCore-pv4.1 -lvtksys-pv4.1 \
  -lvtkPVCatalyst-pv4.1 \
  -lvtkPVCatalystPython27D-pv4.1 \
  -lvtkPVClientServerCoreCore-pv4.1 \
  -lvtkPVClientServerCoreCorePython27D-pv4.1 \
  -lvtkPVClientServerCoreDefault-pv4.1 \
  -lvtkPVClientServerCoreDefaultPython27D-pv4.1 \
  -lvtkPVClientServerCoreRendering-pv4.1 \
  -lvtkPVClientServerCoreRenderingPython27D-pv4.1 \
  -lvtkPVCommon-pv4.1 \
  -lvtkPVCommonPython27D-pv4.1 \
  -lvtkPVPythonCatalyst-pv4.1 \
  -lvtkPVPythonCatalystPythonD-pv4.1 \
  -lvtkPVServerImplementationCore-pv4.1 \
  -lvtkPVServerImplementationCorePython27D-pv4.1 \
  -lvtkPVServerImplementationDefault-pv4.1 \
  -lvtkPVServerImplementationDefaultPython27D-pv4.1 \
  -lvtkPVServerImplementationRendering-pv4.1 \
  -lvtkPVServerImplementationRenderingPython27D-pv4.1 \
  -lvtkPVServerManagerApplication-pv4.1 \
  -lvtkPVServerManagerApplicationPython27D-pv4.1 \
  -lvtkPVServerManagerCore-pv4.1 \
  -lvtkPVServerManagerCorePython27D-pv4.1 \
  -lvtkPVServerManagerDefault-pv4.1 \
  -lvtkPVServerManagerDefaultPython27D-pv4.1 \
  -lvtkPVServerManagerRendering-pv4.1 \
  -lvtkPVServerManagerRenderingPython27D-pv4.1 \
  -lvtkPVVTKExtensionsCore-pv4.1 \
  -lvtkPVVTKExtensionsCorePython27D-pv4.1 \
  -lvtkPVVTKExtensionsDefault-pv4.1 \
  -lvtkPVVTKExtensionsDefaultPython27D-pv4.1 \
  -lvtkPVVTKExtensionsRendering-pv4.1 \
  -lvtkPVVTKExtensionsRenderingPython27D-pv4.1 \
  -lvtkalglib-pv4.1 \
  -lvtkChartsCore-pv4.1 \
  -lvtkChartsCorePython27D-pv4.1 \
  -lvtkClientServer-pv4.1 \
  -lvtkCommonColor-pv4.1 \
  -lvtkCommonColorPython27D-pv4.1 \
  -lvtkCommonComputationalGeometry-pv4.1 \
  -lvtkCommonComputationalGeometryPython27D-pv4.1 \
  -lvtkCommonCore-pv4.1 \
  -lvtkCommonCorePython27D-pv4.1 \
  -lvtkCommonDataModel-pv4.1 \
  -lvtkCommonDataModelPython27D-pv4.1 \
  -lvtkCommonExecutionModel-pv4.1 \
  -lvtkCommonExecutionModelPython27D-pv4.1 \
  -lvtkCommonMath-pv4.1 \
  -lvtkCommonMathPython27D-pv4.1 \
  -lvtkCommonMisc-pv4.1 \
  -lvtkCommonMiscPython27D-pv4.1 \
  -lvtkCommonSystem-pv4.1 \
  -lvtkCommonSystemPython27D-pv4.1 \
  -lvtkCommonTransforms-pv4.1 \
  -lvtkCommonTransformsPython27D-pv4.1 \
  -lvtkDICOMParser-pv4.1 \
  -lvtkDomainsChemistry-pv4.1 \
  -lvtkDomainsChemistryPython27D-pv4.1 \
  -lvtkexoIIc-pv4.1 \
  -lvtkexpat-pv4.1 \
  -lvtkEyeDomeLighting-pv4.1 \
  -lvtkFiltersAMR-pv4.1 \
  -lvtkFiltersAMRPython27D-pv4.1 \
  -lvtkFiltersCore-pv4.1 \
  -lvtkFiltersCorePython27D-pv4.1 \
  -lvtkFiltersExtraction-pv4.1 \
  -lvtkFiltersExtractionPython27D-pv4.1 \
  -lvtkFiltersFlowPaths-pv4.1 \
  -lvtkFiltersFlowPathsPython27D-pv4.1 \
  -lvtkFiltersGeneral-pv4.1 \
  -lvtkFiltersGeneralPython27D-pv4.1 \
  -lvtkFiltersGeneric-pv4.1 \
  -lvtkFiltersGenericPython27D-pv4.1 \
  -lvtkFiltersGeometry-pv4.1 \
  -lvtkFiltersGeometryPython27D-pv4.1 \
  -lvtkFiltersHybrid-pv4.1 \
  -lvtkFiltersHybridPython27D-pv4.1 \
  -lvtkFiltersHyperTree-pv4.1 \
  -lvtkFiltersHyperTreePython27D-pv4.1 \
  -lvtkFiltersModeling-pv4.1 \
  -lvtkFiltersModelingPython27D-pv4.1 \
  -lvtkFiltersParallel-pv4.1 \
  -lvtkFiltersParallelPython27D-pv4.1 \
  -lvtkFiltersParallelStatistics-pv4.1 \
  -lvtkFiltersParallelStatisticsPython27D-pv4.1 \
  -lvtkFiltersProgrammable-pv4.1 \
  -lvtkFiltersProgrammablePython27D-pv4.1 \
  -lvtkFiltersSources-pv4.1 \
  -lvtkFiltersSourcesPython27D-pv4.1 \
  -lvtkFiltersStatistics-pv4.1 \
  -lvtkFiltersStatisticsPython27D-pv4.1 \
  -lvtkFiltersTexture-pv4.1 \
  -lvtkFiltersTexturePython27D-pv4.1 \
  -lvtkFiltersVerdict-pv4.1 \
  -lvtkFiltersVerdictPython27D-pv4.1 \
  -lvtkfreetype-pv4.1 \
  -lvtkftgl-pv4.1 \
  -lvtkgl2ps-pv4.1 \
  -lvtkGUISupportQt-pv4.1 \
  -lvtkhdf5_hl-pv4.1 \
  -lvtkhdf5-pv4.1 \
  -lvtkImagingColor-pv4.1 \
  -lvtkImagingColorPython27D-pv4.1 \
  -lvtkImagingCore-pv4.1 \
  -lvtkImagingCorePython27D-pv4.1 \
  -lvtkImagingFourier-pv4.1 \
  -lvtkImagingFourierPython27D-pv4.1 \
  -lvtkImagingGeneral-pv4.1 \
  -lvtkImagingGeneralPython27D-pv4.1 \
  -lvtkImagingHybrid-pv4.1 \
  -lvtkImagingHybridPython27D-pv4.1 \
  -lvtkImagingMorphological-pv4.1 \
  -lvtkImagingMorphologicalPython27D-pv4.1 \
  -lvtkImagingSources-pv4.1 \
  -lvtkImagingSourcesPython27D-pv4.1 \
  -lvtkInfovisCore-pv4.1 \
  -lvtkInfovisCorePython27D-pv4.1 \
  -lvtkInteractionImage-pv4.1 \
  -lvtkInteractionImagePython27D-pv4.1 \
  -lvtkInteractionStyle-pv4.1 \
  -lvtkInteractionStylePython27D-pv4.1 \
  -lvtkInteractionWidgets-pv4.1 \
  -lvtkInteractionWidgetsPython27D-pv4.1 \
  -lvtkIOAMR-pv4.1 \
  -lvtkIOAMRPython27D-pv4.1 \
  -lvtkIOCore-pv4.1 \
  -lvtkIOCorePython27D-pv4.1 \
  -lvtkIOEnSight-pv4.1 \
  -lvtkIOEnSightPython27D-pv4.1 \
  -lvtkIOExodus-pv4.1 \
  -lvtkIOExodusPython27D-pv4.1 \
  -lvtkIOExport-pv4.1 \
  -lvtkIOExportPython27D-pv4.1 \
  -lvtkIOGeometry-pv4.1 \
  -lvtkIOGeometryPython27D-pv4.1 \
  -lvtkIOImage-pv4.1 \
  -lvtkIOImagePython27D-pv4.1 \
  -lvtkIOImport-pv4.1 \
  -lvtkIOImportPython27D-pv4.1 \
  -lvtkIOInfovis-pv4.1 \
  -lvtkIOInfovisPython27D-pv4.1 \
  -lvtkIOLegacy-pv4.1 \
  -lvtkIOLegacyPython27D-pv4.1 \
  -lvtkIOLSDyna-pv4.1 \
  -lvtkIOLSDynaPython27D-pv4.1 \
  -lvtkIOMovie-pv4.1 \
  -lvtkIOMoviePython27D-pv4.1 \
  -lvtkIONetCDF-pv4.1 \
  -lvtkIONetCDFPython27D-pv4.1 \
  -lvtkIOParallelExodus-pv4.1 \
  -lvtkIOParallelExodusPython27D-pv4.1 \
  -lvtkIOParallelLSDyna-pv4.1 \
  -lvtkIOParallelLSDynaPython27D-pv4.1 \
  -lvtkIOParallel-pv4.1 \
  -lvtkIOParallelPython27D-pv4.1 \
  -lvtkIOPLY-pv4.1 \
  -lvtkIOPLYPython27D-pv4.1 \
  -lvtkIOVPIC-pv4.1 \
  -lvtkIOVPICPython27D-pv4.1 \
  -lvtkIOXdmf2-pv4.1 \
  -lvtkIOXdmf2Python27D-pv4.1 \
  -lvtkIOXMLParser-pv4.1 \
  -lvtkIOXMLParserPython27D-pv4.1 \
  -lvtkIOXML-pv4.1 \
  -lvtkIOXMLPython27D-pv4.1 \
  -lvtkjpeg-pv4.1 \
  -lvtkjsoncpp-pv4.1 \
  -lvtklibxml2-pv4.1 \
  -lvtkmetaio-pv4.1 \
  -lvtkNetCDF_cxx-pv4.1 \
  -lvtkNetCDF-pv4.1 \
  -lvtkoggtheora-pv4.1 \
  -lvtkParallelCore-pv4.1 \
  -lvtkParallelCorePython27D-pv4.1 \
  -lvtkpng-pv4.1 \
  -lvtkPointSpriteGraphics-pv4.1 \
  -lvtkPointSpriteRendering-pv4.1 \
  -lvtkpqApplicationComponents-pv4.1 \
  -lvtkpqComponents-pv4.1 \
  -lvtkpqCore-pv4.1 \
  -lvtkpqPython-pv4.1 \
  -lvtkpqWidgets-pv4.1 \
  -lvtkPythonInterpreter-pv4.1 \
  -lvtkPythonInterpreterPython27D-pv4.1 \
  -lvtkQvis-pv4.1 \
  -lvtkRenderingAnnotation-pv4.1 \
  -lvtkRenderingAnnotationPython27D-pv4.1 \
  -lvtkRenderingContext2D-pv4.1 \
  -lvtkRenderingContext2DPython27D-pv4.1 \
  -lvtkRenderingCore-pv4.1 \
  -lvtkRenderingCorePython27D-pv4.1 \
  -lvtkRenderingFreeTypeOpenGL-pv4.1 \
  -lvtkRenderingFreeTypeOpenGLPython27D-pv4.1 \
  -lvtkRenderingFreeType-pv4.1 \
  -lvtkRenderingFreeTypePython27D-pv4.1 \
  -lvtkRenderingGL2PS-pv4.1 \
  -lvtkRenderingGL2PSPython27D-pv4.1 \
  -lvtkRenderingLabel-pv4.1 \
  -lvtkRenderingLabelPython27D-pv4.1 \
  -lvtkRenderingLIC-pv4.1 \
  -lvtkRenderingLICPython27D-pv4.1 \
  -lvtkRenderingLOD-pv4.1 \
  -lvtkRenderingLODPython27D-pv4.1 \
  -lvtkRenderingMatplotlib-pv4.1 \
  -lvtkRenderingMatplotlibPython27D-pv4.1 \
  -lvtkRenderingOpenGL-pv4.1 \
  -lvtkRenderingOpenGLPython27D-pv4.1 \
  -lvtkRenderingParallel-pv4.1 \
  -lvtkRenderingParallelPython27D-pv4.1 \
  -lvtkRenderingVolumeAMR-pv4.1 \
  -lvtkRenderingVolumeAMRPython27D-pv4.1 \
  -lvtkRenderingVolumeOpenGL-pv4.1 \
  -lvtkRenderingVolumeOpenGLPython27D-pv4.1 \
  -lvtkRenderingVolume-pv4.1 \
  -lvtkRenderingVolumePython27D-pv4.1 \
  -lvtkSciberQuest-pv4.1 \
  -lvtksys-pv4.1 \
  -lvtkTestingRendering-pv4.1 \
  -lvtkTestingRenderingPython27D-pv4.1 \
  -lvtktiff-pv4.1 \
  -lvtkUtilitiesPythonInitializer-pv4.1 \
  -lvtkverdict-pv4.1 \
  -lvtkViewsContext2D-pv4.1 \
  -lvtkViewsContext2DPython27D-pv4.1 \
  -lvtkViewsCore-pv4.1 \
  -lvtkViewsCorePython27D-pv4.1 \
  -lvtkVPIC-pv4.1 \
  -lvtkWebGLExporter-pv4.1 \
  -lvtkWebGLExporterPython27D-pv4.1 \
  -lvtkWrappingPython27Core-pv4.1 \
  -lvtkxdmf2-pv4.1 \
  -lvtkzlib-pv4.1 \
  -lprotobuf
LDLIBS=-ldl -lrt $(PVLIBRARIES)
obj=simple.mpi.o

all: $(obj) pvtest

pvtest: simple.mpi.o
	$(MPICXX) -ggdb -fPIC $^ -o $@ $(LDFLAGS) $(LDLIBS)

%.mpi.o: %.mpi.c
	$(MPICC) -c $(CFLAGS) $< -o $@
%.mpi.o: %.mpi.cc
	$(MPICXX) -c $(CXXFLAGS) $< -o $@

clean:
	rm -f $(obj) pvtest

%.o: %.f95
	$(FC) $(FFLAGS) -c $^ -o $@
