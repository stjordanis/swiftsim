/*******************************************************************************
 * This file is part of SWIFT.
 * Copyright (c) 2016 Tom Theuns (tom.theuns@durham.ac.uk)
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
#ifndef SWIFT_SN_FEEDBACK_H
#define SWIFT_SN_FEEDBACK_H
#include <float.h>
#include "equation_of_state.h"
#include "hydro.h"
/* determine whether location is in cell */
__attribute__((always_inline)) INLINE static int is_in_cell(
    const double cell_min[], const double cell_width[], const double location[],
    const int dimen) {
  for (int i = 0; i < dimen; i++) {
    if (cell_min[i] > location[i]) return 0;
    if ((cell_min[i] + cell_width[i]) <= location[i]) return 0;
  }
  return 1;
};

/**
 * @file src/sourceterms/sn_feedback.h
 * @brief Routines related to source terms (supernova feedback)
 * @param sourceterms the structure describing the source terms properties
 * @param p the particle to apply feedback to
 *
 * This routine heats an individual particle (p), increasing its thermal energy per unit mass 
 *      by supernova energy / particle mass.
 */
__attribute__((always_inline)) INLINE static void do_supernova_feedback(
    const struct sourceterms* sourceterms, struct part* p) {
  const float u_old = hydro_get_internal_energy(p, 0);
  message(" u_old= %e entropy= %e", u_old, p->entropy);
  const float u_new = u_old + sourceterms->supernova.energy / hydro_get_mass(p);
  hydro_set_internal_energy(p, u_new);
  const float u_set = hydro_get_internal_energy(p, 0.0);
  message(" unew = %e %e s= %e", u_new, u_set, p->entropy);
  message(" injected SN energy in particle = %lld, increased energy from %e to %e, check= %e", p->id, u_old, u_new, u_set);
  
};
#endif /* SWIFT_SN_FEEDBACK_H */
