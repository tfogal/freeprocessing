#include <stdio.h>
#include <mpi.h>
#include <vtkCPDataDescription.h>
#include <vtkCPInputDataDescription.h>
#include <vtkCPProcessor.h>
#include <vtkCPPythonScriptPipeline.h>
#include <vtkDoubleArray.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkXMLImageDataWriter.h>
#include <vtkUniformGrid.h>

static void CatalystInit(int numScripts, const char* scripts[]);
static void CatalystFinalize();
static void coprocess(int ts, double time, const size_t dims[3],
                      const double spacing[3], double* field);
static void mkugrid(const size_t dims[3], const double* field);

/* how we talk to PView */
static vtkCPProcessor* processor = NULL;
static vtkUniformGrid* VTKGrid = NULL;

int main(int argc, char* argv[])
{
  printf("starting up..\n"); fflush(stdout);
  MPI_Init(&argc, &argv);
  printf("initialized mpi..\n"); fflush(stdout);
  //const char* scripts[] = {"pvpipeline.py"};
  const char* scripts[] = {"pvimage.py"};
  CatalystInit(1, scripts);
  printf("initialized catalyst..\n"); fflush(stdout);

  const size_t dims[3] = {16,16,16};
  const double spacing[3] = {1.0, 1.0, 1.0};
  double* data = (double*)calloc(dims[0]*dims[1]*dims[2], sizeof(double));
  for(size_t z=0; z < dims[2]; ++z) {
    for(size_t y=0; y < dims[1]; ++y) {
      for(size_t x=0; x < dims[0]; ++x) {
        data[z*dims[1]*dims[0] + y*dims[0] + x] = (double)z*4 + (double)y*2 + x;
      }
    }
  }
  double time = 0.0;
  mkugrid(dims, data);
  for(size_t ts=0; ts < 6000; ++ts) {
    coprocess(ts, time, dims, spacing, data);
    time += 0.36;
  }
  free(data);

  CatalystFinalize();
  MPI_Finalize();
  return 0;
}

static void CatalystInit(int numScripts, const char* scripts[])
{
  if(processor == NULL)
    {
    processor = vtkCPProcessor::New();
    processor->Initialize();
    }
  else
    {
    processor->RemoveAllPipelines();
    }
  for(int i=0;i<numScripts;i++)
    {
    vtkNew<vtkCPPythonScriptPipeline> pipeline;
    fprintf(stderr, "adding script '%s'\n", scripts[i]);
    pipeline->Initialize(scripts[i]);
    processor->AddPipeline(pipeline.GetPointer());
    }
}

static void CatalystFinalize()
{
  if(processor) {
    processor->RemoveAllPipelines();
    processor->Delete();
  }
  if(VTKGrid) {
    VTKGrid->Delete();
  }
  processor = NULL;
  VTKGrid = NULL;
}

static void
mkugrid(const size_t dims[3], const double* field)
{
    fprintf(stderr, "creating %zu %zu %zu grid\n", dims[0], dims[1], dims[2]);
    vtkUniformGrid* cart = vtkUniformGrid::New();
    cart->SetDimensions(dims[0], dims[1], dims[2]);
    vtkDoubleArray* arr = vtkDoubleArray::New();
    arr->SetName("needsaname");
    arr->SetArray((double*)field, cart->GetNumberOfPoints(), 1);
    cart->GetPointData()->AddArray(arr);
    arr->Delete();

    fprintf(stderr, "writing the grid...\n");
    vtkXMLImageDataWriter * sgw = vtkXMLImageDataWriter::New();
    sgw->SetInputData(cart);
    sgw->SetFileName("tmp.vti");
    sgw->Write();
    sgw->Delete();
    cart->Delete();
    fprintf(stderr, "write finished\n");
}


static void
coprocess(int ts, double time, const size_t dims[3],
          const double spacing[3], double* field)
{
  (void)spacing;
  vtkCPDataDescription* dataDesc = vtkCPDataDescription::New();
  dataDesc->AddInput("input");
  dataDesc->SetTimeData(time, ts);
  if(processor->RequestDataDescription(dataDesc) != 0) {
    fprintf(stderr, "doing actual work.\n");
    vtkUniformGrid* cart = vtkUniformGrid::New();
    cart->SetDimensions(dims[0], dims[1], dims[2]);
    dataDesc->GetInputDescriptionByName("input")->SetGrid(cart);
    cart->Delete();
    vtkDoubleArray* arr = vtkDoubleArray::New();
    arr->SetName("needsaname");
    arr->SetArray(field, cart->GetNumberOfPoints(), 1);
    cart->GetPointData()->AddArray(arr);
    arr->Delete();

    processor->CoProcess(dataDesc);
  }
  dataDesc->Delete();
}
