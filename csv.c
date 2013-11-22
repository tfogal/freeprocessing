#include <stdio.h>
#include <stdlib.h>

struct particle {
  double p[3];
  double v[3];
};
static const struct particle particles[] = {
  {.p={1.0, 0.5, 0.9}, .v={0.3, 0.5, 0.1}},
  {.p={0.4, 0.1, 0.6}, .v={0.1, 0.4, 0.8}},
  {.p={0.8, 2.8, 0.2}, .v={0.2, 0.3, 0.9}},
  {.p={3.0, 3.9, 1.4}, .v={1.0, 0.2, 0.1}},
};
static const size_t n_particles = sizeof(particles) / sizeof(particles[0]);

int
main(int argc, char* argv[])
{
  if(argc != 2) {
    fprintf(stderr, "Usage: %s filename\n", argv[0]);
    return EXIT_FAILURE;
  }

  FILE* fp = fopen(argv[1], "w");
  if(!fp) {
    fprintf(stderr, "could not create '%s'\n", argv[1]);
    return EXIT_FAILURE;
  }
  fprintf(fp, "x, y, z, vx, vy, vz\n");
  for(size_t i=0; i < n_particles; ++i) {
    const struct particle* p = &particles[i];
    fprintf(fp, "%6.3f, %6.3f, %6.3f, %6.3f, %6.3f, %6.3f\n",
            p->p[0], p->p[1], p->p[2], p->v[0], p->v[1], p->v[2]);
  }
  fclose(fp);
  return EXIT_SUCCESS;
}
