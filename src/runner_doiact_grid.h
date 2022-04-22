//
// Created by yuyttenh on 23/03/22.
//

#ifndef SWIFTSIM_RUNNER_DOIACT_GRID_H
#define SWIFTSIM_RUNNER_DOIACT_GRID_H

/* Local headers. */
#include "active.h"
#include "cell.h"
#include "shadowswift/delaunay.h"
#include "space_getsid.h"
#include "timers.h"

#ifdef MOVING_MESH

__attribute((always_inline)) INLINE static void runner_dopair_grid_naive(
    struct cell *restrict ci, const struct cell *restrict cj,
    const struct engine *e, const struct sort_entry *restrict sort_i,
    const struct sort_entry *restrict sort_j, int flipped, const double *shift,
    const double rshift, const double hi_max, const double dx_max, int sid) {

  /* Get some useful values. */
  const int count_i = ci->hydro.count;
  const int count_j = cj->hydro.count;
  struct part *restrict parts_j = cj->hydro.parts;

  if (flipped) { /* cj on the left */

    const double di_min = sort_i[0].d - hi_max - dx_max + rshift;

    /* Loop over parts in cj */
    for (int pjd = count_j - 1; pjd >= 0 && sort_j[pjd].d > di_min; pjd--) {

      /* Recover pj */
      int pj_idx = sort_j[pjd].i;
      struct part *pj = &parts_j[pj_idx];

      /* Skip inhibited particles. */
      if (part_is_inhibited(pj, e)) continue;

      /* Shift pj so that it is in the frame of ci (with cj on the left) */
      const double pjx = pj->x[0] - shift[0];
      const double pjy = pj->x[1] - shift[1];
      const double pjz = pj->x[2] - shift[2];

      delaunay_add_new_vertex(ci->grid.delaunay, pjx, pjy, pjz, sid, pj_idx);
    }
  } else {
    const double di_max = sort_i[count_i - 1].d + hi_max + dx_max - rshift;

    /* Loop over parts in cj */
    for (int pjd = 0; pjd < count_j && sort_j[pjd].d < di_max; pjd++) {

      /* Recover pj */
      int pj_idx = sort_j[pjd].i;
      struct part *pj = &parts_j[pj_idx];

      /* Skip inhibited particles. */
      if (part_is_inhibited(pj, e)) continue;

      /* Shift pj so that it is in the frame of ci (with cj on the right) */
      const double pjx = pj->x[0] + shift[0];
      const double pjy = pj->x[1] + shift[1];
      const double pjz = pj->x[2] + shift[2];

      delaunay_add_new_vertex(ci->grid.delaunay, pjx, pjy, pjz, sid, pj_idx);
    }
  }
}

