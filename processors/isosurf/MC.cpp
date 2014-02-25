#include <array>
#include <cstring>
#include <cinttypes>
#include <cerrno>
#include "debug.h"
#include "MC.h"
#include "Vectors.h"
typedef std::array<float,3> FLOATVECTOR3;

#define EPSILON 0.000001f
#define DATA_INDEX(I, J, K, IDIM, JDIM) ((I) + ((IDIM) * (J)) + ((IDIM * JDIM) * (K)))
#define EDGE_INDEX(E, I, J, IDIM) ((E) + (12 * (I)) + ((12 * (IDIM)) * (J)))
#define NO_EDGE -1

#define PURE __attribute__((pure))

DECLARE_CHANNEL(mcpp);

template <class T=float> class LayerTempData {
public:
  const T* botvol;
  const T* topvol;
  int* piEdges;  // tag indexing into vertex list

  LayerTempData<T>(const INTVECTOR3& vVolSize,
                   const T* bottom, const T* top);
  virtual ~LayerTempData();

private:
  INTVECTOR3 m_vVolSize;
};

class Isosurface {
public:
    std::vector<FLOATVECTOR3> vfVertices;
    std::vector<FLOATVECTOR3> vfNormals;
    std::vector<UINTVECTOR3> viTriangles;
    size_t iVertices;
    size_t iTriangles;

    Isosurface();
    Isosurface(int iMaxVertices, int iMaxTris);
    virtual ~Isosurface();

    int AddTriangle(unsigned a, unsigned b, unsigned c);
    int AddVertex(const FLOATVECTOR3& v, const FLOATVECTOR3& n);
    void AppendData(const Isosurface* other);

    void reset(); // resets to initial state, without deallocation.
};

Isosurface::Isosurface() :
  iVertices(0),
  iTriangles(0)
{}

Isosurface::Isosurface(int iMaxVertices, int iMaxTris) :
  iVertices(0),
  iTriangles(0)
{
  vfVertices.reserve(iMaxVertices);
  vfNormals.reserve(iMaxVertices);
  viTriangles.reserve(iMaxTris);
}

Isosurface::~Isosurface() {
}

int Isosurface::AddTriangle(unsigned a, unsigned b, unsigned c) {
  viTriangles.push_back(VECTOR3<unsigned>(a,b,c));
  iTriangles++;
  return iTriangles-1;
}

int Isosurface::AddVertex(const FLOATVECTOR3& v, const FLOATVECTOR3& n) {
  vfVertices.push_back(v);
  vfNormals.push_back(n);
  iVertices++;
  return iVertices-1;
}

void
Isosurface::reset()
{
  this->vfVertices.resize(0);
  this->vfNormals.resize(0);
  this->viTriangles.resize(0);
  this->iVertices = 0;
  this->iTriangles = 0;
}



// ****************************************************************************
// ALSO try *resetting* the Isosurface instead of deleting/re-allocing it every
// layer.  this should allow the vectors to amortize out the allocations much
// better.
// ****************************************************************************
// TRY buffering so that one layer is downloading while another is running MC.
// ****************************************************************************


void Isosurface::AppendData(const Isosurface* other) {
  // if verts in other, expand the storage this surface
  if (other->iVertices > 0) {
		// append data sensibly.
    vfVertices.insert(vfVertices.end(), other->vfVertices.cbegin(),
                      other->vfVertices.cend());
    vfNormals.insert(vfNormals.end(), other->vfNormals.cbegin(),
                     other->vfNormals.cend());
    viTriangles.insert(viTriangles.end(), other->viTriangles.cbegin(),
                       other->viTriangles.cend());
  }

  // update this list's counters
  iVertices  += other->iVertices;
  iTriangles += other->iTriangles;
}

template <class T=float> class MarchingCubes {
public:
    Isosurface* m_Isosurface;
    Isosurface* sliceIsosurface; // temp isosurf, for 1 layer.

    MarchingCubes<T>(void);
    virtual ~MarchingCubes<T>(void);

    virtual void SetVolume(int iSizeX, int iSizeY, int iSizeZ,
                           const T* s0, const T* s1);
    virtual void Process(T TIsoValue);
    void ResetIsosurf();
    uint64_t slice_number;

protected:
    INTVECTOR3 m_vVolSize;
    INTVECTOR3 m_vOffset;
    const T* slice[2]; /* 0 is bottom, 1 is top. */
    T m_TIsoValue;

    virtual void MarchLayer(LayerTempData<T> *layer);
    virtual int MakeVertex(int whichEdge, int i, int j, int k,
                           Isosurface* sliceIso);
};
/*
  these tables for computing the Marching Cubes algorithm
  are Paul Bourke, based on code by Cory Gene Bloyd.

  The indexing of vertices and edges in a cube are defined
  as:
                _4____________4_____________5
               /|                           /
              / |                          /|
             /  |                         / |
            7   |                        /  |
           /    |                       /5  |
          /     |                      /    |
         /      8                     /     9
        /       |                    /      |
      7/________|______6____________/6      |
       |        |                   |       |
       |        |                   |       |
       |        |                   |       |
       |        |0____________0_____|_______|1
      11       /                    |      /
       |      /                    10     /
       |     /                      |    /
       |    /3                      |   /1
       |   /                        |  /
       |  /                         | /
       | /                          |/
       |/3____________2_____________|2

For purposes of calculating vertices along the edges and the
triangulations created, there are 15 distinct cases, with
upper limits of
  12 edge intersections
  5 triangles created per cell
*/

