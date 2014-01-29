#include <stdbool.h>
#include <mpi.h>
#include "compiler.h"

PURE size_t rank();
PURE size_t size();
void par_abort();

void broadcastf(float* f, size_t n);
void broadcastlf(double* lf, size_t n);
void broadcastzu(size_t* zu, size_t n);
void broadcasts(char* str, size_t len);
void broadcastb(bool* b, size_t n);

void allgatherf(float* f, int* rcvcount, int* displacements);
void allgatherlf(double* lf, int* rcvcount, int* displacements);
