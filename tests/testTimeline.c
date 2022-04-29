/*******************************************************************************
 * This file is part of SWIFT.
 * Copyright (C) 2022 Mladen Ivkovic (mladen.ivkovic@hotmail.com)
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
#include "../config.h"

#include "timeline.h"
#include "tools.h"

/* Local headers. */
/* #include <string.h> */

/* Local includes */
/* #include "error.h" */
/* #include "riemann/riemann_trrs.h" */
/* #include "tools.h" */


#define NREPEAT 1000000

/**
 * @brief Check the timeline functions.
 */
int main(int argc, char* argv[]) {


  integertime_t dt, max_step, set_time_end, current_time, time_end_recovered;

  /* Pick a random time bin */
  /* bin = (int) random_uniform(1., (double) num_time_bins); */
  for (int bin = 1; bin < num_time_bins; bin++){
    printf("Running bin %d\n", bin);

    dt = get_integer_timestep(bin);

    for (int r = 0; r < NREPEAT; r++) {
      /* Now pick a place to set this time_end on the timeline. */
      /* we can't have more than this many steps of this size */
      max_step = max_nr_timesteps / dt; 
      /* Set the time_end at any step in between there */
      set_time_end = (integertime_t) (random_uniform(max(0, max_step - 2) , max_step) * dt);
      if (set_time_end > max_nr_timesteps) error("What?");
      /* Now mimick a "current time" by removing a fraction of dt from 
       * the step, and see whether we recover the correct time_end */
      current_time = set_time_end - (integertime_t) (random_uniform(0., 1.) * dt);
      if (current_time == set_time_end) message("What 2? %lld %lld %lld", current_time, set_time_end, dt);
      if (current_time == set_time_end) message("What 3? %lld ",  (integertime_t) (random_uniform(0., 1.) * dt));
      time_end_recovered = get_integer_time_end(current_time, bin);

      if (time_end_recovered != set_time_end) error("Oh no");
    }

  }


  return 0;
}