__attribute((always_inline)) INLINE static void runner_dopair_grid(
    struct cell *restrict ci, const struct cell *restrict cj,
    const struct engine *e, const struct sort_entry *restrict sort_i,
    const struct sort_entry *restrict sort_j,
    const struct sort_entry *restrict sort_active_i, int count_active, int flipped,
    const double *shift, const double rshift, const double hi_max,
    const double dx_max, int sid) {

  /* Get some useful values. */
  const int count_i = ci->hydro.count;
  const int count_j = cj->hydro.count;
  struct part *restrict parts_i = ci->hydro.parts;
  struct part *restrict parts_j = cj->hydro.parts;

  if (flipped) {
    /* ci on the right */

    const double di_min = sort_i[0].d - hi_max - dx_max + rshift;

    /* Loop over the parts in cj (on the left) */
    for (int pjd = count_j - 1; pjd >= 0 && sort_j[pjd].d > di_min; pjd--) {

      /* Recover pj */
      int pj_idx = sort_j[pjd].i;
      struct part *pj = &parts_j[pj_idx];

      /* Skip inhibited particles. */
      if (part_is_inhibited(pj, e)) continue;

      /* Shift pj so that it is in the frame of ci (with cj on the left) */
      const double pjx = pj->x[0] - shift[0];
      const double pjy = pj->x[1] - shift[1];
      const double pjz = pj->x[2] - shift[2];
      const double dj_max = sort_j[pjd].d + dx_max - rshift;

      /* Loop over the sorted active parts in ci (on the right) */
      for (int i = 0; i < count_active; i++) {

        /* Get a hold of pi. */
        struct sort_entry sort_pi = sort_active_i[i];
        struct part *restrict pi = &parts_i[sort_pi.i];

        /* Early abort? */
        const float ri = pi->h;
        if (sort_pi.d - ri >= dj_max) continue;

        /* Compute the pairwise distance. */
        double dx[3] = {pi->x[0] - pjx, pi->x[1] - pjy, pi->x[2] - pjz};
        const double r2 = dx[0] * dx[0] + dx[1] * dx[1] + dx[2] * dx[2];

        /* Hit or miss? */
        if (r2 < ri * ri) {
          delaunay_add_new_vertex(ci->grid.delaunay, pjx, pjy, pjz, sid,
                                  pj_idx);
          break;
        }
      } /* loop over the parts in ci. */
    }   /* loop over the parts in cj. */
  } else {
    /* ci on the left */

    const double di_max = sort_i[count_i - 1].d + hi_max + dx_max - rshift;

    /* Loop over the parts in cj (on the right) */
    for (int pjd = 0; pjd < count_j && sort_j[pjd].d < di_max; pjd++) {

      /* Recover pj */
      int pj_idx = sort_j[pjd].i;
      struct part *pj = &parts_j[sort_j[pjd].i];

      /* Skip inhibited particles. */
      if (part_is_inhibited(pj, e)) continue;

      /* Shift pj so that it is in the frame of ci (with cj on the right) */
      const double pjx = pj->x[0] + shift[0];
      const double pjy = pj->x[1] + shift[1];
      const double pjz = pj->x[2] + shift[2];
      const double dj_min = sort_j[pjd].d - dx_max + rshift;

      /* Loop over the sorted active parts in ci (on the right) */
      for (int i = 0; i < count_active; i++) {

        /* Get a hold of pi. */
        struct sort_entry sort_pi = sort_active_i[i];
        struct part *restrict pi = &parts_i[sort_pi.i];

        /* Early abort? */
        const float ri = pi->h;
        if (sort_pi.d + ri <= dj_min) continue;

        /* Compute the pairwise distance. */
        double dx[3] = {pi->x[0] - pjx, pi->x[1] - pjy, pi->x[2] - pjz};
        const double r2 = dx[0] * dx[0] + dx[1] * dx[1] + dx[2] * dx[2];

        /* Hit or miss? */
        if (r2 < ri * ri) {
          delaunay_add_new_vertex(ci->grid.delaunay, pjx, pjy, pjz, sid,
                                  pj_idx);
          break;
        }
      } /* loop over the parts in ci. */
    }   /* loop over the parts in cj. */
  }     /* Flipped? */
}

