#include <assert.h>
#include <inttypes.h>
#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../debug.h"
#include "mcubes.h"
#include "MC.h"

DECLARE_CHANNEL(mc);

struct mcubes {
  func_init* init;
  func_process* run;
  func_finished* fini;
  bool skip;

  /* given state */
  bool is_signed;
  size_t bpc;
  size_t dims[3];
  float isoval;

  /* internal state */
  FILE* vertices;
  FILE* faces;
  size_t n_vertices; /* vertices output so far. */
  size_t slice; /* which slice are we processing? */
  float points[12][3]; /* for triangulating intermediate pieces */
};

static void
init(void* self, const bool is_signed, const size_t bpc, const size_t d[3])
{
  struct mcubes* mc = (struct mcubes*) self;
  mc->is_signed = is_signed;
  mc->bpc = bpc;
  mc->slice = 0;
  mc->vertices = fopen(".vertices", "wb");
  mc->faces = fopen(".faces", "wb");
  mc->n_vertices = 0;
  memcpy(mc->dims, d, sizeof(size_t)*3);

  FILE* fp = fopen(".isovalue", "r");
  if(!fp) {
    fprintf(stderr, "could not read isovalue from '.isovalue'!\n");
    abort();
  }
  if(fscanf(fp, "%f", &mc->isoval) != 1) {
    fprintf(stderr, "could not scan FP from .isovalue!\n");
    abort();
  }
  fclose(fp);

  if(mc->vertices == NULL) {
    fprintf(stderr, "error open vertex file.\n"); abort();
  }
  if(mc->faces == NULL) {
    fprintf(stderr, "error open face data file.\n"); abort();
  }

  fprintf(mc->vertices, "# TJF's crazy MC v0.2\n");
}

/* samples the volume at the given position.  assumes 'this->dims' is a
 * size_t[3] (really, just size_t[2] will be okay) and that the volume is named
 * `data'. */
#define sample(x_,y_,z_) \
  data[z_*this->dims[1]*this->dims[0] + y_*this->dims[0] + x_]

/* @return true if the two given points straddle the given FP value. */
#define straddle(v_, x0,y0,z0, x1,y1,z1) \
  ({ typeof (*data) s0__ = sample(x0,y0,z0); \
     typeof (*data) s1__ = sample(x1,y1,z1); \
     (s0__ < v_ && v_ < s1__) || (s1__ < v_ && v_ < s0__); })

#define lerp(v, in0,in1, out0,out1) \
  (float)out0 + (v-in0) * ((float)out1-out0) / ((float)in1-in0)

static bool
equalf(const float a, const float b)
{
  return fabs(a - b) <= FLT_EPSILON;
}

static void
test_equalf()
{
  assert(equalf(0.0f, 0.0f));
  assert(equalf(0.0f, 0.1f) == false);
}

/* checks the order of 'a' and 'b', relative to 'c'. */
static bool
ordering_okay(const float a[3], const float b[3], const float c[3])
{
  const float gradient1 = (a[1] - c[1]) / (a[0] - c[0]);
  const float gradient2 = (b[1] - c[1]) / (b[0] - c[0]);

  if(equalf(a[0], c[0])) {
    return (gradient2 < 0) || (equalf(gradient2, 0.0f) && b[0] < c[0]);
  } else if(equalf(b[0], c[0])) {
    return (gradient1 < 0) || (equalf(gradient1, 0.0f) && a[0] < c[0]);
  }

  if(a[0] < c[0]) {
    if(b[0] < c[0]) {
      return gradient1 < gradient2;
    } else {
      return false;
    }
  } else {
    if(b[0] < c[0]) {
      return true;
    } else {
      return gradient1 <= gradient2;
    }
  }
}

/* swaps the given indices in the array. */
static void
swaparr(float points[12][3], size_t i, size_t j)
{
  float tmp[3];
  memcpy(tmp, points[i], sizeof(float)*3);
  memcpy(points[i], points[j], sizeof(float)*3);
  memcpy(points[j], tmp, sizeof(float)*3);
}

static void
test_swaparr()
{
  float abc[12][3] = {
    {0.0f, 0.0f, 0.0f},
    {1.0f, 1.0f, 1.0f},
    {2.0f, 2.0f, 2.0f},
    {3.0f, 3.0f, 3.0f},
    {4.0f, 4.0f, 4.0f},
    {5.0f, 5.0f, 5.0f},
    {6.0f, 6.0f, 6.0f},
    {7.0f, 7.0f, 7.0f},
    {8.0f, 8.0f, 8.0f},
    {9.0f, 9.0f, 9.0f},
    {10.0f, 10.0f, 10.0f},
    {11.0f, 11.0f, 11.0f}
  };
  assert(abc[0][0] == 0.0f && abc[0][1] == 0.0f && abc[0][2] == 0.0f);
  assert(abc[1][0] == 1.0f && abc[1][1] == 1.0f && abc[1][2] == 1.0f);
  swaparr(abc, 0, 1);
  assert(abc[0][0] == 1.0f && abc[0][1] == 1.0f && abc[0][2] == 1.0f);
  assert(abc[1][0] == 0.0f && abc[1][1] == 0.0f && abc[1][2] == 0.0f);
  swaparr(abc, 0, 1);
  assert(abc[0][0] == 0.0f && abc[0][1] == 0.0f && abc[0][2] == 0.0f);
  assert(abc[1][0] == 1.0f && abc[1][1] == 1.0f && abc[1][2] == 1.0f);

  swaparr(abc, 8, 0);
  assert(abc[0][0] == 8.0f && abc[0][1] == 8.0f && abc[0][2] == 8.0f);
  assert(abc[8][0] == 0.0f && abc[8][1] == 0.0f && abc[8][2] == 0.0f);
  swaparr(abc, 0, 8);
  assert(abc[0][0] == 0.0f && abc[0][1] == 0.0f && abc[0][2] == 0.0f);
  assert(abc[8][0] == 8.0f && abc[8][1] == 8.0f && abc[8][2] == 8.0f);
}

/* sorts up to 12 points; 'n' tells us how many points, exactly. */
static void
sort_points(float points[12][3], size_t n) {
  { /* move point with the minimum y to the beginning of the array */
    float min_y = points[0][1];
    size_t min_y_idx = 0;
    for(size_t i=1; i < n; ++i) {
      if(points[i][1] < min_y) {
        min_y = points[i][1];
        min_y_idx = i;
      }
    }
    if(min_y_idx != 0) {
      swaparr(points, 0, min_y_idx);
    }
  }

  /* start at 1: 0th element is already sorted, as per above. */
  for(size_t i=1; i < n; ++i) {
    for(size_t j=1; j < n-i; ++j) { /* note n-i */
      if(!ordering_okay(points[j], points[j+1], points[0])) {
        swaparr(points, j, j+1);
      }
    }
  }
}

static void
print_points(float a[12][3], const size_t n)
{
  printf("\n");
  for(size_t i=0; i < n; ++i) {
    printf("{ %5.3f %5.3f %5.3f }\n", a[i][0], a[i][1], a[i][2]);
  }
}

