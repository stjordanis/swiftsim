/*******************************************************************************
 * This file is part of SWIFT.
 * Copyright (c) 2018 James Willis (james.s.willis@durham.ac.uk)
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
#ifndef SWIFT_FOF_H
#define SWIFT_FOF_H

/* Config parameters. */
#include <config.h>

/* Local headers */
#include "align.h"
#include "parser.h"

/* Avoid cyclic inclusions */
struct cell;
struct gpart;
struct space;
struct engine;
struct unit_system;
struct phys_const;
struct black_holes_props;
struct cosmology;

/**
 * @brief What kind of halo search are we doing?
 *
 * 0 = A 3D FOF group.
 * 1 = A 6D Host halo.
 * 2 = A 6D Subhalo
 */
enum halo_types {fof_group, host_halo, sub_halo};

struct fof_props {

  /*! Whether we're doing periodic FoF calls to seed black holes. */
  int seed_black_holes_enabled;

  /* ----------- Parameters of the FOF search ------- */

  /*! The linking length in units of the mean DM inter-particle separation. */
  double l_x_ratio;

  /*! The absolute linking length in internal units if the user wants to
   *   overwrite the one based on the mean inter-particle separation. */
  double l_x_absolute;

  /*! The square of the linking length. */
  double l_x2;

  /*! The minimum halo mass for black hole seeding. */
  double seed_halo_mass;

  /*! Minimal number of particles in a group */
  size_t min_group_size;

  /*! Default group ID to give to particles not in a group */
  size_t group_id_default;

  /*! ID of the first (largest) group. */
  size_t group_id_offset;

  /*! The base name of the output file */
  char base_name[PARSER_MAX_LINE_SIZE];

  /*! Current search level in halo hierarchy. */
  enum halo_types current_level;

  /* ------------  Group properties ----------------- */

  /*! Number of groups */
  long long num_groups;

  /*! Number of local black holes that belong to groups whose roots are on a
   * different node. */
  int extra_bh_seed_count;

  /*! Index of the root particle of the group a given gpart belongs to. */
  size_t *group_index;

  /*! Size of the group a given gpart belongs to. */
  size_t *group_size;

  /*! Mass of the group a given gpart belongs to. */
  double *group_mass;

  /*! Centre of mass of the group a given gpart belongs to. */
  double *group_centre_of_mass;

  /*! Position of the first particle of a given group. */
  double *group_first_position;

  /*! Index of the part with the maximal density of each group. */
  long long *max_part_density_index;

  /*! Maximal density of all parts of each group. */
  float *max_part_density;

  /* ------------ MPI-related arrays --------------- */

  /*! The number of links between pairs of particles on this node and
   * a foreign node */
  int group_link_count;

  /*! The allocated size of the links array */
  int group_links_size;

  /*! The links between pairs of particles on this node and a foreign
   * node */
  struct fof_mpi *group_links;

#ifdef WITH_HALO_FINDER
  /* ----------------- General metadata ----------------- */
  
  /*! Are we finding subhalos? */
  int find_subhalos;

  /* ----------- Parameters of the configuration space search ------- */

  /*! The subhalo linking length in units of the mean DM inter-particle
   *  separation. */
  double sub_l_x_ratio;

  /*! The host halo linking length. */
  double l_x;

  /*! The subhalo linking length. */
  double sub_l_x;

  /*! The square of the subhalo linking length. */
  double sub_l_x2;

   /* ----------- Parameters of the velocity space search ------- */

  /*! Halo independent part of the host velocity space linking length.  */
  double const_l_v;

  /*! Halo independent part of the subhalo velocity space linking length.  */
  double sub_const_l_v;

  /* For Future use. */
  /*! The overdensity guess for host halos. */
  double host_overdensity;
  
  /*! The overdensity guess for host halos. */
  double subhalo_overdensity;

  /*! Cube root of the overdensity ratio (the difference in spatial "scale"
   *  between over density levels)  */
  double overdensity_ratio;

  /*! Initial velocity space linking length coefficient. */
  double ini_l_v_coeff;

  /*! Minimum allowed velocity space linking length coefficient. */
  double min_l_v_coeff;

