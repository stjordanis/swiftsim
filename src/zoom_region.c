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
                                 zoom_boundary[5]-zoom_boundary[4]) 
                                * s->zoom_props->zoom_buffer_size;

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

  /* Compute the background grid. */
  int n_background_cells = (int)floor(boxsize / max_length);
  if (n_background_cells % 2 == 0) n_background_cells -= 1; // Ensure odd number of cells.

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

    /* Number of zoom top level cells covering the zoom region. */
    const int cdim = parser_get_param_int(params, "ZoomRegion:max_top_level_cells");
    for (int i = 0; i < 3; i++) s->zoom_props->cdim[i] = cdim;

    /* Buffer size to expand the zoom region. */
    const float zoom_buffer = parser_get_param_float(params, "ZoomRegion:buffer_size");
    s->zoom_props->zoom_buffer_size = zoom_buffer;

    construct_zoom_region(params, s);
  }
#endif
}

/**
 * @brief Minimum distance between two TL cells with different sizes.
 *
 * @param ci, cj The two TL cells.
 * @param periodic Account for periodicity?
 * @param dim The boxsize.
 */
double cell_min_dist2_diff_size(const struct cell *restrict ci,
                                const struct cell *restrict cj,
                                const int periodic, const double dim[3]) {
#ifdef WITH_ZOOM_REGION

#ifdef SWIFT_DEBUG_CHECKS
  if (ci->width[0] == cj->width[0]) error("x cells of same size!");
  if (ci->width[1] == cj->width[1]) error("y cells of same size!");
  if (ci->width[2] == cj->width[2]) error("z cells of same size!");
#endif

  const double cix = ci->loc[0] + ci->width[0]/2.;
  const double ciy = ci->loc[1] + ci->width[1]/2.;
  const double ciz = ci->loc[2] + ci->width[2]/2.;

  const double cjx = cj->loc[0] + cj->width[0]/2.;
  const double cjy = cj->loc[1] + cj->width[1]/2.;
  const double cjz = cj->loc[2] + cj->width[2]/2.;

  const double diag_ci2 = ci->width[0] * ci->width[0] + ci->width[1] * ci->width[1] + ci->width[2] * ci->width[2];
  const double diag_cj2 = cj->width[0] * cj->width[0] + cj->width[1] * cj->width[1] + cj->width[2] * cj->width[2];

  /* Get the distance between the cells */
  double dx = cix - cjx;
  double dy = ciy - cjy;
  double dz = ciz - cjz;

  /* Apply BC */
  if (periodic) {
    dx = nearest(dx, dim[0]);
    dy = nearest(dy, dim[1]);
    dz = nearest(dz, dim[2]);
  }
  const double r2 = dx * dx + dy * dy + dz * dz;

  /* Minimal distance between any 2 particles in the two cells */
  const double dist2 = r2 - (diag_ci2/2. + diag_cj2/2.);

  return dist2;
#else
  return 0;
#endif
}

/**
 * @brief Minimum distance between two TL cells.
 *
 * Generic wrapper, don't know if the TL cells are the same size or not at time of calling.
 *
 * @param ci, cj The two TL cells.
 * @param periodic Account for periodicity?
 * @param dim The boxsize.
 */
double cell_min_dist2(const struct cell *restrict ci,
                      const struct cell *restrict cj, const int periodic,
                      const double dim[3]) {
#ifdef WITH_ZOOM_REGION
  double dist2;

  /* Two TL cells the same size. */
  if (ci->width[0] == cj->width[0]) {
    dist2 = cell_min_dist2_same_size(ci, cj, periodic, dim);
  /* Two TL cells of different sizes. */
  } else {
    dist2 = cell_min_dist2_diff_size(ci, cj, periodic, dim);
  }

  return dist2;
#else
  return 0;
#endif
}