static void
test_sort_points()
{
  float a[12][3] = {
    {0.6f, 6.2f, 11.0f},
    {1.0f, 8.5f, 10.0f},
    {2.0f, 6.0f, 9.0f},
    {3.0f, 3.3f, 8.0f},
    {4.0f, 7.0f, 7.0f},
    {5.0f, 1.5f, 6.0f},
    {6.0f, 3.9f, 5.0f},
    {7.0f, 4.0f, 4.0f},
    {8.0f, 5.8f, 3.0f},
    {9.0f, 7.2f, 2.0f},
    {10.0f, 1.0f, 1.0f},
    {11.0f, 0.1f, 0.1f},
  };
  sort_points(a, 12);
#if 0
  print_points(a, 12);
  assert(equalf(abc[0][0], 0.0f));
  assert(equalf(abc[5][0], 5.0f));
  assert(equalf(abc[10][0], 10.0f));
  assert(equalf(abc[1][1], 1.0f));
  assert(equalf(abc[3][1], 3.0f));
  assert(equalf(abc[7][1], 7.0f));
  assert(equalf(abc[2][2], 2.0f));
  assert(equalf(abc[8][2], 8.0f));
  assert(equalf(abc[9][2], 9.0f));
#endif
}

static void
emit(const float pt[3], size_t *i, struct mcubes* this)
{
  /* we actually wait and write out the point in emit_faces!  this is because
   * we're going to reorder them. */
  /* fprintf(this->vertices, "v %f %f %f\n", pt[0], pt[1], pt[2]); */
  memcpy(this->points[*i], pt, sizeof(float)*3);
  *i = (*i) + 1;

  if(pt[0] > this->dims[0]) {
    fprintf(stderr, "wtf %f > %zu!!\n", pt[0], this->dims[0]);
  }
}

static void
emit_faces(struct mcubes* this, size_t n)
{
  printf("[tjf] triangulating %zu points.\n", n);
  sort_points(this->points, n);

  /* first, output our vertices. */
  for(size_t i=0; i < n; ++i) {
    for(size_t j=0; j < 3; ++j) { /* fix up nonsense points. */
      if(!isfinite(this->points[i][j])) {
        this->points[i][j] = 0.0f;
      }
    }
    fprintf(this->vertices, "v %f %f %f\n", this->points[i][0],
            this->points[i][1], this->points[i][2]);
  }
  /* if there's only two points, then it barely clipped the edge of the voxel.
   * just ignore it, it's irrelevant. */
  if(n > 2) {
    print_points(this->points, n);
    for(size_t i=0; i < n-2; ++i) { /* note n-2, don't run off end! */
      /* among the set of 12-or-less points given right now, our indices are
       * just 'i'-based.  but we need to be cognizant of all the points we've
       * output on previous calls! */
      /* also note: OBJ face indices are 1-based, not 0-based. */
      fprintf(this->faces, "f %zu %zu %zu\n", this->n_vertices+i+1,
              this->n_vertices+i+2, this->n_vertices+i+3);
    }
  }
  this->n_vertices += n;
}

/* For any edge, if one vertex is inside of the surface and the other
 * is outside of the surface then the edge intersects the surface.
 * For any cube the are 2^8=256 possible sets of vertex states. This
 * table lists the edges intersected by the surface for all 256
 * possible vertex states.  There are 12 edges.  For each entry in the
 * table, if edge #n is intersected, then bit #n is set to 1 .*/
static unsigned EdgeTable[256] =
{
  0x000, 0x109, 0x203, 0x30a, 0x406, 0x50f, 0x605, 0x70c, 0x80c, 0x905,
  0xa0f, 0xb06, 0xc0a, 0xd03, 0xe09, 0xf00, 0x190, 0x099, 0x393, 0x29a,
  0x596, 0x49f, 0x795, 0x69c, 0x99c, 0x895, 0xb9f, 0xa96, 0xd9a,
  0xc93, 0xf99, 0xe90, 0x230, 0x339, 0x033, 0x13a, 0x636, 0x73f, 0x435,
  0x53c, 0xa3c, 0xb35, 0x83f, 0x936, 0xe3a, 0xf33, 0xc39, 0xd30, 0x3a0,
  0x2a9, 0x1a3, 0x0aa, 0x7a6, 0x6af, 0x5a5, 0x4ac, 0xbac, 0xaa5,
  0x9af, 0x8a6, 0xfaa, 0xea3, 0xda9, 0xca0, 0x460, 0x569, 0x663, 0x76a,
  0x066, 0x16f, 0x265, 0x36c, 0xc6c, 0xd65, 0xe6f, 0xf66, 0x86a,
  0x963, 0xa69, 0xb60, 0x5f0, 0x4f9, 0x7f3, 0x6fa, 0x1f6, 0x0ff, 0x3f5,
  0x2fc, 0xdfc, 0xcf5, 0xfff, 0xef6, 0x9fa, 0x8f3, 0xbf9, 0xaf0, 0x650,
  0x759, 0x453, 0x55a, 0x256, 0x35f, 0x055, 0x15c, 0xe5c, 0xf55,
  0xc5f, 0xd56, 0xa5a, 0xb53, 0x859, 0x950, 0x7c0, 0x6c9, 0x5c3, 0x4ca,
  0x3c6, 0x2cf, 0x1c5, 0x0cc, 0xfcc, 0xec5, 0xdcf, 0xcc6, 0xbca,
  0xac3, 0x9c9, 0x8c0, 0x8c0, 0x9c9, 0xac3, 0xbca, 0xcc6, 0xdcf, 0xec5,
  0xfcc, 0x0cc, 0x1c5, 0x2cf, 0x3c6, 0x4ca, 0x5c3, 0x6c9, 0x7c0, 0x950,
  0x859, 0xb53, 0xa5a, 0xd56, 0xc5f, 0xf55, 0xe5c, 0x15c, 0x055,
  0x35f, 0x256, 0x55a, 0x453, 0x759, 0x650, 0xaf0, 0xbf9, 0x8f3, 0x9fa,
  0xef6, 0xfff, 0xcf5, 0xdfc, 0x2fc, 0x3f5, 0x0ff, 0x1f6, 0x6fa,
  0x7f3, 0x4f9, 0x5f0, 0xb60, 0xa69, 0x963, 0x86a, 0xf66, 0xe6f, 0xd65,
  0xc6c, 0x36c, 0x265, 0x16f, 0x066, 0x76a, 0x663, 0x569, 0x460, 0xca0,
  0xda9, 0xea3, 0xfaa, 0x8a6, 0x9af, 0xaa5, 0xbac, 0x4ac, 0x5a5,
  0x6af, 0x7a6, 0x0aa, 0x1a3, 0x2a9, 0x3a0, 0xd30, 0xc39, 0xf33, 0xe3a,
  0x936, 0x83f, 0xb35, 0xa3c, 0x53c, 0x435, 0x73f, 0x636, 0x13a,
  0x033, 0x339, 0x230, 0xe90, 0xf99, 0xc93, 0xd9a, 0xa96, 0xb9f, 0x895,
  0x99c, 0x69c, 0x795, 0x49f, 0x596, 0x29a, 0x393, 0x099, 0x190, 0xf00,
  0xe09, 0xd03, 0xc0a, 0xb06, 0xa0f, 0x905, 0x80c, 0x70c, 0x605,
  0x50f, 0x406, 0x30a, 0x203, 0x109, 0x000
};

/* For each of the possible vertex states listed in aiCubeEdgeFlags
 * there is a specific triangulation of the edge intersection points.
 * a2iTriangleConnectionTable lists all of them in the form of 0-5
 * edge triples with the list terminated by the invalid value -1.  For
 * example: triTable[3] list the 2 triangles formed
 * when corner[0] and corner[1] are inside of the surface, but the
 * rest of the cube is not. */
