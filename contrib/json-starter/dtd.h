/* A simple data type descriptor. */
#ifndef TJF_DTD_H
#define TJF_DTD_H

enum dtype { BYTE, FLOAT32, FLOAT64, GARBAGE };

struct dtd {
  enum dtype datatype;
  size_t dims[3];
  /* rectilinear grids, these are where the coords are. */
  double* coords[3];
};

#endif