__attribute__((always_inline)) INLINE static void
runner_dopair_branch_grid_construction(struct runner *restrict r,
                                       struct cell *restrict ci,
                                       struct cell *restrict cj) {

  TIMER_TIC;

  const struct engine *restrict e = r->e;

#ifdef SWIFT_DEBUG_CHECKS
  assert(ci->hydro.count != 0 && cj->hydro.count != 0);
#endif

  /* Is the cell active and local? */
  assert((cell_is_active_hydro(ci, e) && ci->nodeID == e->nodeID));

  /* Check that cells are drifted. */
  if (!cell_are_part_drifted(ci, e) || !cell_are_part_drifted(cj, e))
    error("Interacting undrifted cells.");

  if (ci == cj) error("Interacting cell with itself!");

  /* Get the sort ID. */
  double shift[3] = {0.0, 0.0, 0.0};
  struct cell *ci_temp = ci;
  struct cell *cj_temp = cj;
  int sid = space_getsid(e->s, &ci_temp, &cj_temp, shift);
  const int flipped = ci != ci_temp;

  /* Have the cells been sorted? */
  if (!(ci->hydro.sorted & (1 << sid)) ||
      ci->hydro.dx_max_sort_old > space_maxreldx * ci->dmin)
    error("Interacting unsorted cells.");
  if (!(cj->hydro.sorted & (1 << sid)) ||
      cj->hydro.dx_max_sort_old > space_maxreldx * cj->dmin)
    error("Interacting unsorted cells.");

  /* Delaunay already allocated? */
  if (ci->grid.delaunay == NULL) {
    ci->grid.delaunay = delaunay_malloc(ci->loc, ci->width, ci->hydro.count);
    ci->grid.ti_old = e->ti_current;
  } else {
    /* Check if reset is needed */
    if (ci->grid.ti_old < e->ti_current) {
      delaunay_reset(ci->grid.delaunay, ci->loc, ci->width, ci->hydro.count);
      ci->grid.ti_old = e->ti_current;
    }
  }

  /* We are good to go!*/

  /* Pick-out the sorted lists. */
  const struct sort_entry *restrict sort_i = cell_get_hydro_sorts(ci, sid);
  const struct sort_entry *restrict sort_j = cell_get_hydro_sorts(cj, sid);

  /* Get the cutoff shift. */
  double rshift = 0.0;
  for (int k = 0; k < 3; k++) rshift += shift[k] * runner_shift[sid][k];

  /* Flip sid if needed */
  if (flipped) sid = 26 - sid;

    /* Mark cell face as inside of simulation volume */
#ifdef SWIFT_DEBUG_CHECKS
  if (ci->grid.delaunay->sid_is_inside_face[sid])
    error("Already ran construction task for this sid!");
#endif
  ci->grid.delaunay->sid_is_inside_face[sid] |= 1;

  /* Get some other useful values. */
  const int count_i = ci->hydro.count;
  const int count_j = cj->hydro.count;
  struct part *restrict parts_i = ci->hydro.parts;
  const float hi_max = ci->hydro.h_max_active;
  const float dx_max = (ci->hydro.dx_max_sort + cj->hydro.dx_max_sort);

  /* Naive interaction?
   * If > 90% of the particles of ci that could neighbour a particle of cj are
   * active, we just add all the particles of cj that could be within reach. */

  int count_active = 0;
  struct sort_entry *sort_active_i = (struct sort_entry *)malloc(count_i * sizeof(struct sort_entry));
  if (flipped) { /* ci on the right */

    const double di_max = sort_j[count_j - 1].d + hi_max + dx_max - rshift;

    if (rshift != 0.) {
      shift[0] = shift[0];
    }

    /* Loop over parts in ci that could interact with parts in cj */
    for (int pid = 0; pid < count_i && sort_i[pid].d < di_max; pid++) {

      /* Found active particle? */
      if (part_is_active(&parts_i[sort_i[pid].i], e)) {
        sort_active_i[count_active] = sort_i[pid];
        count_active++;
      }
    }
  } else {
    const double di_min = sort_j[0].d - hi_max - dx_max + rshift;

    if (rshift != 0.) {
      shift[0] = shift[0];
    }

    /* Loop over parts in ci that could interact with parts in cj */
    for (int pid = count_i - 1; pid >= 0 && sort_i[pid].d > di_min; pid--) {

      /* Found active particle? */
      if (part_is_active(&parts_i[sort_i[pid].i], e)) {
        sort_active_i[count_active] = sort_i[pid];
        count_active++;
      }
    }
  }

  if ((float)count_active / (float)count_i > 0.9f) {
    /* Do a naive pair interaction */
    runner_dopair_grid_naive(ci, cj, e, sort_i, sort_j, flipped, shift, rshift,
                             hi_max, dx_max, sid);
  } else {
    /* Do a smart pair interaction. (we only construct voronoi cells of active
     * particles). */
    runner_dopair_grid(ci, cj, e, sort_i, sort_j, sort_active_i,
                       count_active, flipped, shift, rshift, hi_max, dx_max,
                       sid);
  }

  /* Be clean */
  free(sort_active_i);

  TIMER_TOC(timer_dopair_grid_construction);
}