static int16_t edgeTable[256] = {
  0x0  , 0x109, 0x203, 0x30a, 0x406, 0x50f, 0x605, 0x70c,
  0x80c, 0x905, 0xa0f, 0xb06, 0xc0a, 0xd03, 0xe09, 0xf00,
  0x190, 0x99 , 0x393, 0x29a, 0x596, 0x49f, 0x795, 0x69c,
  0x99c, 0x895, 0xb9f, 0xa96, 0xd9a, 0xc93, 0xf99, 0xe90,
  0x230, 0x339, 0x33 , 0x13a, 0x636, 0x73f, 0x435, 0x53c,
  0xa3c, 0xb35, 0x83f, 0x936, 0xe3a, 0xf33, 0xc39, 0xd30,
  0x3a0, 0x2a9, 0x1a3, 0xaa , 0x7a6, 0x6af, 0x5a5, 0x4ac,
  0xbac, 0xaa5, 0x9af, 0x8a6, 0xfaa, 0xea3, 0xda9, 0xca0,
  0x460, 0x569, 0x663, 0x76a, 0x66 , 0x16f, 0x265, 0x36c,
  0xc6c, 0xd65, 0xe6f, 0xf66, 0x86a, 0x963, 0xa69, 0xb60,
  0x5f0, 0x4f9, 0x7f3, 0x6fa, 0x1f6, 0xff , 0x3f5, 0x2fc,
  0xdfc, 0xcf5, 0xfff, 0xef6, 0x9fa, 0x8f3, 0xbf9, 0xaf0,
  0x650, 0x759, 0x453, 0x55a, 0x256, 0x35f, 0x55 , 0x15c,
  0xe5c, 0xf55, 0xc5f, 0xd56, 0xa5a, 0xb53, 0x859, 0x950,
  0x7c0, 0x6c9, 0x5c3, 0x4ca, 0x3c6, 0x2cf, 0x1c5, 0xcc ,
  0xfcc, 0xec5, 0xdcf, 0xcc6, 0xbca, 0xac3, 0x9c9, 0x8c0,
  0x8c0, 0x9c9, 0xac3, 0xbca, 0xcc6, 0xdcf, 0xec5, 0xfcc,
  0xcc , 0x1c5, 0x2cf, 0x3c6, 0x4ca, 0x5c3, 0x6c9, 0x7c0,
  0x950, 0x859, 0xb53, 0xa5a, 0xd56, 0xc5f, 0xf55, 0xe5c,
  0x15c, 0x55 , 0x35f, 0x256, 0x55a, 0x453, 0x759, 0x650,
  0xaf0, 0xbf9, 0x8f3, 0x9fa, 0xef6, 0xfff, 0xcf5, 0xdfc,
  0x2fc, 0x3f5, 0xff , 0x1f6, 0x6fa, 0x7f3, 0x4f9, 0x5f0,
  0xb60, 0xa69, 0x963, 0x86a, 0xf66, 0xe6f, 0xd65, 0xc6c,
  0x36c, 0x265, 0x16f, 0x66 , 0x76a, 0x663, 0x569, 0x460,
  0xca0, 0xda9, 0xea3, 0xfaa, 0x8a6, 0x9af, 0xaa5, 0xbac,
  0x4ac, 0x5a5, 0x6af, 0x7a6, 0xaa , 0x1a3, 0x2a9, 0x3a0,
  0xd30, 0xc39, 0xf33, 0xe3a, 0x936, 0x83f, 0xb35, 0xa3c,
  0x53c, 0x435, 0x73f, 0x636, 0x13a, 0x33 , 0x339, 0x230,
  0xe90, 0xf99, 0xc93, 0xd9a, 0xa96, 0xb9f, 0x895, 0x99c,
  0x69c, 0x795, 0x49f, 0x596, 0x29a, 0x393, 0x99 , 0x190,
  0xf00, 0xe09, 0xd03, 0xc0a, 0xb06, 0xa0f, 0x905, 0x80c,
  0x70c, 0x605, 0x50f, 0x406, 0x30a, 0x203, 0x109, 0x0
};

/* this could actually be an int8_t, but those are annoying since they map to
 * chars. */
