#ifndef FREEPROCESSING_PSIPHI_CONFIG_H
#define FREEPROCESSING_PSIPHI_CONFIG_H

enum Axis { X=0, Y=1, Z=2 };
/* desired slice information: {Y, 5} means the 5th slice of the Y axis.  We
 * parse the information on which slice[s] the user wants from the config file.
 */
struct slice {
  enum Axis axis;
  size_t idx;
};
struct field {
  char* name;
  size_t lower; /* offset of this field in bytestream */
  size_t upper; /* final byte in stream +1 for this field */
  bool out3d; /* should we output a unified 3D volume of this field? */
};
struct header {
  size_t nghost; /* number of ghost cells, per-dim */
  size_t dims[3]; /* dimensions of each brick, with ghost cells */
  size_t nbricks[3]; /* number of bricks, per dimension */
};

#endif