__attribute__((always_inline)) INLINE static void
runner_doself_grid_construction(struct runner *restrict r,
                                struct cell *restrict c) {

  TIMER_TIC;

  const struct engine *restrict e = r->e;

  /* Anything to do here? */
  if (c->hydro.count == 0) return;

  /* Is the cell active and local? */
  assert((cell_is_active_hydro(c, e) && c->nodeID == e->nodeID));

  /* Check that cells are drifted. */
  if (!cell_are_part_drifted(c, e)) error("Interacting undrifted cell.");

  /* Delaunay already allocated? */
  if (c->grid.delaunay == NULL) {
    c->grid.delaunay = delaunay_malloc(c->loc, c->width, c->hydro.count);
    c->grid.ti_old = e->ti_current;
  } else {
    /* Check if rebuild is needed */
    if (c->grid.ti_old < e->ti_current) {
      delaunay_reset(c->grid.delaunay, c->loc, c->width, c->hydro.count);
      c->grid.ti_old = e->ti_current;
    }
  }

  /* We are good to go!*/

  const int count = c->hydro.count;
  struct part *restrict parts = c->hydro.parts;

#ifdef SHADOWFAX_HILBERT_ORDERING
  /* Update hilbert keys + sort */
  cell_update_hilbert_keys(c);
  for (int i = 0; i < count; i++) {
    c->hydro.hilbert_r_sort[i] = i;
  }
  qsort_r(c->hydro.hilbert_r_sort, count, sizeof(int), sort_h_comp,
          c->hydro.hilbert_keys);
#endif

  /* Loop over the parts in c. */
  for (int i = 0; i < count; i++) {
#ifdef SHADOWFAX_HILBERT_ORDERING
    int idx = c->hydro.hilbert_r_sort[i];
#else
    int idx = i;
#endif
    /* Get a pointer to the idx-th particle. */
    struct part *restrict pi = &parts[idx];
    const double pix = pi->x[0];
    const double piy = pi->x[1];
    const double piz = pi->x[2];

    /* Add inactive particles only if they fall in the search radius of an
     * active particle */
    if (part_is_active(pi, e)) {
      delaunay_add_local_vertex(c->grid.delaunay, idx, pix, piy, piz);
    } else {
      /* pi is inactive, check if there is an active pj containing pi in its
       * search radius. */
      for (int j = 0; j < count; j++) {
        /* Get a pointer to the j-th particle. */
        struct part *restrict pj = &parts[j];
        if (!part_is_active(pj, e)) continue;
        const double rj = pj->h;

        /* Compute pairwise distance */
        const double dx[3] = {pj->x[0] - pix, pj->x[1] - piy, pj->x[2] - piz};
        const double r2 = dx[0] * dx[0] + dx[1] * dx[1] + dx[2] * dx[2];

        /* Hit or miss? */
        if (r2 < rj * rj) {
          delaunay_add_local_vertex(c->grid.delaunay, idx, pix, piy, piz);
          break;
        }
      }
    }
  }

  TIMER_TOC(timer_doself_grid_construction);
}