  /* ------------  Group/Host/Subhalo properties ----------------- */

  /*! Index of the root particle of the group a given gpart belongs to.
   *  Used when the other is overwritten with a nr_group version. */
  size_t *part_group_index;
  
  /*! Number of groups on this rank. */
  long long num_groups_rank;

  /*! Number of particles in groups */
  long long num_parts_in_groups;

  /*! Number of hosts */
  long long num_hosts;

  /*! Number of particles in hosts */
  long long num_parts_in_hosts;

  /*! Number of subhalos */
  long long num_subhalos;

  /*! Number of particles in subhalos */
  long long num_parts_in_subhalos;

  /*! Number of hosts on this rank. */
  long long num_hosts_rank;

  /*! Number of subhalos on this rank. */
  long long num_subhalos_rank;

  /*! The groups mass weighted bulk velocity. */
  double *group_velocity;

  /*! The groups kinetic energy. */
  double *group_kinetic_energy;

  /*! The groups binding energy. */
  double *group_binding_energy;

  /*! The groups maximum extent along each dimension. */
  double *group_extent;

  /*! The groups width along each dimension. */
  double *group_width;

  /*! Index of the root particle of the host a given gpart belongs to. */
  size_t *host_index;

  /*! Index of the root particle of the group a given gpart belongs to.
   *  Used when the other is overwritten with a nr_group version. */
  size_t *part_host_index;

  /*! Size of the host a given gpart belongs to. */
  size_t *host_size;

  /*! Mass of the host a given gpart belongs to. */
  double *host_mass;

  /*! Centre of mass of the host a given gpart belongs to. */
  double *host_centre_of_mass;

  /*! Position of the first particle of a given host. */
  double *host_first_position;

  /*! The hosts mass weighted bulk velocity. */
  double *host_velocity;

  /*! The hosts kinetic energy. */
  double *host_kinetic_energy;

  /*! The hosts binding energy. */
  double *host_binding_energy;

  /*! The hosts maximum extent along each dimension. */
  double *host_extent;

  /*! The hosts width along each dimension. */
  double *host_width;

  /*! Index of the root particle of the subhalo a given gpart belongs to. */
  size_t *subhalo_index;

  /*! Size of the subhalo a given gpart belongs to. */
  size_t *subhalo_size;

  /*! Mass of the subhalo a given gpart belongs to. */
  double *subhalo_mass;

  /*! Centre of mass of the subhalo a given gpart belongs to. */
  double *subhalo_centre_of_mass;

  /*! Position of the first particle of a given subhalo. */
  double *subhalo_first_position;

  /*! The subhalos mass weighted bulk velocity. */
  double *subhalo_velocity;

  /*! The hosts kinetic energy. */
  double *subhalo_kinetic_energy;

  /*! The hosts binding energy. */
  double *subhalo_binding_energy;

  /*! The host of each subhalo. */
  size_t *subhalo_host_id;

  /*! The subhalos maximum extent along each dimension. */
  double *subhalo_extent;

  /*! The subhalos width along each dimension. */
  double *subhalo_width;

  /* ------------  Group/Host/Subhalo paritle information ----------------- */

  /*! Indices of group particles. */
  size_t *group_particle_inds;

  /*! Indices of host particles. */
  size_t *host_particle_inds;

  /*! Indices of subhalo particles. */
  size_t *subhalo_particle_inds;

  /*! Positions of group particles. */
  double *group_particle_pos;

  /*! Positions of host particles. */
  double *host_particle_pos;

  /*! Positions of subhalo particles. */
  double *subhalo_particle_pos;

  /*! First index of group particles. */
  size_t *group_start;

  /*! First index of group particles. */
  size_t *host_start;
  
  /*! First index of group particles. */
  size_t *subhalo_start;

  
#endif
};

/* Store group size and offset into array. */
struct group_length {

  size_t index, size;

} SWIFT_STRUCT_ALIGN;

#ifdef WITH_MPI

/* MPI message required for FOF. */
struct fof_mpi {

  /* The local particle's root ID.*/
  size_t group_i;

  /* The local group's size.*/
  size_t group_i_size;

  /* The foreign particle's root ID.*/
  size_t group_j;

