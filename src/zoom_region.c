/* Config parameters. */
#include "../config.h"

#include <float.h>

#include "cell.h"
#include "gravity_properties.h"
#include "engine.h"
#include "proxy.h"
#include "space.h"
#include "zoom_region.h"

/* MPI headers. */
#ifdef WITH_MPI
#include <mpi.h>
#endif

void construct_zoom_region(struct swift_params *params, struct space *s) {
#ifdef WITH_ZOOM_REGION
  const float BUFFER_SIZE = 2.0;

  /* Compute the bounds of the zoom region from the DM particles. */
  double zoom_boundary[6] = {1e20, -1e20, 1e20, -1e20, 1e20, -1e20};
  const size_t nr_gparts = s->nr_gparts;

  for (size_t k = 0; k < nr_gparts; k++) {
    if (s->gparts[k].type != swift_type_dark_matter) continue;

    if (s->gparts[k].x[0] < zoom_boundary[0])
      zoom_boundary[0] = s->gparts[k].x[0];
    if (s->gparts[k].x[0] > zoom_boundary[1])
      zoom_boundary[1] = s->gparts[k].x[0];
    if (s->gparts[k].x[1] < zoom_boundary[2])
      zoom_boundary[2] = s->gparts[k].x[1];
    if (s->gparts[k].x[1] > zoom_boundary[3])
      zoom_boundary[3] = s->gparts[k].x[1];
    if (s->gparts[k].x[2] < zoom_boundary[4])
      zoom_boundary[4] = s->gparts[k].x[2];
    if (s->gparts[k].x[2] > zoom_boundary[5])
      zoom_boundary[5] = s->gparts[k].x[2];
  }

  /* Compute maximum side length of the zoom region, need zoom boundary to be symetric. */
  const double max_length = max3(zoom_boundary[1]-zoom_boundary[0],
                                 zoom_boundary[3]-zoom_boundary[2],
                                 zoom_boundary[5]-zoom_boundary[4]) * BUFFER_SIZE;

  /* Geometric centre of zoom region. */
  double midpoint[3] = {zoom_boundary[0] + (zoom_boundary[1] - zoom_boundary[0]) / 2.,
                        zoom_boundary[2] + (zoom_boundary[3] - zoom_boundary[2]) / 2.,
                        zoom_boundary[4] + (zoom_boundary[5] - zoom_boundary[4]) / 2.};

  /* Compute shift to put zoom region in the center of the box. */
  const double boxsize = s->dim[0];
  const double shift[3] = {boxsize / 2. - midpoint[0],
                           boxsize / 2. - midpoint[1],
                           boxsize / 2. - midpoint[2]};
  for (int i = 0; i < 3; i++) s->zoom_props->shift[i] = shift[i];

  /* Store size of zoom cell. */
  for (int i = 0; i < 3; i++) s->zoom_props->width[i] = max_length;

  /* Compute the background grid. */
  int n_background_cells = (int)floor(boxsize / max_length);
  if (n_background_cells % 2 == 0) n_background_cells -= 1; // Ensure odd number of cells.
  s->zoom_props->n_background_cells = n_background_cells;  

  /* Resize the top level cells in the space. */
  const double dmax = max3(s->dim[0], s->dim[1], s->dim[2]);
  s->cell_min = 0.99 * dmax / n_background_cells;

  /* Check that it is big enough. */
  const double dmin = min3(s->dim[0], s->dim[1], s->dim[2]);
  int needtcells = 3 * dmax / dmin;
  if (n_background_cells < needtcells)
    error(
        "Scheduler:max_top_level_cells is too small %d, needs to be at "
        "least %d",
        n_background_cells, needtcells);
#endif
}

void zoom_region_init(struct swift_params *params, struct space *s) {
#ifdef WITH_ZOOM_REGION
  /* Are we running with a zoom region? */
  s->with_zoom_region = parser_get_opt_param_int(params, "ZoomRegion:enable", 0);

  /* If so... */
  if (s->with_zoom_region) {

    /* Zoom region properties are stored in a structure. */
    s->zoom_props = (struct zoom_region_properties *)malloc(
        sizeof(struct zoom_region_properties));
    if (s->zoom_props == NULL)
      error("Error allocating memory for the zoom parameters.");

    const int cdim = parser_get_param_int(params, "ZoomRegion:max_top_level_cells");
    for (int i = 0; i < 3; i++) s->zoom_props->cdim[i] = cdim;

    construct_zoom_region(params, s);
  }
#endif
}