__attribute__((always_inline)) INLINE static void
runner_dopair_subset_grid_construction(struct runner *restrict r,
                                       struct cell *restrict ci,
                                       struct part *restrict parts_i,
                                       const int *restrict ind,
                                       const double *restrict r_prev,
                                       double r_max, int count,
                                       struct cell *restrict cj) {
  const struct engine *restrict e = r->e;

  const int count_j = cj->hydro.count;
  struct part *restrict parts_j = cj->hydro.parts;

  if (!cell_is_active_hydro(ci, e) && cj->nodeID == e->nodeID)
    error("Running construction task for inactive cell!");

  /* Get the sort ID. */
  double shift[3] = {0.0, 0.0, 0.0};
  struct cell *ci_temp = ci;
  struct cell *cj_temp = cj;
  int sid = space_getsid(e->s, &ci_temp, &cj_temp, shift);

  /* Pick-out the sorted lists. */
  const struct sort_entry *sort_i = cell_get_hydro_sorts(ci, sid);
  const struct sort_entry *sort_j = cell_get_hydro_sorts(cj, sid);

  /* Useful variables*/
  const float dx_max = (ci->hydro.dx_max_sort + cj->hydro.dx_max_sort);

  const int flipped = ci != ci_temp;
  if (flipped) {
    /* ci on the right */

    /* Correct sid if the cells have been flipped */
    sid = 26 - sid;

    if (shift[1] != 0) {
      shift[1] = shift[1];
    }

    /* Get the minimal position of any particle of ci along the sorting axis. */
    const float di_min = sort_i[0].d;

    /* Loop over the neighbouring particles parts_j until they are definitely
     * too far to be a candidate ghost particle. */
    for (int pjd = count_j - 1;
         pjd >= 0 && sort_j[pjd].d > di_min - dx_max - r_max; pjd--) {

      /* Get a pointer to the jth particle. */
      int pj_idx = sort_j[pjd].i;
      struct part *restrict pj = &parts_j[pj_idx];

      /* Skip inhibited particles. */
      if (part_is_inhibited(pj, e)) continue;

      /* Shift pj so that it is in the frame of ci (with cj on the left) */
      const double pjx = pj->x[0] - shift[0];
      const double pjy = pj->x[1] - shift[1];
      const double pjz = pj->x[2] - shift[2];

      /* Loop over all the unconverged particles in parts_i and check if pj
       * falls within the new search radius, but outside the old search radius
       * of the unconverged particle pi. */
      for (int pid = 0; pid < count; pid++) {

        /* Get a hold of the ith part in ci. */
        struct part *restrict pi = &parts_i[ind[pid]];
        const double ri = pi->h;
        const double ri_prev = r_prev[ind[pid]];

#ifdef SWIFT_DEBUG_CHECKS
        if (!part_is_active(pi, e)) {
          error(
              "Encountered inactive unconverged particle in ghost construction "
              "task!");
        }
#endif

        /* Compute the pairwise distance. */
        const double dx[3] = {pi->x[0] - pjx, pi->x[1] - pjy, pi->x[2] - pjz};
        const double r2 = dx[0] * dx[0] + dx[1] * dx[1] + dx[2] * dx[2];

        /* Hit or miss? */
        if (r2 < ri_prev * ri_prev) {
          /* pj already added during previous iteration */
          break;
        } else if (r2 < ri * ri) {
          delaunay_add_new_vertex(ci->grid.delaunay, pjx, pjy, pjz, sid,
                                  pj_idx);
          break;
        }
      } /* Loop over unconverged particles in ci */
    }   /* Loop over particles in cj */
  } else {
    /* ci on the left */

    /* Get the maximal position of any particle of ci along the sorting axis. */
    const float di_max = sort_i[ci->hydro.count - 1].d + ci->hydro.dx_max_sort;

    /* Loop over the neighbouring particles parts_j until they are definitely
     * too far to be a candidate ghost particle. */
    for (int pjd = 0; pjd < count_j && sort_j[pjd].d < di_max + r_max; pjd++) {

      /* Get a pointer to the jth particle. */
      int pj_idx = sort_j[pjd].i;
      struct part *restrict pj = &parts_j[pj_idx];

      /* Skip inhibited particles. */
      if (part_is_inhibited(pj, e)) continue;

      /* Shift pj so that it is in the frame of ci (with cj on the right) */
      const double pjx = pj->x[0] + shift[0];
      const double pjy = pj->x[1] + shift[1];
      const double pjz = pj->x[2] + shift[2];

      /* Loop over all the unconverged particles in parts_i and check if pj
       * falls within the new search radius, but outside the old search radius
       * of the unconverged particle pi. */
      for (int pid = 0; pid < count; pid++) {

        /* Get a hold of the ith part in ci. */
        struct part *restrict pi = &parts_i[ind[pid]];
        const double ri = pi->h;
        const double ri_prev = r_prev[ind[pid]];

#ifdef SWIFT_DEBUG_CHECKS
        if (!part_is_active(pi, e)) {
          error(
              "Encountered inactive unconverged particle in ghost construction "
              "task!");
        }
#endif

        /* Compute the pairwise distance. */
        const double dx[3] = {pi->x[0] - pjx, pi->x[1] - pjy, pi->x[2] - pjz};
        const double r2 = dx[0] * dx[0] + dx[1] * dx[1] + dx[2] * dx[2];

        /* Hit or miss? */
        if (r2 < ri_prev * ri_prev) {
          /* pj already added during previous iteration */
          break;
        } else if (r2 < ri * ri) {
          delaunay_add_new_vertex(ci->grid.delaunay, pjx, pjy, pjz, sid,
                                  pj_idx);
          break;
        }
      } /* Loop over unconverged particles in ci */
    }   /* Loop over particles in cj */
  }     /* Flipped? */
}