static int16_t triTable[256][16] = {
{NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {0, 8, 3, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {0, 1, 9, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {1, 8, 3, 9, 8, 1, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {1, 2, 10, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {0, 8, 3, 1, 2, 10, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {9, 2, 10, 0, 2, 9, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {2, 8, 3, 2, 10, 8, 10, 9, 8, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {3, 11, 2, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {0, 11, 2, 8, 11, 0, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {1, 9, 0, 2, 3, 11, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {1, 11, 2, 1, 9, 11, 9, 8, 11, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {3, 10, 1, 11, 10, 3, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {0, 10, 1, 0, 8, 10, 8, 11, 10, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {3, 9, 0, 3, 11, 9, 11, 10, 9, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {9, 8, 10, 10, 8, 11, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {4, 7, 8, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {4, 3, 0, 7, 3, 4, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {0, 1, 9, 8, 4, 7, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {4, 1, 9, 4, 7, 1, 7, 3, 1, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {1, 2, 10, 8, 4, 7, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {3, 4, 7, 3, 0, 4, 1, 2, 10, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {9, 2, 10, 9, 0, 2, 8, 4, 7, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {2, 10, 9, 2, 9, 7, 2, 7, 3, 7, 9, 4, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {8, 4, 7, 3, 11, 2, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {11, 4, 7, 11, 2, 4, 2, 0, 4, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {9, 0, 1, 8, 4, 7, 2, 3, 11, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {4, 7, 11, 9, 4, 11, 9, 11, 2, 9, 2, 1, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {3, 10, 1, 3, 11, 10, 7, 8, 4, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {1, 11, 10, 1, 4, 11, 1, 0, 4, 7, 11, 4, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {4, 7, 8, 9, 0, 11, 9, 11, 10, 11, 0, 3, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {4, 7, 11, 4, 11, 9, 9, 11, 10, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {9, 5, 4, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {9, 5, 4, 0, 8, 3, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {0, 5, 4, 1, 5, 0, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {8, 5, 4, 8, 3, 5, 3, 1, 5, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {1, 2, 10, 9, 5, 4, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {3, 0, 8, 1, 2, 10, 4, 9, 5, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {5, 2, 10, 5, 4, 2, 4, 0, 2, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {2, 10, 5, 3, 2, 5, 3, 5, 4, 3, 4, 8, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {9, 5, 4, 2, 3, 11, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {0, 11, 2, 0, 8, 11, 4, 9, 5, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {0, 5, 4, 0, 1, 5, 2, 3, 11, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {2, 1, 5, 2, 5, 8, 2, 8, 11, 4, 8, 5, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {10, 3, 11, 10, 1, 3, 9, 5, 4, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {4, 9, 5, 0, 8, 1, 8, 10, 1, 8, 11, 10, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {5, 4, 0, 5, 0, 11, 5, 11, 10, 11, 0, 3, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {5, 4, 8, 5, 8, 10, 10, 8, 11, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {9, 7, 8, 5, 7, 9, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {9, 3, 0, 9, 5, 3, 5, 7, 3, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {0, 7, 8, 0, 1, 7, 1, 5, 7, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {1, 5, 3, 3, 5, 7, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {9, 7, 8, 9, 5, 7, 10, 1, 2, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {10, 1, 2, 9, 5, 0, 5, 3, 0, 5, 7, 3, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {8, 0, 2, 8, 2, 5, 8, 5, 7, 10, 5, 2, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {2, 10, 5, 2, 5, 3, 3, 5, 7, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {7, 9, 5, 7, 8, 9, 3, 11, 2, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {9, 5, 7, 9, 7, 2, 9, 2, 0, 2, 7, 11, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {2, 3, 11, 0, 1, 8, 1, 7, 8, 1, 5, 7, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {11, 2, 1, 11, 1, 7, 7, 1, 5, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {9, 5, 8, 8, 5, 7, 10, 1, 3, 10, 3, 11, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {5, 7, 0, 5, 0, 9, 7, 11, 0, 1, 0, 10, 11, 10, 0, NO_EDGE},
 {11, 10, 0, 11, 0, 3, 10, 5, 0, 8, 0, 7, 5, 7, 0, NO_EDGE},
 {11, 10, 5, 7, 11, 5, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {10, 6, 5, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {0, 8, 3, 5, 10, 6, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {9, 0, 1, 5, 10, 6, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {1, 8, 3, 1, 9, 8, 5, 10, 6, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {1, 6, 5, 2, 6, 1, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {1, 6, 5, 1, 2, 6, 3, 0, 8, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {9, 6, 5, 9, 0, 6, 0, 2, 6, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {5, 9, 8, 5, 8, 2, 5, 2, 6, 3, 2, 8, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {2, 3, 11, 10, 6, 5, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {11, 0, 8, 11, 2, 0, 10, 6, 5, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {0, 1, 9, 2, 3, 11, 5, 10, 6, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {5, 10, 6, 1, 9, 2, 9, 11, 2, 9, 8, 11, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {6, 3, 11, 6, 5, 3, 5, 1, 3, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {0, 8, 11, 0, 11, 5, 0, 5, 1, 5, 11, 6, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {3, 11, 6, 0, 3, 6, 0, 6, 5, 0, 5, 9, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {6, 5, 9, 6, 9, 11, 11, 9, 8, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {5, 10, 6, 4, 7, 8, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {4, 3, 0, 4, 7, 3, 6, 5, 10, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {1, 9, 0, 5, 10, 6, 8, 4, 7, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {10, 6, 5, 1, 9, 7, 1, 7, 3, 7, 9, 4, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {6, 1, 2, 6, 5, 1, 4, 7, 8, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {1, 2, 5, 5, 2, 6, 3, 0, 4, 3, 4, 7, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {8, 4, 7, 9, 0, 5, 0, 6, 5, 0, 2, 6, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {7, 3, 9, 7, 9, 4, 3, 2, 9, 5, 9, 6, 2, 6, 9, NO_EDGE},
 {3, 11, 2, 7, 8, 4, 10, 6, 5, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {5, 10, 6, 4, 7, 2, 4, 2, 0, 2, 7, 11, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {0, 1, 9, 4, 7, 8, 2, 3, 11, 5, 10, 6, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {9, 2, 1, 9, 11, 2, 9, 4, 11, 7, 11, 4, 5, 10, 6, NO_EDGE},
 {8, 4, 7, 3, 11, 5, 3, 5, 1, 5, 11, 6, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {5, 1, 11, 5, 11, 6, 1, 0, 11, 7, 11, 4, 0, 4, 11, NO_EDGE},
 {0, 5, 9, 0, 6, 5, 0, 3, 6, 11, 6, 3, 8, 4, 7, NO_EDGE},
 {6, 5, 9, 6, 9, 11, 4, 7, 9, 7, 11, 9, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {10, 4, 9, 6, 4, 10, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {4, 10, 6, 4, 9, 10, 0, 8, 3, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {10, 0, 1, 10, 6, 0, 6, 4, 0, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {8, 3, 1, 8, 1, 6, 8, 6, 4, 6, 1, 10, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {1, 4, 9, 1, 2, 4, 2, 6, 4, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {3, 0, 8, 1, 2, 9, 2, 4, 9, 2, 6, 4, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {0, 2, 4, 4, 2, 6, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {8, 3, 2, 8, 2, 4, 4, 2, 6, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {10, 4, 9, 10, 6, 4, 11, 2, 3, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {0, 8, 2, 2, 8, 11, 4, 9, 10, 4, 10, 6, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {3, 11, 2, 0, 1, 6, 0, 6, 4, 6, 1, 10, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {6, 4, 1, 6, 1, 10, 4, 8, 1, 2, 1, 11, 8, 11, 1, NO_EDGE},
 {9, 6, 4, 9, 3, 6, 9, 1, 3, 11, 6, 3, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {8, 11, 1, 8, 1, 0, 11, 6, 1, 9, 1, 4, 6, 4, 1, NO_EDGE},
 {3, 11, 6, 3, 6, 0, 0, 6, 4, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {6, 4, 8, 11, 6, 8, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {7, 10, 6, 7, 8, 10, 8, 9, 10, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {0, 7, 3, 0, 10, 7, 0, 9, 10, 6, 7, 10, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {10, 6, 7, 1, 10, 7, 1, 7, 8, 1, 8, 0, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {10, 6, 7, 10, 7, 1, 1, 7, 3, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {1, 2, 6, 1, 6, 8, 1, 8, 9, 8, 6, 7, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {2, 6, 9, 2, 9, 1, 6, 7, 9, 0, 9, 3, 7, 3, 9, NO_EDGE},
 {7, 8, 0, 7, 0, 6, 6, 0, 2, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {7, 3, 2, 6, 7, 2, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {2, 3, 11, 10, 6, 8, 10, 8, 9, 8, 6, 7, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {2, 0, 7, 2, 7, 11, 0, 9, 7, 6, 7, 10, 9, 10, 7, NO_EDGE},
 {1, 8, 0, 1, 7, 8, 1, 10, 7, 6, 7, 10, 2, 3, 11, NO_EDGE},
 {11, 2, 1, 11, 1, 7, 10, 6, 1, 6, 7, 1, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {8, 9, 6, 8, 6, 7, 9, 1, 6, 11, 6, 3, 1, 3, 6, NO_EDGE},
 {0, 9, 1, 11, 6, 7, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {7, 8, 0, 7, 0, 6, 3, 11, 0, 11, 6, 0, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {7, 11, 6, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {7, 6, 11, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {3, 0, 8, 11, 7, 6, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {0, 1, 9, 11, 7, 6, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {8, 1, 9, 8, 3, 1, 11, 7, 6, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {10, 1, 2, 6, 11, 7, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {1, 2, 10, 3, 0, 8, 6, 11, 7, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {2, 9, 0, 2, 10, 9, 6, 11, 7, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {6, 11, 7, 2, 10, 3, 10, 8, 3, 10, 9, 8, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {7, 2, 3, 6, 2, 7, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {7, 0, 8, 7, 6, 0, 6, 2, 0, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {2, 7, 6, 2, 3, 7, 0, 1, 9, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {1, 6, 2, 1, 8, 6, 1, 9, 8, 8, 7, 6, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {10, 7, 6, 10, 1, 7, 1, 3, 7, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {10, 7, 6, 1, 7, 10, 1, 8, 7, 1, 0, 8, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {0, 3, 7, 0, 7, 10, 0, 10, 9, 6, 10, 7, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {7, 6, 10, 7, 10, 8, 8, 10, 9, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {6, 8, 4, 11, 8, 6, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {3, 6, 11, 3, 0, 6, 0, 4, 6, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {8, 6, 11, 8, 4, 6, 9, 0, 1, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {9, 4, 6, 9, 6, 3, 9, 3, 1, 11, 3, 6, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {6, 8, 4, 6, 11, 8, 2, 10, 1, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {1, 2, 10, 3, 0, 11, 0, 6, 11, 0, 4, 6, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {4, 11, 8, 4, 6, 11, 0, 2, 9, 2, 10, 9, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {10, 9, 3, 10, 3, 2, 9, 4, 3, 11, 3, 6, 4, 6, 3, NO_EDGE},
 {8, 2, 3, 8, 4, 2, 4, 6, 2, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {0, 4, 2, 4, 6, 2, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {1, 9, 0, 2, 3, 4, 2, 4, 6, 4, 3, 8, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {1, 9, 4, 1, 4, 2, 2, 4, 6, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {8, 1, 3, 8, 6, 1, 8, 4, 6, 6, 10, 1, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {10, 1, 0, 10, 0, 6, 6, 0, 4, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {4, 6, 3, 4, 3, 8, 6, 10, 3, 0, 3, 9, 10, 9, 3, NO_EDGE},
 {10, 9, 4, 6, 10, 4, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {4, 9, 5, 7, 6, 11, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {0, 8, 3, 4, 9, 5, 11, 7, 6, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {5, 0, 1, 5, 4, 0, 7, 6, 11, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {11, 7, 6, 8, 3, 4, 3, 5, 4, 3, 1, 5, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {9, 5, 4, 10, 1, 2, 7, 6, 11, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {6, 11, 7, 1, 2, 10, 0, 8, 3, 4, 9, 5, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {7, 6, 11, 5, 4, 10, 4, 2, 10, 4, 0, 2, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {3, 4, 8, 3, 5, 4, 3, 2, 5, 10, 5, 2, 11, 7, 6, NO_EDGE},
 {7, 2, 3, 7, 6, 2, 5, 4, 9, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {9, 5, 4, 0, 8, 6, 0, 6, 2, 6, 8, 7, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {3, 6, 2, 3, 7, 6, 1, 5, 0, 5, 4, 0, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {6, 2, 8, 6, 8, 7, 2, 1, 8, 4, 8, 5, 1, 5, 8, NO_EDGE},
 {9, 5, 4, 10, 1, 6, 1, 7, 6, 1, 3, 7, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {1, 6, 10, 1, 7, 6, 1, 0, 7, 8, 7, 0, 9, 5, 4, NO_EDGE},
 {4, 0, 10, 4, 10, 5, 0, 3, 10, 6, 10, 7, 3, 7, 10, NO_EDGE},
 {7, 6, 10, 7, 10, 8, 5, 4, 10, 4, 8, 10, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {6, 9, 5, 6, 11, 9, 11, 8, 9, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {3, 6, 11, 0, 6, 3, 0, 5, 6, 0, 9, 5, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {0, 11, 8, 0, 5, 11, 0, 1, 5, 5, 6, 11, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {6, 11, 3, 6, 3, 5, 5, 3, 1, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {1, 2, 10, 9, 5, 11, 9, 11, 8, 11, 5, 6, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {0, 11, 3, 0, 6, 11, 0, 9, 6, 5, 6, 9, 1, 2, 10, NO_EDGE},
 {11, 8, 5, 11, 5, 6, 8, 0, 5, 10, 5, 2, 0, 2, 5, NO_EDGE},
 {6, 11, 3, 6, 3, 5, 2, 10, 3, 10, 5, 3, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {5, 8, 9, 5, 2, 8, 5, 6, 2, 3, 8, 2, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {9, 5, 6, 9, 6, 0, 0, 6, 2, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {1, 5, 8, 1, 8, 0, 5, 6, 8, 3, 8, 2, 6, 2, 8, NO_EDGE},
 {1, 5, 6, 2, 1, 6, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {1, 3, 6, 1, 6, 10, 3, 8, 6, 5, 6, 9, 8, 9, 6, NO_EDGE},
 {10, 1, 0, 10, 0, 6, 9, 5, 0, 5, 6, 0, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {0, 3, 8, 5, 6, 10, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {10, 5, 6, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {11, 5, 10, 7, 5, 11, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {11, 5, 10, 11, 7, 5, 8, 3, 0, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {5, 11, 7, 5, 10, 11, 1, 9, 0, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {10, 7, 5, 10, 11, 7, 9, 8, 1, 8, 3, 1, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {11, 1, 2, 11, 7, 1, 7, 5, 1, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {0, 8, 3, 1, 2, 7, 1, 7, 5, 7, 2, 11, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {9, 7, 5, 9, 2, 7, 9, 0, 2, 2, 11, 7, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {7, 5, 2, 7, 2, 11, 5, 9, 2, 3, 2, 8, 9, 8, 2, NO_EDGE},
 {2, 5, 10, 2, 3, 5, 3, 7, 5, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {8, 2, 0, 8, 5, 2, 8, 7, 5, 10, 2, 5, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {9, 0, 1, 5, 10, 3, 5, 3, 7, 3, 10, 2, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {9, 8, 2, 9, 2, 1, 8, 7, 2, 10, 2, 5, 7, 5, 2, NO_EDGE},
 {1, 3, 5, 3, 7, 5, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {0, 8, 7, 0, 7, 1, 1, 7, 5, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {9, 0, 3, 9, 3, 5, 5, 3, 7, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {9, 8, 7, 5, 9, 7, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {5, 8, 4, 5, 10, 8, 10, 11, 8, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {5, 0, 4, 5, 11, 0, 5, 10, 11, 11, 3, 0, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {0, 1, 9, 8, 4, 10, 8, 10, 11, 10, 4, 5, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {10, 11, 4, 10, 4, 5, 11, 3, 4, 9, 4, 1, 3, 1, 4, NO_EDGE},
 {2, 5, 1, 2, 8, 5, 2, 11, 8, 4, 5, 8, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {0, 4, 11, 0, 11, 3, 4, 5, 11, 2, 11, 1, 5, 1, 11, NO_EDGE},
 {0, 2, 5, 0, 5, 9, 2, 11, 5, 4, 5, 8, 11, 8, 5, NO_EDGE},
 {9, 4, 5, 2, 11, 3, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {2, 5, 10, 3, 5, 2, 3, 4, 5, 3, 8, 4, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {5, 10, 2, 5, 2, 4, 4, 2, 0, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {3, 10, 2, 3, 5, 10, 3, 8, 5, 4, 5, 8, 0, 1, 9, NO_EDGE},
 {5, 10, 2, 5, 2, 4, 1, 9, 2, 9, 4, 2, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {8, 4, 5, 8, 5, 3, 3, 5, 1, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {0, 4, 5, 1, 0, 5, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {8, 4, 5, 8, 5, 3, 9, 0, 5, 0, 3, 5, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {9, 4, 5, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {4, 11, 7, 4, 9, 11, 9, 10, 11, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {0, 8, 3, 4, 9, 7, 9, 11, 7, 9, 10, 11, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {1, 10, 11, 1, 11, 4, 1, 4, 0, 7, 4, 11, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {3, 1, 4, 3, 4, 8, 1, 10, 4, 7, 4, 11, 10, 11, 4, NO_EDGE},
 {4, 11, 7, 9, 11, 4, 9, 2, 11, 9, 1, 2, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {9, 7, 4, 9, 11, 7, 9, 1, 11, 2, 11, 1, 0, 8, 3, NO_EDGE},
 {11, 7, 4, 11, 4, 2, 2, 4, 0, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {11, 7, 4, 11, 4, 2, 8, 3, 4, 3, 2, 4, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {2, 9, 10, 2, 7, 9, 2, 3, 7, 7, 4, 9, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {9, 10, 7, 9, 7, 4, 10, 2, 7, 8, 7, 0, 2, 0, 7, NO_EDGE},
 {3, 7, 10, 3, 10, 2, 7, 4, 10, 1, 10, 0, 4, 0, 10, NO_EDGE},
 {1, 10, 2, 8, 7, 4, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {4, 9, 1, 4, 1, 7, 7, 1, 3, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {4, 9, 1, 4, 1, 7, 0, 8, 1, 8, 7, 1, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {4, 0, 3, 7, 4, 3, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {4, 8, 7, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {9, 10, 8, 10, 11, 8, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {3, 0, 9, 3, 9, 11, 11, 9, 10, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {0, 1, 10, 0, 10, 8, 8, 10, 11, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {3, 1, 10, 11, 3, 10, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {1, 2, 11, 1, 11, 9, 9, 11, 8, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {3, 0, 9, 3, 9, 11, 1, 2, 9, 2, 11, 9, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {0, 2, 11, 8, 0, 11, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {3, 2, 11, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {2, 3, 8, 2, 8, 10, 10, 8, 9, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {9, 10, 2, 0, 9, 2, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {2, 3, 8, 2, 8, 10, 0, 1, 8, 1, 10, 8, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {1, 10, 2, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {1, 3, 8, 9, 1, 8, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {0, 9, 1, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {0, 3, 8, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE},
 {NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE, NO_EDGE}
};

template <class T> MarchingCubes<T>::MarchingCubes(void) :
  m_Isosurface(NULL),
  sliceIsosurface(NULL)
{
  m_vVolSize    = INTVECTOR3(0,0,0);
  slice[0] = slice[1] = NULL;
  m_Isosurface  = NULL;
  this->slice_number = 0;
  this->m_Isosurface = new Isosurface();
}

template <class T> MarchingCubes<T>::~MarchingCubes(void)
{
  delete this->m_Isosurface;
  delete this->sliceIsosurface;
  this->m_Isosurface = NULL;
  this->sliceIsosurface = NULL;
}

template <class T> void
MarchingCubes<T>::SetVolume(int iSizeX, int iSizeY, int iSizeZ,
                            const T* slice0, const T* slice1)
{
  this->slice[0] = slice0;
  this->slice[1] = slice1;
  m_vVolSize  = INTVECTOR3(iSizeX, iSizeY, iSizeZ);
  m_TIsoValue = 0;
  if(NULL == this->sliceIsosurface) {
		// local part of the isosurface with at most 12 vertices and at most 5 triangles per cell
		this->sliceIsosurface = new Isosurface((m_vVolSize.x-1) * (m_vVolSize.y-1) * 12, (m_vVolSize.x-1) * (m_vVolSize.y-1) * 5);
  }
}

template <class T> void MarchingCubes<T>::Process(T TIsoValue)
{
  // store isovalue
  m_TIsoValue = TIsoValue;

  // init isosurface data
#if 0
  delete m_Isosurface;
  m_Isosurface = new Isosurface();
#else
  m_Isosurface->reset();
#endif

  // if the volume is empty we are done
  if (m_vVolSize.volume() == 0) return;

  // create a new layer - dataset
  LayerTempData<T>* layerData =
    new LayerTempData<T>(m_vVolSize, slice[0], slice[1]);

  // march the first layer
  MarchLayer(layerData);

  // delete the layer dataset
  delete layerData;
}

template<typename T> void
MarchingCubes<T>::ResetIsosurf()
{
  delete m_Isosurface; m_Isosurface = NULL;
}

#define iLayer 0
template <class T> void
MarchingCubes<T>::MarchLayer(LayerTempData<T>* layer) {
  int cellVerts[12];  // the 12 possible vertices in a cell
  std::fill(cellVerts, cellVerts+12, NO_EDGE);
  this->sliceIsosurface->reset();

  // march all cells in the layer
  const size_t voxels[2] = { (size_t)m_vVolSize.x-1, (size_t)m_vVolSize.y-1 };
  for(size_t j = 0; j < voxels[1]; j++) {
    for(size_t i = 0; i < voxels[0]; i++) {

      // fetch data from the volume
      T fVolumeValues[8];
      fVolumeValues[0] = (layer->botvol[(j+1)  * m_vVolSize.x + i]);
      fVolumeValues[1] = (layer->botvol[(j+1)  * m_vVolSize.x + i+1]);
      fVolumeValues[2] = (layer->botvol[j      * m_vVolSize.x + i+1]);
      fVolumeValues[3] = (layer->botvol[j      * m_vVolSize.x + i]);
      fVolumeValues[4] = (layer->topvol[(j+1)  * m_vVolSize.x + i]);
      fVolumeValues[5] = (layer->topvol[(j+1)  * m_vVolSize.x + i+1]);
      fVolumeValues[6] = (layer->topvol[j      * m_vVolSize.x + i+1]);
      fVolumeValues[7] = (layer->topvol[j      * m_vVolSize.x + i]);

      // compute the index for the table lookup
      int cellIndex = 1*int(fVolumeValues[0] < m_TIsoValue)+
                      2*int(fVolumeValues[1] < m_TIsoValue)+
                      4*int(fVolumeValues[2] < m_TIsoValue)+
                      8*int(fVolumeValues[3] < m_TIsoValue)+
                     16*int(fVolumeValues[4] < m_TIsoValue)+
                     32*int(fVolumeValues[5] < m_TIsoValue)+
                     64*int(fVolumeValues[6] < m_TIsoValue)+
                    128*int(fVolumeValues[7] < m_TIsoValue);

      // get the coordinates for the vertices, compute the
      // triangulation and interpolate the normals
      if (edgeTable[cellIndex] &    1) {
        if (layer->piEdges[EDGE_INDEX(0, i, j, m_vVolSize.x-1)] == NO_EDGE) {
          cellVerts[0]  = m_Isosurface->iVertices + MakeVertex(0, i, j, iLayer, sliceIsosurface);
        } else {
          cellVerts[0] = layer->piEdges[EDGE_INDEX(0, i, j, m_vVolSize.x-1)];
        }
      }
      if (edgeTable[cellIndex] &    2) {
        if (layer->piEdges[EDGE_INDEX(1, i, j, m_vVolSize.x-1)] == NO_EDGE) {
          cellVerts[1]  = m_Isosurface->iVertices+MakeVertex(1, i, j, iLayer, sliceIsosurface);
        } else {
          cellVerts[1] = layer->piEdges[EDGE_INDEX(1, i, j, m_vVolSize.x-1)];
        }
      }
      if (edgeTable[cellIndex] &    4) {
        if (layer->piEdges[EDGE_INDEX(2, i, j, m_vVolSize.x-1)] == NO_EDGE) {
          cellVerts[2]  = m_Isosurface->iVertices +MakeVertex(2, i, j, iLayer, sliceIsosurface);
        } else {
          cellVerts[2] = layer->piEdges[EDGE_INDEX(2, i, j, m_vVolSize.x-1)];
        }
      }
      if (edgeTable[cellIndex] &    8) {
        if (layer->piEdges[EDGE_INDEX(3, i, j, m_vVolSize.x-1)] == NO_EDGE) {
          cellVerts[3]  = m_Isosurface->iVertices+MakeVertex(3, i, j, iLayer, sliceIsosurface);
        } else {
          cellVerts[3] = layer->piEdges[EDGE_INDEX(3, i, j, m_vVolSize.x-1)];
        }
      }
      if (edgeTable[cellIndex] &    16) {
        if (layer->piEdges[EDGE_INDEX(4, i, j, m_vVolSize.x-1)] == NO_EDGE) {
          cellVerts[4]  = m_Isosurface->iVertices+MakeVertex(4, i, j, iLayer, sliceIsosurface);
        } else {
          cellVerts[4] = layer->piEdges[EDGE_INDEX(4, i, j, m_vVolSize.x-1)];
        }
      }
      if (edgeTable[cellIndex] &    32) {
        if (layer->piEdges[EDGE_INDEX(5, i, j, m_vVolSize.x-1)] == NO_EDGE) {
          cellVerts[5]  = m_Isosurface->iVertices+MakeVertex(5, i, j, iLayer, sliceIsosurface);
        } else {
          cellVerts[5] = layer->piEdges[EDGE_INDEX(5, i, j, m_vVolSize.x-1)];
        }
      }
      if (edgeTable[cellIndex] &    64) {
        if (layer->piEdges[EDGE_INDEX(6, i, j, m_vVolSize.x-1)] == NO_EDGE) {
          cellVerts[6]  = m_Isosurface->iVertices +MakeVertex(6, i, j, iLayer, sliceIsosurface);
        } else {
          cellVerts[6] = layer->piEdges[EDGE_INDEX(6, i, j, m_vVolSize.x-1)];
        }
      }
      if (edgeTable[cellIndex] &    128) {
        if (layer->piEdges[EDGE_INDEX(7, i, j, m_vVolSize.x-1)] == NO_EDGE) {
          cellVerts[7]  = m_Isosurface->iVertices +MakeVertex(7, i, j, iLayer, sliceIsosurface);
        } else {
          cellVerts[7] = layer->piEdges[EDGE_INDEX(7, i, j, m_vVolSize.x-1)];
        }
      }
      if (edgeTable[cellIndex] &    256) {
        if (layer->piEdges[EDGE_INDEX(8, i, j, m_vVolSize.x-1)] == NO_EDGE) {
          cellVerts[8]  = m_Isosurface->iVertices +MakeVertex(8, i, j, iLayer, sliceIsosurface);
        } else {
          cellVerts[8] = layer->piEdges[EDGE_INDEX(8, i, j, m_vVolSize.x-1)];
        }
      }
      if (edgeTable[cellIndex] &    512) {
        if (layer->piEdges[EDGE_INDEX(9, i, j, m_vVolSize.x-1)] == NO_EDGE) {
          cellVerts[9]  = m_Isosurface->iVertices +MakeVertex(9, i, j, iLayer, sliceIsosurface);
        } else {
        cellVerts[9] = layer->piEdges[EDGE_INDEX(9, i, j, m_vVolSize.x-1)];
        }
      }
      if (edgeTable[cellIndex] &    1024) {
        if (layer->piEdges[EDGE_INDEX(10, i, j, m_vVolSize.x-1)] == NO_EDGE) {
          cellVerts[10]  = m_Isosurface->iVertices +MakeVertex(10, i, j, iLayer, sliceIsosurface);
        } else {
          cellVerts[10] = layer->piEdges[EDGE_INDEX(10, i, j, m_vVolSize.x-1)];
        }
      }
      if (edgeTable[cellIndex] &    2048) {
        if (layer->piEdges[EDGE_INDEX(11, i, j, m_vVolSize.x-1)] == NO_EDGE) {
          cellVerts[11]  = m_Isosurface->iVertices +MakeVertex(11, i, j, iLayer, sliceIsosurface);
        } else {
          cellVerts[11] = layer->piEdges[EDGE_INDEX(11, i, j, m_vVolSize.x-1)];
        }
      }

      // put the cellVerts tags into this cell's layer->edges table
      for (size_t iEdge = 0; iEdge < 12; iEdge++) {
          if (cellVerts[iEdge] != NO_EDGE) {
            layer->piEdges[EDGE_INDEX(iEdge, i, j, voxels[0])] = cellVerts[iEdge];
          }
      }

      // now propagate the vertex/normal tags to the adjacent cells to
      // the right and behind in this layer.
      if (i < m_vVolSize.x - 2) { // we should propagate to the right
        layer->piEdges[EDGE_INDEX( 3, i+1, j, voxels[0])] = cellVerts[1];
        layer->piEdges[EDGE_INDEX( 7, i+1, j, voxels[0])] = cellVerts[5];
        layer->piEdges[EDGE_INDEX( 8, i+1, j, voxels[0])] = cellVerts[9];
        layer->piEdges[EDGE_INDEX(11, i+1, j, voxels[0])] = cellVerts[10];
      }

      if (j < m_vVolSize.y - 2) { // we should propagate to the rear
        layer->piEdges[EDGE_INDEX( 2, i, j+1, voxels[0])] = cellVerts[0];
        layer->piEdges[EDGE_INDEX( 6, i, j+1, voxels[0])] = cellVerts[4];
        layer->piEdges[EDGE_INDEX(11, i, j+1, voxels[0])] = cellVerts[8];
        layer->piEdges[EDGE_INDEX(10, i, j+1, voxels[0])] = cellVerts[9];
      }

      // store the vertex indices in the triangle data structure
      int iTableIndex = 0;
      while (triTable[cellIndex][iTableIndex] != -1) {
        assert(cellVerts[triTable[cellIndex][iTableIndex+0]] >= 0);
        assert(cellVerts[triTable[cellIndex][iTableIndex+1]] >= 0);
        assert(cellVerts[triTable[cellIndex][iTableIndex+2]] >= 0);
        sliceIsosurface->AddTriangle(
          cellVerts[triTable[cellIndex][iTableIndex+0]],
          cellVerts[triTable[cellIndex][iTableIndex+1]],
          cellVerts[triTable[cellIndex][iTableIndex+2]]
        );
        iTableIndex+=3;
      }
    }
  }

  // add this layer's triangles to the global list
  m_Isosurface->AppendData(sliceIsosurface);
}

template <class T> int
MarchingCubes<T>::MakeVertex(int iEdgeIndex, int i, int j, int k,
                             Isosurface* sliceIso) {
  INTVECTOR3 vFrom; // first grid vertex
  INTVECTOR3 vTo; // second grid vertex

  T fFromValue, fToValue;
  size_t fidx, toidx;
  // on the edge index decide what the edges are
  // we can't use 'k' for the data index because we only have 2 slices, not the
  // whole volume; but we still need the write 'k' to get the position correct.
  // so we hack out all the z values to be 0.
  switch (iEdgeIndex) {
    case  0:
      vFrom = INTVECTOR3(  i,j+1,  k);  vTo  = INTVECTOR3(i+1,j+1,  k);
      fidx = DATA_INDEX(vFrom.x, vFrom.y, 0, m_vVolSize.x, m_vVolSize.y);
      toidx = DATA_INDEX(vTo.x, vTo.y, 0, m_vVolSize.x, m_vVolSize.y);
      // slice[0] vs. slice[1] comes from k: 'k' gives slice[0], 'k+1' gives
      // slice[1]
      fFromValue = slice[0][fidx];
      fToValue = slice[0][toidx];
      break;
    case  1:
      vFrom = INTVECTOR3(i+1,j+1,  k);  vTo  = INTVECTOR3(i+1,  j,  k);
      fidx = DATA_INDEX(vFrom.x, vFrom.y, 0, m_vVolSize.x, m_vVolSize.y);
      toidx = DATA_INDEX(vTo.x, vTo.y, 0, m_vVolSize.x, m_vVolSize.y);
      fFromValue = slice[0][fidx];
      fToValue = slice[0][toidx];
      break;
    case  2:
      vFrom = INTVECTOR3(i+1,  j,  k);  vTo  = INTVECTOR3(  i,  j,  k);
      fidx = DATA_INDEX(vFrom.x, vFrom.y, 0, m_vVolSize.x, m_vVolSize.y);
      toidx = DATA_INDEX(vTo.x, vTo.y, 0, m_vVolSize.x, m_vVolSize.y);
      fFromValue = slice[0][fidx];
      fToValue = slice[0][toidx];
      break;
    case  3:
      vFrom = INTVECTOR3(  i,  j,  k);  vTo  = INTVECTOR3(  i,j+1,  k);
      fidx = DATA_INDEX(vFrom.x, vFrom.y, 0, m_vVolSize.x, m_vVolSize.y);
      toidx = DATA_INDEX(vTo.x, vTo.y, 0, m_vVolSize.x, m_vVolSize.y);
      fFromValue = slice[0][fidx];
      fToValue = slice[0][toidx];
      break;
    case  4:
      vFrom = INTVECTOR3(  i,j+1,k+1);  vTo  = INTVECTOR3(i+1,j+1,k+1);
      fidx = DATA_INDEX(vFrom.x, vFrom.y, 0, m_vVolSize.x, m_vVolSize.y);
      toidx = DATA_INDEX(vTo.x, vTo.y, 0, m_vVolSize.x, m_vVolSize.y);
      fFromValue = slice[1][fidx];
      fToValue = slice[1][toidx];
      break;
    case  5:
      vFrom = INTVECTOR3(i+1,j+1,k+1);  vTo  = INTVECTOR3(i+1,  j,k+1);
      fidx = DATA_INDEX(vFrom.x, vFrom.y, 0, m_vVolSize.x, m_vVolSize.y);
      toidx = DATA_INDEX(vTo.x, vTo.y, 0, m_vVolSize.x, m_vVolSize.y);
      fFromValue = slice[1][fidx];
      fToValue = slice[1][toidx];
      break;
    case  6:
      vFrom = INTVECTOR3(i+1,  j,k+1);  vTo  = INTVECTOR3(  i,  j,k+1);
      fidx = DATA_INDEX(vFrom.x, vFrom.y, 0, m_vVolSize.x, m_vVolSize.y);
      toidx = DATA_INDEX(vTo.x, vTo.y, 0, m_vVolSize.x, m_vVolSize.y);
      fFromValue = slice[1][fidx];
      fToValue = slice[1][toidx];
      break;
    case  7:
      vFrom = INTVECTOR3(  i,  j,k+1);  vTo  = INTVECTOR3(  i,j+1,k+1);
      fidx = DATA_INDEX(vFrom.x, vFrom.y, 0, m_vVolSize.x, m_vVolSize.y);
      toidx = DATA_INDEX(vTo.x, vTo.y, 0, m_vVolSize.x, m_vVolSize.y);
      fFromValue = slice[1][fidx];
      fToValue = slice[1][toidx];
      break;
    case  8:
      vFrom = INTVECTOR3(  i,j+1,  k);  vTo  = INTVECTOR3(  i,j+1,k+1);
      fidx = DATA_INDEX(vFrom.x, vFrom.y, 0, m_vVolSize.x, m_vVolSize.y);
      toidx = DATA_INDEX(vTo.x, vTo.y, 0, m_vVolSize.x, m_vVolSize.y);
      fFromValue = slice[0][fidx];
      fToValue = slice[1][toidx];
      break;
    case  9:
      vFrom = INTVECTOR3(i+1,j+1,  k);  vTo  = INTVECTOR3(i+1,j+1,k+1);
      fidx = DATA_INDEX(vFrom.x, vFrom.y, 0, m_vVolSize.x, m_vVolSize.y);
      toidx = DATA_INDEX(vTo.x, vTo.y, 0, m_vVolSize.x, m_vVolSize.y);
      fFromValue = slice[0][fidx];
      fToValue = slice[1][toidx];
      break;
    case 10:
      vFrom = INTVECTOR3(i+1,  j,  k);  vTo  = INTVECTOR3(i+1,  j,k+1);
      fidx = DATA_INDEX(vFrom.x, vFrom.y, 0, m_vVolSize.x, m_vVolSize.y);
      toidx = DATA_INDEX(vTo.x, vTo.y, 0, m_vVolSize.x, m_vVolSize.y);
      fFromValue = slice[0][fidx];
      fToValue = slice[1][toidx];
      break;
    case 11:
      vFrom = INTVECTOR3(  i,  j,  k);  vTo  = INTVECTOR3(  i,  j,k+1);
      fidx = DATA_INDEX(vFrom.x, vFrom.y, 0, m_vVolSize.x, m_vVolSize.y);
      toidx = DATA_INDEX(vTo.x, vTo.y, 0, m_vVolSize.x, m_vVolSize.y);
      fFromValue = slice[0][fidx];
      fToValue = slice[1][toidx];
      break;
    default:
      assert(false);
      fFromValue = fToValue = T(0);
  }

  // determine the relative distance along edge vFrom->vTo that the
  // isosurface vertex lies
  float d = float(fFromValue - m_TIsoValue) / float(fFromValue - fToValue);
  if (d < EPSILON) d = 0.0f; else if (d > (1.0f-EPSILON)) d = 1.0f;

  // interpolate the vertex
  //FLOATVECTOR3 vVertex = FLOATVECTOR3(vFrom) + d * FLOATVECTOR3(vTo - vFrom);
  FLOATVECTOR3 vVertex = {{
    vFrom[0] + d * (vTo[0] - vFrom[0]),
    vFrom[1] + d * (vTo[1] - vFrom[1]),
    vFrom[2] + d * (vTo[2] - vFrom[2])
  }};
  // since we're doing our weird slice-by-slice stuff, the values appear
  // projected to the plane touching the origin; push them back to the current
  // slice we're on.
  vVertex[2] = vVertex[2] + this->slice_number;

  // we skip the normal as a bit of a hack.
  return sliceIso->AddVertex(vVertex, FLOATVECTOR3());
}

template <class T>
LayerTempData<T>::LayerTempData(const VECTOR3<int>& vVolSize,
                                const T* bot, const T* top) :
  botvol(bot),
  topvol(top)
{
  m_vVolSize = vVolSize;

  // allocate storage to hold the indexing tags for edges in the layer
  piEdges = new int[(vVolSize.x-1) * (vVolSize.y-1) * 12];

  // init edge list
  const size_t len = (vVolSize.x-1) * (vVolSize.y-1) * 12;
  for(size_t i=0; i < len; ++i) {
    piEdges[i] = NO_EDGE;
  }
}

template <class T> LayerTempData<T>::~LayerTempData() {
  delete[] piEdges;
}

template<typename T> size_t
marchlayer(const T* s0, const T* s1, const size_t dims[3], uint64_t layer,
           float isovalue, FILE* vertices, FILE* faces,
           const uint64_t nvertices)
{
  static MarchingCubes<T> mc;
//  mc.ResetIsosurf();
  mc.slice_number = layer;
  mc.SetVolume(dims[0], dims[1], dims[2], s0, s1);
  mc.Process(isovalue);
  const Isosurface* iso = mc.m_Isosurface;
  for(size_t vert=0; vert < iso->iVertices; ++vert) {
#if 0
    fprintf(vertices, "v %f %f %f\n", iso->vfVertices[vert][0],
            iso->vfVertices[vert][1], iso->vfVertices[vert][2]);
#else
    const size_t nelem = fwrite(iso->vfVertices[vert].data(), sizeof(float), 3,
                                vertices);
    if(nelem != 3) {
      ERR(mcpp, "vertex write failed (%zu): %d", nelem, errno);
    }
#endif
  }
  for(size_t tri=0; tri < iso->iTriangles; ++tri) {
    /* nvertices accounts for previous layers. +1: OBJ indices are 1-based. */
#if 0
    fprintf(faces, "f %lu %lu %lu\n",
            nvertices + (iso->viTriangles[tri][0] + 1),
            nvertices + (iso->viTriangles[tri][1] + 1),
            nvertices + (iso->viTriangles[tri][2] + 1));
#else
    const std::array<long unsigned int,3> v = {
            nvertices + (iso->viTriangles[tri][0] + 1),
            nvertices + (iso->viTriangles[tri][1] + 1),
            nvertices + (iso->viTriangles[tri][2] + 1)
    };
    const size_t nelem = fwrite(v.data(), sizeof(uint32_t), 3, faces);
    if(nelem != 3) {
      ERR(mcpp, "faces write failed (%zu): %d", nelem, errno);
    }
#endif
  }
  return iso->iVertices;
}

/** @returns the number of vertices processed in this iteration. */
CMARCH size_t
marchlayeru16(const uint16_t* s0, const uint16_t* s1, const size_t dims[3],
              uint64_t layer, float isovalue,
              FILE* vertices, FILE* faces,
              const uint64_t nvertices)
{
  return marchlayer<uint16_t>(s0,s1, dims, layer, isovalue, vertices, faces,
                              nvertices);
}
CMARCH size_t
marchlayer16(const int16_t* s0, const int16_t* s1, const size_t dims[3],
             uint64_t layer, float isovalue,
             FILE* vertices, FILE* faces,
             const uint64_t nvertices)
{
  return marchlayer<int16_t>(s0, s1, dims, layer, isovalue, vertices, faces,
                             nvertices);
}
CMARCH size_t
marchlayeru8(const uint8_t* s0, const uint8_t* s1, const size_t dims[3],
             uint64_t layer, float isovalue,
             FILE* vertices, FILE* faces,
             const uint64_t nvertices)
{
  return marchlayer<uint8_t>(s0, s1, dims, layer, isovalue, vertices, faces,
                             nvertices);
}
CMARCH size_t
marchlayer8(const int8_t* s0, const int8_t* s1, const size_t dims[3],
            uint64_t layer, float isovalue,
            FILE* vertices, FILE* faces,
            const uint64_t nvertices)
{
  return marchlayer<int8_t>(s0, s1, dims, layer, isovalue, vertices, faces,
                            nvertices);
}

/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2008 Scientific Computing and Imaging Institute,
   University of Utah.


   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included
   in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
   OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
   THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.
*/
