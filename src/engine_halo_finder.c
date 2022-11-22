/*******************************************************************************
 * This file is part of SWIFT.
 * Copyright (c) 2012 Pedro Gonnet (pedro.gonnet@durham.ac.uk)
 *                    Matthieu Schaller (schaller@strw.leidenuniv.nl)
 *               2015 Peter W. Draper (p.w.draper@durham.ac.uk)
 *               2022 Will Roper (w.roper@sussex.ac.uk)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 ******************************************************************************/
/* Config parameters. */
#include <config.h>

/* Includes. */
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* This object's header. */
#include "engine.h"

/* Local includes. */
#include "cell.h"
#include "fof.h"
#include "halo.h"

/**
 * @brief Allocate the memory and initialise the arrays for a FOF calculation.
 *
 * @param s The #space to act on.
 * @param total_nr_DM_particles The total number of DM particles in the
 * simulation.
 * @param props The properties of the FOF structure.
 * @param props Are we running the halo finder?
 */
void halo_finder_allocate(const struct space *s,
                          const long long total_nr_DM_particles,
                          struct fof_props *props, int run_halo_finder) {

  const int verbose = s->e->verbose;
  const ticks total_tic = getticks();

  /* Get halo finder properties. */
  const halo_finder_props hf_props = s->halo_finder_properties;

  /* Start by computing the mean inter DM particle separation */

  /* Collect the mass of the first non-background gpart */
  double high_res_DM_mass = 0.;
  for (size_t i = 0; i < s->nr_gparts; ++i) {
    const struct gpart *gp = &s->gparts[i];
    if (gp->type == swift_type_dark_matter &&
        gp->time_bin != time_bin_inhibited &&
        gp->time_bin != time_bin_not_created) {
      high_res_DM_mass = gp->mass;
      break;
    }
  }

  /* Are we using the aboslute value or the one derived from the mean
     inter-particle sepration? */
  if (props->l_x_absolute != -1.) {
    props->l_x2 = props->l_x_absolute * props->l_x_absolute;

    /* Assign the linking lengths to the halo finder properties. */
    hf_props->l_x = props->l_x_absolute;
    hf_props->l_x2 = props->l_x2;

    if (s->e->nodeID == 0) 
      message("Host linking length is set to %e [internal units].",
              props->l_x_absolute);

    /* Define the subhalo linking length based on overdensity ratio. */
    if (hf_props->find_subhalos) {
      hf_props->sub_l_x = hf_props->overdensity_ratio * props->l_x_absolute;
      hf_props->sub_l_x2 = props->l_x2;

      if (s->e->nodeID == 0)
        message("Subhalo linking length is set to %e [internal units].",
                props->l_x_absolute);
    }

  } else {

    /* Safety check */
    if (!(s->e->policy & engine_policy_cosmology))
      error(
          "Attempting to run FoF on a simulation using cosmological "
          "information but cosmology was not initialised");

    /* Calculate the mean inter-particle separation as if we were in
       a scenario where the entire box was filled with high-resolution
         particles */
    const double Omega_cdm = s->e->cosmology->Omega_cdm;
    const double Omega_b = s->e->cosmology->Omega_b;
    const double Omega_m = Omega_cdm + Omega_b;
    const double critical_density_0 = s->e->cosmology->critical_density_0;
    double mean_matter_density;
    if (s->with_hydro)
      mean_matter_density = Omega_cdm * critical_density_0;
    else
      mean_matter_density = Omega_m * critical_density_0;

    /* Mean inter-particle separation of the DM particles */
    const double mean_inter_particle_sep =
        cbrt(high_res_DM_mass / mean_matter_density);

    /* Calculate the particle linking length based upon the mean inter-particle
     * spacing of the DM particles. */
    const double l_x = props->l_x_ratio * mean_inter_particle_sep;

    props->l_x2 = l_x * l_x;

    if (s->e->nodeID == 0)
      message(
          "Host linking length is set to %e [internal units] (%f of mean "
          "inter-DM-particle separation).",
          l_x, props->l_x_ratio);

    /* Assign the linking lengths to the halo finder properties. */
    hf_props->l_x = l_x;
    hf_props->l_x2 = props->l_x2;

    /* Define the subhalo linking length based on overdensity ratio. */
    if (run_halo_finder && hf_props->find_subhalos) {
      hf_props->sub_l_x = hf_props->overdensity_ratio * l_x;
      hf_props->sub_l_x2 = props->l_x2;

      if (s->e->nodeID == 0)
      message(
          "Subhalo Linking length is set to %e [internal units] (%f of mean "
          "inter-DM-particle separation).",
          l_x, props->l_x_ratio);
    }
  }

  /* Now compute the velocity space linking lengths. */
  const double newt_G = s->e->physical_constants->const_newton_G;
  hf_props->const_l_v = sqrt(newt_G / 2) *
    pow((4 * M_PI * hf_props->host_overdensity * mean_matter_density) / 3,
        1.0 / 6.0);
  hf_props->sub_const_l_v = sqrt(newt_G / 2) *
    pow((4 * M_PI * hf_props->subhalo_overdensity * mean_matter_density) / 3,
        1.0 / 6.0);

#ifdef WITH_MPI
  /* Check size of linking length against the top-level cell dimensions. */
  if (props->l_x2 > s->width[0] * s->width[0])
    error(
        "Linking length greater than the width of a top-level cell. Need to "
        "check more than one layer of top-level cells for links.");
#endif

  /* Allocate and initialise a group index array. */
  if (swift_memalign("fof_group_index", (void **)&props->group_index, 64,
                     s->nr_gparts * sizeof(size_t)) != 0)
    error("Failed to allocate list of particle group indices for FOF search.");

  /* Allocate and initialise a group size array. */
  if (swift_memalign("fof_group_size", (void **)&props->group_size, 64,
                     s->nr_gparts * sizeof(size_t)) != 0)
    error("Failed to allocate list of group size for FOF search.");

  /* Allocate and initialise a group index array. */
  if (swift_memalign("fof_host_index", (void **)&props->host_index, 64,
                     s->nr_gparts * sizeof(size_t)) != 0)
    error("Failed to allocate list of particle host indices for FOF search.");

  /* Allocate and initialise a group size array. */
  if (swift_memalign("fof_host_size", (void **)&props->host_size, 64,
                     s->nr_gparts * sizeof(size_t)) != 0)
    error("Failed to allocate list of host size for FOF search.");

  if (hf_props->find_subhalos) {
    /* Allocate and initialise a group index array. */
    if (swift_memalign("fof_subhalo_index", (void **)&props->subhalo_index, 64,
                       s->nr_gparts * sizeof(size_t)) != 0)
      error("Failed to allocate list of particle subhalo indices for FOF search.");

    /* Allocate and initialise a group size array. */
    if (swift_memalign("fof_subhalo_size", (void **)&props->subhalo_size, 64,
                       s->nr_gparts * sizeof(size_t)) != 0)
      error("Failed to allocate list of subhalo size for FOF search.");
  }

  ticks tic = getticks();

  /* Set initial group index */
  threadpool_map(&s->e->threadpool, fof_set_initial_group_index_mapper,
                 props->group_index, s->nr_gparts, sizeof(size_t),
                 threadpool_auto_chunk_size, props->group_index);

  if (verbose)
    message("Setting initial group index took: %.3f %s.",
            clocks_from_ticks(getticks() - tic), clocks_getunit());

  tic = getticks();

  /* Set initial host index */
  threadpool_map(&s->e->threadpool, fof_set_initial_group_index_mapper,
                 props->host_index, s->nr_gparts, sizeof(size_t),
                 threadpool_auto_chunk_size, props->host_index);

  if (verbose)
    message("Setting initial host index took: %.3f %s.",
            clocks_from_ticks(getticks() - tic), clocks_getunit());

  if (hr_props->find_subhalos) {
    tic = getticks();

    /* Set initial group index */
    threadpool_map(&s->e->threadpool, fof_set_initial_group_index_mapper,
                   props->subhalo_index, s->nr_gparts, sizeof(size_t),
                   threadpool_auto_chunk_size, props->subhalo_index);

    if (verbose)
      message("Setting initial subhalo index took: %.3f %s.",
              clocks_from_ticks(getticks() - tic), clocks_getunit());
  }

  tic = getticks();

  /* Set initial group sizes */
  threadpool_map(&s->e->threadpool, fof_set_initial_group_size_mapper,
                 props->group_size, s->nr_gparts, sizeof(size_t),
                 threadpool_auto_chunk_size, NULL);

  if (verbose)
    message("Setting initial group sizes took: %.3f %s.",
            clocks_from_ticks(getticks() - tic), clocks_getunit());

    tic = getticks();

  /* Set initial host index */
  threadpool_map(&s->e->threadpool, fof_set_initial_group_size_mapper,
                 props->host_size, s->nr_gparts, sizeof(size_t),
                 threadpool_auto_chunk_size, NULL);

  if (verbose)
    message("Setting initial host sizes took: %.3f %s.",
            clocks_from_ticks(getticks() - tic), clocks_getunit());

  if (hr_props->find_subhalos) {
    tic = getticks();

    /* Set initial group index */
    threadpool_map(&s->e->threadpool, fof_set_initial_group_size_mapper,
                   props->subhalo_size, s->nr_gparts, sizeof(size_t),
                   threadpool_auto_chunk_size, NULL);

    if (verbose)
      message("Setting initial subhalo sizes took: %.3f %s.",
              clocks_from_ticks(getticks() - tic), clocks_getunit());
  }

#ifdef SWIFT_DEBUG_CHECKS
  ti_current = s->e->ti_current;
#endif

  if (verbose)
    message("took %.3f %s.", clocks_from_ticks(getticks() - total_tic),
            clocks_getunit());
}