__attribute__((always_inline)) INLINE static void
runner_doself_subset_grid_construction(struct runner *restrict r,
                                       struct cell *restrict ci,
                                       struct part *restrict parts_i,
                                       const int *restrict ind,
                                       const double *restrict r_prev,
                                       int count) {
  struct engine *e = r->e;

  /* Loop over all inactive particles in ci */
  for (int j = 0; j < ci->hydro.count; j++) {

    /* Retrieve particle */
    struct part *pj = &parts_i[j];
    const double pjx = pj->x[0];
    const double pjy = pj->x[1];
    const double pjz = pj->x[2];

    /* Skip active particles (already added during the self task) */
    if (part_is_active(pj, e)) continue;

    /* Loop over all unconverged particles */
    for (int i = 0; i < count; i++) {

      /* Retrieve particle */
      const int pid = ind[i];
      struct part *pi = &parts_i[pid];
      const double ri = pi->h;
      const double ri_prev = r_prev[pid];

      /* Compute pairwise distance */
      const double dx[3] = {pi->x[0] - pjx, pi->x[1] - pjy, pi->x[2] - pjz};
      const double r2 = dx[0] * dx[0] + dx[1] * dx[1] + dx[2] * dx[2];

      /* Hit or miss? */
      if (r2 < ri_prev * ri_prev) {
        /* pj already added during previous iteration */
        break;
      } else if (r2 < ri * ri) {
        delaunay_add_local_vertex(ci->grid.delaunay, pid, pjx, pjy, pjz);
        break;
      }
    }
  }
}

#else
/* No grid construction needed */

__attribute__((always_inline)) INLINE static void
runner_dopair_grid_construction(struct runner *restrict r,
                                struct cell *restrict ci,
                                struct cell *restrict cj) {}

__attribute__((always_inline)) INLINE static void
runner_doself_grid_construction(struct runner *restrict r,
                                struct cell *restrict c) {}

__attribute__((always_inline)) INLINE static void
runner_dopair_subset_grid_construction(struct runner *restrict r,
                                       struct cell *restrict ci,
                                       struct part *restrict parts_i,
                                       const int *restrict ind,
                                       const double *restrict r_prev,
                                       double r_max, int count,
                                       struct cell *restrict cj) {}

__attribute__((always_inline)) INLINE static void
runner_doself_subset_grid_construction(struct runner *restrict r,
                                       struct cell *restrict ci,
                                       struct part *restrict parts_i,
                                       const int *restrict ind,
                                       const double *restrict r_prev,
                                       int count) {}

#endif

#endif  // SWIFTSIM_RUNNER_DOIACT_GRID_H
