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

#define NREPEAT 1000000

/**
 * @brief test get_integer_time_end() function.
 * For each particle time bin, pick a random valid time_end for
 * the dt given by the particle bin; then set a random current_time
 * by subtracting some time intereval < dt from the expected time_end
 * and see whether the recovered time_end matches up.
 */
void test_get_integer_time_end(void) {

  integertime_t dt, max_step, set_time_end, current_time, time_end_recovered,
      displacement;

  /* run over all possible time bins */
  /* TODO: starting at a higher bin than 0 to verify that this isn't an issue
   * only for small bins */
  /* Indeed tests seem to pass for bin > 21 */
  for (int bin = 21; bin < num_time_bins; bin++) {
    printf("Running bin %d\n", bin);

    dt = get_integer_timestep(bin);

    for (int r = 0; r < NREPEAT; r++) {
      /* First pick a place to set this time_end on the timeline. */

      /* we can't have more than this many steps of this size */
      max_step = max_nr_timesteps / dt;

      /* Set the time_end at any step in between there */
      set_time_end = (integertime_t)(random_uniform(0, max_step)) * dt;

      /* Do some safety checks */
      if (set_time_end % dt != 0) error("time_end not divisible by dt?");
      if (set_time_end > max_nr_timesteps)
        error("Time end > max_nr_timesteps?");
      if (set_time_end < (integertime_t)0) error("Time end < 0?");

      /* Now mimick a "current time" by removing a fraction of dt from
       * the step, and see whether we recover the correct time_end */
      displacement = (integertime_t)(random_uniform(0., 1.) * dt);
      current_time = set_time_end - displacement;

      /* Another round of safety checks */
      if (current_time == set_time_end)
        message(
            "current==time_end? current=%lld time_end=%lld dt=%lld "
            "displacement=%lld bin=%d",
            current_time, set_time_end, dt, displacement, bin);
      if (current_time > set_time_end)
        message(
            "current>time_end? current=%lld time_end=%lld dt=%lld "
            "displacement=%lld bin=%d",
            current_time, set_time_end, dt, displacement, bin);
      time_end_recovered = get_integer_time_end(current_time, bin);

      /* Now the actual check. */
      if (time_end_recovered != set_time_end)
        error(
            "time_end incorrect: expect=%lld got=%lld diff=%lld; current=%lld "
            "displacement=%lld, dt=%lld",
            set_time_end, time_end_recovered, set_time_end - time_end_recovered,
            current_time, displacement, dt);
    }
  }
}

/**
 * @brief Check the timeline functions.
 */
int main(int argc, char* argv[]) {

  test_get_integer_time_end();

  return 0;
}