/**
 * @brief Run a halo finder search.
 *
 * @param e the engine
 * @param dump_results Are we writing group catalogues to output files?
 * @param dump_debug_results Are we writing a txt-file debug catalogue
 * (including BH seed info)?
 * @param seed_black_holes Are we seeding black holes?
 * @param foreign_buffers_allocated Are the foreign buffers currently
 * allocated?
 */
void engine_halo_finder(struct engine *e, const int dump_results,
                        const int dump_debug_results,
                        const int seed_black_holes,
                        const int foreign_buffers_allocated) {

#ifdef WITH_FOF

  const ticks tic = getticks();

  /* Start by cleaning up the foreign buffers */
  if (foreign_buffers_allocated) {
#ifdef WITH_MPI
    space_free_foreign_parts(e->s, /*clear pointers=*/1);
#endif
  }

  /* Compute number of DM particles */
  const long long total_nr_baryons =
      e->total_nr_parts + e->total_nr_sparts + e->total_nr_bparts;
  const long long total_nr_dmparts =
      e->total_nr_gparts - e->total_nr_DM_background_gparts -
      e->total_nr_neutrino_gparts - total_nr_baryons;

  /* Initialise FOF parameters and allocate FOF arrays. */
  halo_finder_allocate(e->s, total_nr_dmparts, e->fof_properties);

  /* TODO: (WILL) Fairly sure all the tasks could be done in the same call with
   * group->host->subhalos unlocks and a single clean up at the end.
   * This would break if we go iterative though, iteration would have to rerun
   * host and subhalo steps after clean up. */

  /* ---------------- First do the spatial FOF ---------------- */

  /* Make FOF tasks */
  engine_make_fof_tasks(e);

  /* and activate them. */
  engine_activate_fof_tasks(e);

  /* Print the number of active tasks ? */
  if (e->verbose) engine_print_task_counts(e);

  /* Perform local FOF tasks. */
  engine_launch(e, "fof");

  /* Perform FOF search over foreign particles. */
  fof_search_tree(e->fof_properties, e->black_holes_properties,
                  e->physical_constants, e->cosmology, e->s, dump_results,
                  dump_debug_results, /*seed_black_holes*/0);

  /* ---------------- Run 6D host FOF ---------------- */

  /* Make the host halo tasks */
  engine_make_host_tasks(e);

  /* and activate them. */
  engine_activate_fof_tasks(e);

  /* Print the number of active tasks ? */
  if (e->verbose) engine_print_task_counts(e);

  /* Perform local host tasks. */
  engine_launch(e, "fof");

  /* Perform host search over foreign particles. */
  fof_search_tree(e->fof_properties, e->black_holes_properties,
                  e->physical_constants, e->cosmology, e->s, dump_results,
                  dump_debug_results, /*seed_black_holes*/0);

  /* ---------------- Repeat for the subhalos ---------------- */

  if (e->fof_properties->find_subhalos) {
    
    /* Make the subhalo halo tasks */
    engine_make_subhalo_tasks(e);

    /* and activate them. */
    engine_activate_fof_tasks(e);

    /* Print the number of active tasks ? */
    if (e->verbose) engine_print_task_counts(e);

    /* Perform local host tasks. */
    engine_launch(e, "fof");

    /* Perform host search over foreign particles. */
    fof_search_tree(e->fof_properties, e->black_holes_properties,
                    e->physical_constants, e->cosmology, e->s, dump_results,
                    dump_debug_results, /*seed_black_holes*/0);
  }

  /* Restore the foreign buffers as they were*/
  if (foreign_buffers_allocated) {
#ifdef WITH_MPI
    engine_allocate_foreign_particles(e, /*fof=*/0);
#endif
  }

  if (engine_rank == 0)
    message("Complete FOF search took: %.3f %s.",
            clocks_from_ticks(getticks() - tic), clocks_getunit());
#else
  error("SWIFT was not compiled with FOF enabled!");
#endif
}