  /* The local group's size.*/
  size_t group_j_size;
};

/* Struct used to find final group ID when using MPI */
struct fof_final_index {
  size_t local_root;
  size_t global_root;
};

/* Struct used to find the total mass of a group when using MPI */
struct fof_final_mass {
  size_t global_root;
  double group_mass;
  double first_position[3];
  double centre_of_mass[3];
  long long max_part_density_index;
  float max_part_density;
};

/* Struct used to iterate over the hash table and unpack the mass fragments of a
 * group when using MPI */
struct fof_mass_send_hashmap {
  struct fof_final_mass *mass_send;
  size_t nsend;
};

/* Store local and foreign cell indices that touch. */
struct cell_pair_indices {
  struct cell *local, *foreign;
};
#endif

/* Function prototypes. */
void fof_init(struct fof_props *props, struct swift_params *params,
              const struct phys_const *phys_const, const struct unit_system *us,
              const int stand_alone_fof);
void fof_create_mpi_types(void);
void fof_allocate(const struct space *s, const long long total_nr_DM_particles,
                  struct fof_props *props);
void fof_search_tree(struct fof_props *props,
                     const struct black_holes_props *bh_props,
                     const struct phys_const *constants,
                     const struct cosmology *cosmo, struct space *s,
                     const int dump_results, const int dump_debug_results,
                     const int seed_black_holes);
void group_search_tree(struct fof_props *props,
                     const struct black_holes_props *bh_props,
                     const struct phys_const *constants,
                     const struct cosmology *cosmo, struct space *s,
                     const int dump_results, const int dump_debug_results,
                     const int seed_black_holes, const int is_halo_finder);
void host_search_tree(struct fof_props *props,
                      const struct phys_const *constants,
                      const struct cosmology *cosmo, struct space *s,
                      const int dump_results, const int dump_debug_results);
void subhalo_search_tree(struct fof_props *props,
                         const struct phys_const *constants,
                         const struct cosmology *cosmo, struct space *s,
                         const int dump_results, const int dump_debug_results);
void rec_fof_search_self(const struct fof_props *props, const double dim[3],
                         const double search_r2,
                         const enum halo_types halo_level,
                         const struct cosmology *cosmo,
                         const int periodic,
                         const struct gpart *const space_gparts,
                         struct cell *c);
void rec_fof_search_pair(const struct fof_props *props, const double dim[3],
                         const double search_r2,
                         const enum halo_types halo_level,
                         const struct cosmology *cosmo,
                         const int periodic,
                         const struct gpart *const space_gparts,
                         struct cell *restrict ci, struct cell *restrict cj);
void fof_calc_group_kinetic_nrg(struct fof_props *props, const struct space *s,
                                const struct cosmology *cosmo, struct cell *c);
void fof_calc_group_binding_nrg_self(struct fof_props *props,
                                     const struct space *s,
                                     const struct cosmology *cosmo,
                                     struct cell *c);
void fof_calc_group_binding_nrg_pair(struct fof_props *props,
                                     const struct space *s,
                                     const struct cosmology *cosmo,
                                     struct cell *ci, struct cell *cj);
void fof_struct_dump(const struct fof_props *props, FILE *stream);
void fof_struct_restore(struct fof_props *props, FILE *stream);
void fof_set_initial_group_index_mapper(void *map_data, int num_elements,
                                        void *extra_data);
void fof_set_initial_group_size_mapper(void *map_data, int num_elements,
                                        void *extra_data);

/* Halo finder prototypes */
void halo_finder_search_self_cell_gpart(const struct fof_props *props,
                                        const struct cosmology *cosmo,
                                        const struct gpart *const space_gparts,
                                        struct cell *c);
void halo_finder_search_pair_cells_gpart(const struct fof_props *props,
                                         const double dim[3],
                                         const struct cosmology *cosmo,
                                         const int periodic,
                                         const struct gpart *const space_gparts,
                                         struct cell *restrict ci,
                                         struct cell *restrict cj);

#ifdef WITH_MPI
/* MPI data type for the particle transfers */
extern MPI_Datatype fof_mpi_type;
extern MPI_Datatype group_length_mpi_type;
#endif

#endif /* SWIFT_FOF_H */