static const int16_t triTable[256][16] = {
  {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {0, 8, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {0, 1, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {1, 8, 3, 9, 8, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {1, 2, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {0, 8, 3, 1, 2, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {9, 2, 10, 0, 2, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {2, 8, 3, 2, 10, 8, 10, 9, 8, -1, -1, -1, -1, -1, -1, -1},
  {3, 11, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {0, 11, 2, 8, 11, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {1, 9, 0, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {1, 11, 2, 1, 9, 11, 9, 8, 11, -1, -1, -1, -1, -1, -1, -1},
  {3, 10, 1, 11, 10, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {0, 10, 1, 0, 8, 10, 8, 11, 10, -1, -1, -1, -1, -1, -1, -1},
  {3, 9, 0, 3, 11, 9, 11, 10, 9, -1, -1, -1, -1, -1, -1, -1},
  {9, 8, 10, 10, 8, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {4, 7, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {4, 3, 0, 7, 3, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {0, 1, 9, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {4, 1, 9, 4, 7, 1, 7, 3, 1, -1, -1, -1, -1, -1, -1, -1},
  {1, 2, 10, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {3, 4, 7, 3, 0, 4, 1, 2, 10, -1, -1, -1, -1, -1, -1, -1},
  {9, 2, 10, 9, 0, 2, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1},
  {2, 10, 9, 2, 9, 7, 2, 7, 3, 7, 9, 4, -1, -1, -1, -1},
  {8, 4, 7, 3, 11, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {11, 4, 7, 11, 2, 4, 2, 0, 4, -1, -1, -1, -1, -1, -1, -1},
  {9, 0, 1, 8, 4, 7, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1},
  {4, 7, 11, 9, 4, 11, 9, 11, 2, 9, 2, 1, -1, -1, -1, -1},
  {3, 10, 1, 3, 11, 10, 7, 8, 4, -1, -1, -1, -1, -1, -1, -1},
  {1, 11, 10, 1, 4, 11, 1, 0, 4, 7, 11, 4, -1, -1, -1, -1},
  {4, 7, 8, 9, 0, 11, 9, 11, 10, 11, 0, 3, -1, -1, -1, -1},
  {4, 7, 11, 4, 11, 9, 9, 11, 10, -1, -1, -1, -1, -1, -1, -1},
  {9, 5, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {9, 5, 4, 0, 8, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {0, 5, 4, 1, 5, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {8, 5, 4, 8, 3, 5, 3, 1, 5, -1, -1, -1, -1, -1, -1, -1},
  {1, 2, 10, 9, 5, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {3, 0, 8, 1, 2, 10, 4, 9, 5, -1, -1, -1, -1, -1, -1, -1},
  {5, 2, 10, 5, 4, 2, 4, 0, 2, -1, -1, -1, -1, -1, -1, -1},
  {2, 10, 5, 3, 2, 5, 3, 5, 4, 3, 4, 8, -1, -1, -1, -1},
  {9, 5, 4, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {0, 11, 2, 0, 8, 11, 4, 9, 5, -1, -1, -1, -1, -1, -1, -1},
  {0, 5, 4, 0, 1, 5, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1},
  {2, 1, 5, 2, 5, 8, 2, 8, 11, 4, 8, 5, -1, -1, -1, -1},
  {10, 3, 11, 10, 1, 3, 9, 5, 4, -1, -1, -1, -1, -1, -1, -1},
  {4, 9, 5, 0, 8, 1, 8, 10, 1, 8, 11, 10, -1, -1, -1, -1},
  {5, 4, 0, 5, 0, 11, 5, 11, 10, 11, 0, 3, -1, -1, -1, -1},
  {5, 4, 8, 5, 8, 10, 10, 8, 11, -1, -1, -1, -1, -1, -1, -1},
  {9, 7, 8, 5, 7, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {9, 3, 0, 9, 5, 3, 5, 7, 3, -1, -1, -1, -1, -1, -1, -1},
  {0, 7, 8, 0, 1, 7, 1, 5, 7, -1, -1, -1, -1, -1, -1, -1},
  {1, 5, 3, 3, 5, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {9, 7, 8, 9, 5, 7, 10, 1, 2, -1, -1, -1, -1, -1, -1, -1},
  {10, 1, 2, 9, 5, 0, 5, 3, 0, 5, 7, 3, -1, -1, -1, -1},
  {8, 0, 2, 8, 2, 5, 8, 5, 7, 10, 5, 2, -1, -1, -1, -1},
  {2, 10, 5, 2, 5, 3, 3, 5, 7, -1, -1, -1, -1, -1, -1, -1},
  {7, 9, 5, 7, 8, 9, 3, 11, 2, -1, -1, -1, -1, -1, -1, -1},
  {9, 5, 7, 9, 7, 2, 9, 2, 0, 2, 7, 11, -1, -1, -1, -1},
  {2, 3, 11, 0, 1, 8, 1, 7, 8, 1, 5, 7, -1, -1, -1, -1},
  {11, 2, 1, 11, 1, 7, 7, 1, 5, -1, -1, -1, -1, -1, -1, -1},
  {9, 5, 8, 8, 5, 7, 10, 1, 3, 10, 3, 11, -1, -1, -1, -1},
  {5, 7, 0, 5, 0, 9, 7, 11, 0, 1, 0, 10, 11, 10, 0, -1},
  {11, 10, 0, 11, 0, 3, 10, 5, 0, 8, 0, 7, 5, 7, 0, -1},
  {11, 10, 5, 7, 11, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {10, 6, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {0, 8, 3, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {9, 0, 1, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {1, 8, 3, 1, 9, 8, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1},
  {1, 6, 5, 2, 6, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {1, 6, 5, 1, 2, 6, 3, 0, 8, -1, -1, -1, -1, -1, -1, -1},
  {9, 6, 5, 9, 0, 6, 0, 2, 6, -1, -1, -1, -1, -1, -1, -1},
  {5, 9, 8, 5, 8, 2, 5, 2, 6, 3, 2, 8, -1, -1, -1, -1},
  {2, 3, 11, 10, 6, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {11, 0, 8, 11, 2, 0, 10, 6, 5, -1, -1, -1, -1, -1, -1, -1},
  {0, 1, 9, 2, 3, 11, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1},
  {5, 10, 6, 1, 9, 2, 9, 11, 2, 9, 8, 11, -1, -1, -1, -1},
  {6, 3, 11, 6, 5, 3, 5, 1, 3, -1, -1, -1, -1, -1, -1, -1},
  {0, 8, 11, 0, 11, 5, 0, 5, 1, 5, 11, 6, -1, -1, -1, -1},
  {3, 11, 6, 0, 3, 6, 0, 6, 5, 0, 5, 9, -1, -1, -1, -1},
  {6, 5, 9, 6, 9, 11, 11, 9, 8, -1, -1, -1, -1, -1, -1, -1},
  {5, 10, 6, 4, 7, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {4, 3, 0, 4, 7, 3, 6, 5, 10, -1, -1, -1, -1, -1, -1, -1},
  {1, 9, 0, 5, 10, 6, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1},
  {10, 6, 5, 1, 9, 7, 1, 7, 3, 7, 9, 4, -1, -1, -1, -1},
  {6, 1, 2, 6, 5, 1, 4, 7, 8, -1, -1, -1, -1, -1, -1, -1},
  {1, 2, 5, 5, 2, 6, 3, 0, 4, 3, 4, 7, -1, -1, -1, -1},
  {8, 4, 7, 9, 0, 5, 0, 6, 5, 0, 2, 6, -1, -1, -1, -1},
  {7, 3, 9, 7, 9, 4, 3, 2, 9, 5, 9, 6, 2, 6, 9, -1},
  {3, 11, 2, 7, 8, 4, 10, 6, 5, -1, -1, -1, -1, -1, -1, -1},
  {5, 10, 6, 4, 7, 2, 4, 2, 0, 2, 7, 11, -1, -1, -1, -1},
  {0, 1, 9, 4, 7, 8, 2, 3, 11, 5, 10, 6, -1, -1, -1, -1},
  {9, 2, 1, 9, 11, 2, 9, 4, 11, 7, 11, 4, 5, 10, 6, -1},
  {8, 4, 7, 3, 11, 5, 3, 5, 1, 5, 11, 6, -1, -1, -1, -1},
  {5, 1, 11, 5, 11, 6, 1, 0, 11, 7, 11, 4, 0, 4, 11, -1},
  {0, 5, 9, 0, 6, 5, 0, 3, 6, 11, 6, 3, 8, 4, 7, -1},
  {6, 5, 9, 6, 9, 11, 4, 7, 9, 7, 11, 9, -1, -1, -1, -1},
  {10, 4, 9, 6, 4, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {4, 10, 6, 4, 9, 10, 0, 8, 3, -1, -1, -1, -1, -1, -1, -1},
  {10, 0, 1, 10, 6, 0, 6, 4, 0, -1, -1, -1, -1, -1, -1, -1},
  {8, 3, 1, 8, 1, 6, 8, 6, 4, 6, 1, 10, -1, -1, -1, -1},
  {1, 4, 9, 1, 2, 4, 2, 6, 4, -1, -1, -1, -1, -1, -1, -1},
  {3, 0, 8, 1, 2, 9, 2, 4, 9, 2, 6, 4, -1, -1, -1, -1},
  {0, 2, 4, 4, 2, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {8, 3, 2, 8, 2, 4, 4, 2, 6, -1, -1, -1, -1, -1, -1, -1},
  {10, 4, 9, 10, 6, 4, 11, 2, 3, -1, -1, -1, -1, -1, -1, -1},
  {0, 8, 2, 2, 8, 11, 4, 9, 10, 4, 10, 6, -1, -1, -1, -1},
  {3, 11, 2, 0, 1, 6, 0, 6, 4, 6, 1, 10, -1, -1, -1, -1},
  {6, 4, 1, 6, 1, 10, 4, 8, 1, 2, 1, 11, 8, 11, 1, -1},
  {9, 6, 4, 9, 3, 6, 9, 1, 3, 11, 6, 3, -1, -1, -1, -1},
  {8, 11, 1, 8, 1, 0, 11, 6, 1, 9, 1, 4, 6, 4, 1, -1},
  {3, 11, 6, 3, 6, 0, 0, 6, 4, -1, -1, -1, -1, -1, -1, -1},
  {6, 4, 8, 11, 6, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {7, 10, 6, 7, 8, 10, 8, 9, 10, -1, -1, -1, -1, -1, -1, -1},
  {0, 7, 3, 0, 10, 7, 0, 9, 10, 6, 7, 10, -1, -1, -1, -1},
  {10, 6, 7, 1, 10, 7, 1, 7, 8, 1, 8, 0, -1, -1, -1, -1},
  {10, 6, 7, 10, 7, 1, 1, 7, 3, -1, -1, -1, -1, -1, -1, -1},
  {1, 2, 6, 1, 6, 8, 1, 8, 9, 8, 6, 7, -1, -1, -1, -1},
  {2, 6, 9, 2, 9, 1, 6, 7, 9, 0, 9, 3, 7, 3, 9, -1},
  {7, 8, 0, 7, 0, 6, 6, 0, 2, -1, -1, -1, -1, -1, -1, -1},
  {7, 3, 2, 6, 7, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {2, 3, 11, 10, 6, 8, 10, 8, 9, 8, 6, 7, -1, -1, -1, -1},
  {2, 0, 7, 2, 7, 11, 0, 9, 7, 6, 7, 10, 9, 10, 7, -1},
  {1, 8, 0, 1, 7, 8, 1, 10, 7, 6, 7, 10, 2, 3, 11, -1},
  {11, 2, 1, 11, 1, 7, 10, 6, 1, 6, 7, 1, -1, -1, -1, -1},
  {8, 9, 6, 8, 6, 7, 9, 1, 6, 11, 6, 3, 1, 3, 6, -1},
  {0, 9, 1, 11, 6, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {7, 8, 0, 7, 0, 6, 3, 11, 0, 11, 6, 0, -1, -1, -1, -1},
  {7, 11, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {7, 6, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {3, 0, 8, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {0, 1, 9, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {8, 1, 9, 8, 3, 1, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1},
  {10, 1, 2, 6, 11, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {1, 2, 10, 3, 0, 8, 6, 11, 7, -1, -1, -1, -1, -1, -1, -1},
  {2, 9, 0, 2, 10, 9, 6, 11, 7, -1, -1, -1, -1, -1, -1, -1},
  {6, 11, 7, 2, 10, 3, 10, 8, 3, 10, 9, 8, -1, -1, -1, -1},
  {7, 2, 3, 6, 2, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {7, 0, 8, 7, 6, 0, 6, 2, 0, -1, -1, -1, -1, -1, -1, -1},
  {2, 7, 6, 2, 3, 7, 0, 1, 9, -1, -1, -1, -1, -1, -1, -1},
  {1, 6, 2, 1, 8, 6, 1, 9, 8, 8, 7, 6, -1, -1, -1, -1},
  {10, 7, 6, 10, 1, 7, 1, 3, 7, -1, -1, -1, -1, -1, -1, -1},
  {10, 7, 6, 1, 7, 10, 1, 8, 7, 1, 0, 8, -1, -1, -1, -1},
  {0, 3, 7, 0, 7, 10, 0, 10, 9, 6, 10, 7, -1, -1, -1, -1},
  {7, 6, 10, 7, 10, 8, 8, 10, 9, -1, -1, -1, -1, -1, -1, -1},
  {6, 8, 4, 11, 8, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {3, 6, 11, 3, 0, 6, 0, 4, 6, -1, -1, -1, -1, -1, -1, -1},
  {8, 6, 11, 8, 4, 6, 9, 0, 1, -1, -1, -1, -1, -1, -1, -1},
  {9, 4, 6, 9, 6, 3, 9, 3, 1, 11, 3, 6, -1, -1, -1, -1},
  {6, 8, 4, 6, 11, 8, 2, 10, 1, -1, -1, -1, -1, -1, -1, -1},
  {1, 2, 10, 3, 0, 11, 0, 6, 11, 0, 4, 6, -1, -1, -1, -1},
  {4, 11, 8, 4, 6, 11, 0, 2, 9, 2, 10, 9, -1, -1, -1, -1},
  {10, 9, 3, 10, 3, 2, 9, 4, 3, 11, 3, 6, 4, 6, 3, -1},
  {8, 2, 3, 8, 4, 2, 4, 6, 2, -1, -1, -1, -1, -1, -1, -1},
  {0, 4, 2, 4, 6, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {1, 9, 0, 2, 3, 4, 2, 4, 6, 4, 3, 8, -1, -1, -1, -1},
  {1, 9, 4, 1, 4, 2, 2, 4, 6, -1, -1, -1, -1, -1, -1, -1},
  {8, 1, 3, 8, 6, 1, 8, 4, 6, 6, 10, 1, -1, -1, -1, -1},
  {10, 1, 0, 10, 0, 6, 6, 0, 4, -1, -1, -1, -1, -1, -1, -1},
  {4, 6, 3, 4, 3, 8, 6, 10, 3, 0, 3, 9, 10, 9, 3, -1},
  {10, 9, 4, 6, 10, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {4, 9, 5, 7, 6, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {0, 8, 3, 4, 9, 5, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1},
  {5, 0, 1, 5, 4, 0, 7, 6, 11, -1, -1, -1, -1, -1, -1, -1},
  {11, 7, 6, 8, 3, 4, 3, 5, 4, 3, 1, 5, -1, -1, -1, -1},
  {9, 5, 4, 10, 1, 2, 7, 6, 11, -1, -1, -1, -1, -1, -1, -1},
  {6, 11, 7, 1, 2, 10, 0, 8, 3, 4, 9, 5, -1, -1, -1, -1},
  {7, 6, 11, 5, 4, 10, 4, 2, 10, 4, 0, 2, -1, -1, -1, -1},
  {3, 4, 8, 3, 5, 4, 3, 2, 5, 10, 5, 2, 11, 7, 6, -1},
  {7, 2, 3, 7, 6, 2, 5, 4, 9, -1, -1, -1, -1, -1, -1, -1},
  {9, 5, 4, 0, 8, 6, 0, 6, 2, 6, 8, 7, -1, -1, -1, -1},
  {3, 6, 2, 3, 7, 6, 1, 5, 0, 5, 4, 0, -1, -1, -1, -1},
  {6, 2, 8, 6, 8, 7, 2, 1, 8, 4, 8, 5, 1, 5, 8, -1},
  {9, 5, 4, 10, 1, 6, 1, 7, 6, 1, 3, 7, -1, -1, -1, -1},
  {1, 6, 10, 1, 7, 6, 1, 0, 7, 8, 7, 0, 9, 5, 4, -1},
  {4, 0, 10, 4, 10, 5, 0, 3, 10, 6, 10, 7, 3, 7, 10, -1},
  {7, 6, 10, 7, 10, 8, 5, 4, 10, 4, 8, 10, -1, -1, -1, -1},
  {6, 9, 5, 6, 11, 9, 11, 8, 9, -1, -1, -1, -1, -1, -1, -1},
  {3, 6, 11, 0, 6, 3, 0, 5, 6, 0, 9, 5, -1, -1, -1, -1},
  {0, 11, 8, 0, 5, 11, 0, 1, 5, 5, 6, 11, -1, -1, -1, -1},
  {6, 11, 3, 6, 3, 5, 5, 3, 1, -1, -1, -1, -1, -1, -1, -1},
  {1, 2, 10, 9, 5, 11, 9, 11, 8, 11, 5, 6, -1, -1, -1, -1},
  {0, 11, 3, 0, 6, 11, 0, 9, 6, 5, 6, 9, 1, 2, 10, -1},
  {11, 8, 5, 11, 5, 6, 8, 0, 5, 10, 5, 2, 0, 2, 5, -1},
  {6, 11, 3, 6, 3, 5, 2, 10, 3, 10, 5, 3, -1, -1, -1, -1},
  {5, 8, 9, 5, 2, 8, 5, 6, 2, 3, 8, 2, -1, -1, -1, -1},
  {9, 5, 6, 9, 6, 0, 0, 6, 2, -1, -1, -1, -1, -1, -1, -1},
  {1, 5, 8, 1, 8, 0, 5, 6, 8, 3, 8, 2, 6, 2, 8, -1},
  {1, 5, 6, 2, 1, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {1, 3, 6, 1, 6, 10, 3, 8, 6, 5, 6, 9, 8, 9, 6, -1},
  {10, 1, 0, 10, 0, 6, 9, 5, 0, 5, 6, 0, -1, -1, -1, -1},
  {0, 3, 8, 5, 6, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {10, 5, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {11, 5, 10, 7, 5, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {11, 5, 10, 11, 7, 5, 8, 3, 0, -1, -1, -1, -1, -1, -1, -1},
  {5, 11, 7, 5, 10, 11, 1, 9, 0, -1, -1, -1, -1, -1, -1, -1},
  {10, 7, 5, 10, 11, 7, 9, 8, 1, 8, 3, 1, -1, -1, -1, -1},
  {11, 1, 2, 11, 7, 1, 7, 5, 1, -1, -1, -1, -1, -1, -1, -1},
  {0, 8, 3, 1, 2, 7, 1, 7, 5, 7, 2, 11, -1, -1, -1, -1},
  {9, 7, 5, 9, 2, 7, 9, 0, 2, 2, 11, 7, -1, -1, -1, -1},
  {7, 5, 2, 7, 2, 11, 5, 9, 2, 3, 2, 8, 9, 8, 2, -1},
  {2, 5, 10, 2, 3, 5, 3, 7, 5, -1, -1, -1, -1, -1, -1, -1},
  {8, 2, 0, 8, 5, 2, 8, 7, 5, 10, 2, 5, -1, -1, -1, -1},
  {9, 0, 1, 5, 10, 3, 5, 3, 7, 3, 10, 2, -1, -1, -1, -1},
  {9, 8, 2, 9, 2, 1, 8, 7, 2, 10, 2, 5, 7, 5, 2, -1},
  {1, 3, 5, 3, 7, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {0, 8, 7, 0, 7, 1, 1, 7, 5, -1, -1, -1, -1, -1, -1, -1},
  {9, 0, 3, 9, 3, 5, 5, 3, 7, -1, -1, -1, -1, -1, -1, -1},
  {9, 8, 7, 5, 9, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {5, 8, 4, 5, 10, 8, 10, 11, 8, -1, -1, -1, -1, -1, -1, -1},
  {5, 0, 4, 5, 11, 0, 5, 10, 11, 11, 3, 0, -1, -1, -1, -1},
  {0, 1, 9, 8, 4, 10, 8, 10, 11, 10, 4, 5, -1, -1, -1, -1},
  {10, 11, 4, 10, 4, 5, 11, 3, 4, 9, 4, 1, 3, 1, 4, -1},
  {2, 5, 1, 2, 8, 5, 2, 11, 8, 4, 5, 8, -1, -1, -1, -1},
  {0, 4, 11, 0, 11, 3, 4, 5, 11, 2, 11, 1, 5, 1, 11, -1},
  {0, 2, 5, 0, 5, 9, 2, 11, 5, 4, 5, 8, 11, 8, 5, -1},
  {9, 4, 5, 2, 11, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {2, 5, 10, 3, 5, 2, 3, 4, 5, 3, 8, 4, -1, -1, -1, -1},
  {5, 10, 2, 5, 2, 4, 4, 2, 0, -1, -1, -1, -1, -1, -1, -1},
  {3, 10, 2, 3, 5, 10, 3, 8, 5, 4, 5, 8, 0, 1, 9, -1},
  {5, 10, 2, 5, 2, 4, 1, 9, 2, 9, 4, 2, -1, -1, -1, -1},
  {8, 4, 5, 8, 5, 3, 3, 5, 1, -1, -1, -1, -1, -1, -1, -1},
  {0, 4, 5, 1, 0, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {8, 4, 5, 8, 5, 3, 9, 0, 5, 0, 3, 5, -1, -1, -1, -1},
  {9, 4, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {4, 11, 7, 4, 9, 11, 9, 10, 11, -1, -1, -1, -1, -1, -1, -1},
  {0, 8, 3, 4, 9, 7, 9, 11, 7, 9, 10, 11, -1, -1, -1, -1},
  {1, 10, 11, 1, 11, 4, 1, 4, 0, 7, 4, 11, -1, -1, -1, -1},
  {3, 1, 4, 3, 4, 8, 1, 10, 4, 7, 4, 11, 10, 11, 4, -1},
  {4, 11, 7, 9, 11, 4, 9, 2, 11, 9, 1, 2, -1, -1, -1, -1},
  {9, 7, 4, 9, 11, 7, 9, 1, 11, 2, 11, 1, 0, 8, 3, -1},
  {11, 7, 4, 11, 4, 2, 2, 4, 0, -1, -1, -1, -1, -1, -1, -1},
  {11, 7, 4, 11, 4, 2, 8, 3, 4, 3, 2, 4, -1, -1, -1, -1},
  {2, 9, 10, 2, 7, 9, 2, 3, 7, 7, 4, 9, -1, -1, -1, -1},
  {9, 10, 7, 9, 7, 4, 10, 2, 7, 8, 7, 0, 2, 0, 7, -1},
  {3, 7, 10, 3, 10, 2, 7, 4, 10, 1, 10, 0, 4, 0, 10, -1},
  {1, 10, 2, 8, 7, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {4, 9, 1, 4, 1, 7, 7, 1, 3, -1, -1, -1, -1, -1, -1, -1},
  {4, 9, 1, 4, 1, 7, 0, 8, 1, 8, 7, 1, -1, -1, -1, -1},
  {4, 0, 3, 7, 4, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {4, 8, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {9, 10, 8, 10, 11, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {3, 0, 9, 3, 9, 11, 11, 9, 10, -1, -1, -1, -1, -1, -1, -1},
  {0, 1, 10, 0, 10, 8, 8, 10, 11, -1, -1, -1, -1, -1, -1, -1},
  {3, 1, 10, 11, 3, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {1, 2, 11, 1, 11, 9, 9, 11, 8, -1, -1, -1, -1, -1, -1, -1},
  {3, 0, 9, 3, 9, 11, 1, 2, 9, 2, 11, 9, -1, -1, -1, -1},
  {0, 2, 11, 8, 0, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {3, 2, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {2, 3, 8, 2, 8, 10, 10, 8, 9, -1, -1, -1, -1, -1, -1, -1},
  {9, 10, 2, 0, 9, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {2, 3, 8, 2, 8, 10, 0, 1, 8, 1, 10, 8, -1, -1, -1, -1},
  {1, 10, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {1, 3, 8, 9, 1, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {0, 9, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {0, 3, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
  {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}
};

/* @param compactID maps edge indices to indices in this->point */
static void
emit_faces1(struct mcubes* this, size_t n, const unsigned below,
            const size_t compactID[12])
{
  printf("[tjf] triangulating %zu points.\n", n);

  /* if it's just one point... eh, it's just a corner.  skip it. */
  /* if(n < 1) { return; } */

  /* first, output our vertices. */
  for(size_t i=0; i < n; ++i) {
    for(size_t j=0; j < 3; ++j) {
      if(!isfinite(this->points[i][j])) { /* fix up nonsense points. */
        abort();
        this->points[i][j] = 0.0f;
      }
    }
    fprintf(this->vertices, "v %f %f %f\n", this->points[i][0],
            this->points[i][1], this->points[i][2]);
  }

  /* now output our triangles.  there can be a maximum of 5. */
  for(size_t tri=0; tri < 5 && triTable[below][3*tri] != -1; ++tri) {
    fprintf(this->faces, "f ");
    for(size_t corner=0; corner < 3; ++corner) {
      assert(triTable[below][3*tri+corner] != -1);
      const size_t pointID = triTable[below][3*tri+corner];
      const size_t compacted_ptID = compactID[pointID];
      assert(compacted_ptID <= (this->n_vertices+n));
      /* the +1 due to OBJ being 1-based. */
      fprintf(this->faces, "%zu ", this->n_vertices + compacted_ptID + 1);
    }
    fprintf(this->faces, "\n");
  }
  fprintf(this->vertices, "# --------\n");
  fprintf(this->faces, "# -------\n");
  this->n_vertices += n;
}

/* need to add slice number to all the 'z's here! */
#define marching(dims_, isovalue)                                              \
  assert(dims_[0] > 2); assert(dims_[1] > 2);                                  \
    /* since we need four corners, indent by one.                              \
     * then we can be sure -1 is always a valid index. */                      \
    for(size_t y=1; y < dims_[1]; ++y) {                                       \
      for(size_t x=1; x < dims_[0]; ++x) {                                     \
        /* we have 12 edges.  look at each one and decide if it's above or     \
         * below the isovalue. */                                              \
        /* we need this for triangulating points later. */                     \
        size_t i=0;                                                            \
        /* first 8 are simple: the front face and back face. */                \
        for(size_t z=0; z < 2; ++z) {                                          \
          if(straddle(isovalue, x-1,y-1,z, x,y-1,z)) {                         \
            float point[3] = {                                                 \
              lerp(isovalue, sample(x-1,y-1,z),sample(x,y-1,z), x-1,x),        \
              y-1,                                                             \
              z + this->slice                                                  \
            };                                                                 \
            emit(point, &i, this);                                             \
          }                                                                    \
          if(straddle(isovalue, x-1,y,z, x,y,z)) {                             \
            float point[3] = {                                                 \
              lerp(isovalue, sample(x-1,y,z),sample(x,y,z), x-1,x),            \
              y,                                                               \
              z + this->slice                                                  \
            };                                                                 \
            emit(point, &i, this);                                             \
          }                                                                    \
          if(straddle(isovalue, x-1,y-1,z, x-1,y,z)) {                         \
            float point[3] = {                                                 \
              x-1,                                                             \
              lerp(isovalue, sample(x-1,y-1,z),sample(x-1,y,z), y-1,y),        \
              z + this->slice                                                  \
            };                                                                 \
            emit(point, &i, this);                                             \
          }                                                                    \
          if(straddle(isovalue, x,y-1,z, x,y,z)) {                             \
            float point[3] = {                                                 \
              x,                                                               \
              lerp(isovalue, sample(x,y-1,z),sample(x,y,z), y-1,y),            \
              z + this->slice                                                  \
            };                                                                 \
            emit(point, &i, this);                                             \
          }                                                                    \
        }                                                                      \
        /* next 4 are the edges that connect the front and back faces. */      \
        /* ... */                                                              \
        if(straddle(isovalue, x-1,y-1,0, x-1,y-1,1)) {                         \
          float point[3] = {                                                   \
            x-1,                                                               \
            y-1,                                                               \
            lerp(isovalue, sample(x-1,y-1,0),sample(x-1,y-1,1), this->slice,   \
                                                                this->slice+1) \
          };                                                                   \
          emit(point, &i, this);                                               \
        }                                                                      \
        if(straddle(isovalue, x-1,y,0, x-1,y,1)) {                             \
          float point[3] = {                                                   \
            x-1,                                                               \
            y,                                                                 \
            lerp(isovalue, sample(x-1,y,0),sample(x-1,y,1), this->slice,       \
                                                            this->slice+1)     \
          };                                                                   \
          emit(point, &i, this);                                               \
        }                                                                      \
        if(straddle(isovalue, x,y-1,0, x,y-1,1)) {                             \
          float point[3] = {                                                   \
            x,                                                                 \
            y-1,                                                               \
            lerp(isovalue, sample(x,y-1,0),sample(x,y-1,1), this->slice,       \
                                                            this->slice+1)     \
          };                                                                   \
          emit(point, &i, this);                                               \
        }                                                                      \
        if(straddle(isovalue, x,y,0, x,y,1)) {                                 \
          float point[3] = {                                                   \
            x,                                                                 \
            y,                                                                 \
            lerp(isovalue, sample(x,y,0),sample(x,y,1), this->slice,           \
                                                        this->slice+1)         \
          };                                                                   \
          emit(point, &i, this);                                               \
        }                                                                      \
        emit_faces(this, i);                                                   \
      }                                                                        \
    }

void
marching1(void* self, const uint16_t* data, const size_t dims[3],
          const float isovalue)
{
  struct mcubes* this = (struct mcubes*) self;
  for(size_t y=1; y < dims[1]; ++y) {
    unsigned below = 0; /* vert flag list of vertices below isovalue */
    size_t edgePointMap[12];
    memset(edgePointMap, -1, sizeof(size_t)*12);
    for(size_t x=1; x < dims[0]; ++x) {
      /* local copy values at cube vertices. */
      /* yes, the sampling order here is absurd, but it needs to match
       * up with the ordering of EdgeTable, which I pulled from somewhere
       * and thus don't want to re-order. */
#if 0
      const float cubev[8] = {
        sample(x-1,y-1,0), /* 0 */
        sample(x,  y-1,0), /* 1 */
        sample(x,  y,  0), /* 2 */
        sample(x-1,y,  0), /* 3 */
        sample(x-1,y-1,1), /* 4 */
        sample(x,  y-1,1), /* 5 */
        sample(x,  y,  1), /* 6 */
        sample(x-1,y,  1), /* 7 */
      };

      if(sample(x-1,y-1,0) < isovalue) { below |= (1U << 0); }
      if(sample(x,  y-1,0) < isovalue) { below |= (1U << 1); }
      if(sample(x,  y,  0) < isovalue) { below |= (1U << 2); }
      if(sample(x-1,y,  0) < isovalue) { below |= (1U << 3); }
      if(sample(x-1,y-1,1) < isovalue) { below |= (1U << 4); }
      if(sample(x,  y-1,1) < isovalue) { below |= (1U << 5); }
      if(sample(x,  y,  1) < isovalue) { below |= (1U << 6); }
      if(sample(x-1,y,  1) < isovalue) { below |= (1U << 7); }
#else
      const float cubev[8] = {
        sample(x-1,y-1,1), /* 0 */
        sample(x,  y-1,1), /* 1 */
        sample(x,  y-1,0), /* 2 */
        sample(x-1,y-1,0), /* 3 */
        sample(x-1,y,  1), /* 4 */
        sample(x,  y,  1), /* 5 */
        sample(x,  y,  0), /* 6 */
        sample(x-1,y,  0), /* 7 */
      };

      if(sample(x-1,y-1,1) < isovalue) { below |= (1U << 0); }
      if(sample(x,  y-1,1) < isovalue) { below |= (1U << 1); }
      if(sample(x,  y-1,0) < isovalue) { below |= (1U << 2); }
      if(sample(x-1,y-1,0) < isovalue) { below |= (1U << 3); }
      if(sample(x-1,y,  1) < isovalue) { below |= (1U << 4); }
      if(sample(x,  y,  1) < isovalue) { below |= (1U << 5); }
      if(sample(x,  y,  0) < isovalue) { below |= (1U << 6); }
      if(sample(x-1,y,  0) < isovalue) { below |= (1U << 7); }
#endif
      unsigned edges = EdgeTable[below];
      /* entirely above / entirely below? */
      if(edges == 0) { /* done! */ continue; }

      /* This doesn't make much sense unless you are numbering the vertices the
       * same way I am / the EdgeTable does.  Which is:
       *
       *          4 +--------+ 5
       *           /.      / |
       *      7  /  .    /   |
       *       +--------+ 6  |
       *       |    .   |    |
       *       |   0.   |    | 1
       *       |    +...|....+
       *       |  .     |   /
       *       |.       | /
       *       +--------+
       *      3          2
       */

      size_t nverts = 0;
      /* now, in the expression (1U<<i), 'i' represents the edge idx.  note that
       * it is important that EdgeTable, our iteration 1U<<i, and cubev[]
       * indices all iterate over vertices/edges consistently! */
#if 0
      if(EdgeTable[below] & (1U<<0)) {
        /* i==0 represents the bottom edge, which is a change in X, so the X
         * coordinate is the one to lerp. */
        const float point[3] = {
          lerp(isovalue, cubev[0],cubev[1], x-1,x), y-1, this->slice
        };
        edgePointMap[0] = nverts;
        emit(point, &nverts, this);
      }
      if(EdgeTable[below] & (1U<<1)) { /* i==1, vert1 to vert2 */
        const float point[3] = {
          x, lerp(isovalue, cubev[1],cubev[2], y-1,y), this->slice
        };
        edgePointMap[1] = nverts;
        emit(point, &nverts, this);
      }
      if(EdgeTable[below] & (1U<<2)) { /* v2 -> v3 */
        const float point[3] = {
          lerp(isovalue, cubev[2],cubev[3], x,x-1), y, this->slice
        };
        edgePointMap[2] = nverts;
        emit(point, &nverts, this);
      }
      if(EdgeTable[below] & (1U<<3)) { /* v3 -> v0 */
        const float point[3] = {
          x-1, lerp(isovalue, cubev[3],cubev[0], y,y-1), this->slice
        };
        edgePointMap[3] = nverts;
        emit(point, &nverts, this);
      }
      if(EdgeTable[below] & (1U<<4)) {
        const float point[3] = {
          lerp(isovalue, cubev[4],cubev[5], x-1,x), y-1, this->slice+1
        };
        edgePointMap[4] = nverts;
        emit(point, &nverts, this);
      }
      if(EdgeTable[below] & (1U<<5)) {
        const float point[3] = {
          x, lerp(isovalue, cubev[5],cubev[6], y-1,y), this->slice+1
        };
        edgePointMap[5] = nverts;
        emit(point, &nverts, this);
      }
      if(EdgeTable[below] & (1U<<6)) {
        const float point[3] = {
          lerp(isovalue, cubev[6],cubev[7], x,x-1), y, this->slice+1
        };
        edgePointMap[6] = nverts;
        emit(point, &nverts, this);
      }
      if(EdgeTable[below] & (1U<<7)) {
        const float point[3] = {
          x-1, lerp(isovalue, cubev[7],cubev[4], y,y-1), this->slice+1
        };
        edgePointMap[7] = nverts;
        emit(point, &nverts, this);
      }
      /* now the 4 edges which connect the cube in Z. */
      if(EdgeTable[below] & (1U<<8)) {
        const float point[3] = {
          x-1, y-1, lerp(isovalue, cubev[0],cubev[4], this->slice,
                                                      this->slice+1)
        };
        edgePointMap[8] = nverts;
        emit(point, &nverts, this);
      }
      if(EdgeTable[below] & (1U<<9)) {
        const float point[3] = {
          x, y-1, lerp(isovalue, cubev[1],cubev[5], this->slice,
                                                    this->slice+1)
        };
        edgePointMap[9] = nverts;
        emit(point, &nverts, this);
      }
      if(EdgeTable[below] & (1U<<10)) {
        const float point[3] = {
          x, y, lerp(isovalue, cubev[2],cubev[6], this->slice,
                                                  this->slice+1)
        };
        edgePointMap[10] = nverts;
        emit(point, &nverts, this);
      }
      if(EdgeTable[below] & (1U<<11)) {
        const float point[3] = {
          x, y, lerp(isovalue, cubev[3],cubev[7], this->slice,
                                                  this->slice+1)
        };
        edgePointMap[11] = nverts;
        emit(point, &nverts, this);
      }
#else
      if(EdgeTable[below] & (1U<<0)) {
        /* i==0 represents the bottom edge, which is a change in X, so the X
         * coordinate is the one to lerp. */
        this->points[0][0] = lerp(isovalue, cubev[0],cubev[1], x-1,x);
        this->points[0][1] = y-1;
        this->points[0][2] = this->slice+1;
        edgePointMap[0] = nverts;
        nverts++;
      }
      if(EdgeTable[below] & (1U<<1)) { /* i==1, vert1 to vert2 */
        this->points[1][0] = x;
        this->points[1][1] = y-1;
        this->points[1][2] = lerp(isovalue, cubev[1],cubev[2],
                                  this->slice+1,this->slice);
        edgePointMap[1] = nverts;
        nverts++;
      }
      if(EdgeTable[below] & (1U<<2)) { /* v2 -> v3 */
        this->points[2][0] = lerp(isovalue, cubev[2],cubev[3], x,x-1);
        this->points[2][1] = y-1;
        this->points[2][2] = this->slice;
        edgePointMap[2] = nverts;
        nverts++;
      }
      if(EdgeTable[below] & (1U<<3)) { /* v3 -> v0 */
        this->points[3][0] = x-1;
        this->points[3][1] = y-1;
        this->points[3][1] = lerp(isovalue, cubev[3],cubev[0],
                                  this->slice,this->slice+1);
        edgePointMap[3] = nverts;
        nverts++;
      }
      if(EdgeTable[below] & (1U<<4)) {
        this->points[4][0] = lerp(isovalue, cubev[4],cubev[5], x-1,x);
        this->points[4][1] = y;
        this->points[4][2] = this->slice+1;
        edgePointMap[4] = nverts;
        nverts++;
      }
      if(EdgeTable[below] & (1U<<5)) {
        this->points[5][0] = x;
        this->points[5][1] = y;
        this->points[5][2] = lerp(isovalue, cubev[5],cubev[6],
                                  this->slice+1,this->slice);
        edgePointMap[5] = nverts;
        nverts++;
      }
      if(EdgeTable[below] & (1U<<6)) {
        this->points[6][0] = lerp(isovalue, cubev[6],cubev[7], x,x-1);
        this->points[6][1] = y;
        this->points[6][2] = this->slice;
        edgePointMap[6] = nverts;
        nverts++;
      }
      if(EdgeTable[below] & (1U<<7)) {
        this->points[7][0] = x-1;
        this->points[7][1] = y;
        this->points[7][2] =  lerp(isovalue, cubev[7],cubev[4],
                                   this->slice,this->slice+1);
        edgePointMap[7] = nverts;
        nverts++;
      }
      /* now the vertical edges */
      if(EdgeTable[below] & (1U<<8)) {
        this->points[8][0] = x-1;
        this->points[8][1] = lerp(isovalue, cubev[0],cubev[4], y-1,y);
        this->points[8][2] = this->slice+1;
        edgePointMap[8] = nverts;
        nverts++;
      }
      if(EdgeTable[below] & (1U<<9)) {
        this->points[9][0] = x;
        this->points[9][1] = lerp(isovalue, cubev[1],cubev[5], y-1,y);
        this->points[9][2] = this->slice+1;
        edgePointMap[9] = nverts;
        nverts++;
      }
      if(EdgeTable[below] & (1U<<10)) {
        this->points[10][0] = x;
        this->points[10][1] = lerp(isovalue, cubev[2],cubev[6], y-1,y);
        this->points[10][2] = this->slice;
        edgePointMap[10] = nverts;
        nverts++;
      }
      if(EdgeTable[below] & (1U<<11)) {
        this->points[11][0] = x-1;
        this->points[11][1] = lerp(isovalue, cubev[3],cubev[7], y-1,y);
        this->points[11][2] = this->slice;
        edgePointMap[11] = nverts;
        nverts++;
      }
#endif
      emit_faces1(this, nverts, below, edgePointMap);
    }
  }
}


static void
march(void* self, const void* d, const size_t nelems)
{
  struct mcubes* this = (struct mcubes*) self;

  if(nelems != this->dims[0]*this->dims[1]) {
    ERR(mc, "not enough data!");
    abort();
  }

  switch(this->bpc) {
    case 1:
      if(this->is_signed) {
        TRACE(mc, "signed 8bit");
        const int8_t* data = (const int8_t*) d;
        this->n_vertices += marchlayer8(data, this->dims, this->slice,
                                        this->isoval, this->vertices,
                                        this->faces, this->n_vertices);
      } else {
        TRACE(mc, "UNsigned 8bit");
        const uint8_t* data = (const uint8_t*) d;
        this->n_vertices += marchlayeru8(data, this->dims, this->slice,
                                         this->isoval, this->vertices,
                                         this->faces, this->n_vertices);
      }
      break;
    case 2:
      if(this->is_signed) {
        TRACE(mc, "signed 16bit");
        const int16_t* data = (const int16_t*) d;
        this->n_vertices += marchlayer16(data, this->dims, this->slice,
                                         this->isoval, this->vertices,
                                         this->faces, this->n_vertices);
      } else {
        TRACE(mc, "UNsigned 16bit");
        const uint16_t* data = (const uint16_t*) d;
        this->n_vertices += marchlayeru16(data, this->dims, this->slice,
                                          this->isoval, this->vertices,
                                          this->faces, this->n_vertices);
      }
      break;
    default: WARN(mc, "unimplemented."); abort(); /* unimplemented. */ break;
  }
#ifndef NDEBUG
  printf("[tjf] %zu vertices at slice %zu\n", this->n_vertices, this->slice);
#endif
  this->slice++;
}

static void
finalize(void* self)
{
  struct mcubes* mc = (struct mcubes*) self;
  if(fclose(mc->vertices) != 0) {
    perror("error closing vertex data file");
  }
  if(fclose(mc->faces) != 0) {
    perror("error closing face data file.");
  }
  mc->vertices = mc->faces = NULL;
}

static void
test_all()
{
  test_equalf();
  test_swaparr();
  test_sort_points();
}

struct processor*
mcubes()
{
  struct mcubes* mc = calloc(sizeof(struct mcubes), 1);
  mc->init = init;
  mc->run = march;
  mc->fini = finalize;
  mc->skip = false;
  assert(offsetof(struct mcubes, init) == offsetof(struct processor, init));
  assert(offsetof(struct mcubes, run ) == offsetof(struct processor, run));
  assert(offsetof(struct mcubes, fini) == offsetof(struct processor, fini));
  assert(offsetof(struct mcubes, skip) == offsetof(struct processor, skip));
  test_all();
  return (struct processor*)mc;
}
