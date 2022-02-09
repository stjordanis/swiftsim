/*******************************************************************************
 * This file is part of SWIFT.
 * Copyright (c) 2022 Mladen Ivkovic (mladen.ivkovic@hotmail.com)
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

#include "rt_reschedule.h"

/**
 * @file src/rt_reschedule.c
 * @brief rescheduling of RT tasks for subcycling
 */

/**
 * @brief Set a task that has been run to a state where it can
 * run again
 * TODO: docs
 */
void rt_reschedule_task(struct engine *e, struct task *t, struct cell *c,
                        int wait, int callloc) {
  /* TODO: remove cell parameter later . possibly engine too? */

  if (t == NULL) return;

  if (t->wait != 0)
    message(
        "Got t->wait = %d? Should be zero when rescheduling %s, callloc %d "
        "cellID %lld",
        t->wait, taskID_names[t->type], callloc, c->cellID);
  int w, s;
  if ((w = atomic_cas(&t->wait, 0, wait)) != 0)
    error(
        "Got t->wait = %d? Should be zero when rescheduling %s, callloc %d "
        "cellID %lld",
        w, taskID_names[t->type], callloc, c->cellID);
  if ((s = atomic_cas(&t->skip, 1, 0)) != 1)
    error(
        "ERROR - Trying to reschedule a task with skip = %d; Should be 1 when "
        "rescheduling ; task %s cell %lld cycle %d",
        s, taskID_names[t->type], t->ci->cellID, t->ci->hydro.rt_cycle);
}

/**
 * @brief Re-schedule the entire RT cycle (that does actual work)
 * for the given cell.
 * @param r The #runner thread.
 * @param c The #cell.
 * @return 1 if done, 0 if not.
 */
int rt_reschedule(struct runner *r, struct cell *c) {
  struct engine *e = r->e;
  if (!e->subcycle_rt) return 0;

  /* increment the counter */
  c->hydro.rt_cycle += 1;
  celltrace(c->cellID, "running rt_reschedule cycle %d", c->hydro.rt_cycle-1);

  if (c->hydro.rt_cycle > RT_RESCHEDULE_MAX) {
    error("trying to subcycle too much?");
  } 
  /* else { */
    /* Conditionally re-schedule the requeue task. On the first cycle
     * (before the first re-cycle), the task should be unskipped already.
     * We need to do this too even if we're at the final cycle. */
    /* struct task *rt_requeue = c->hydro.rt_requeue; */
    /* if (rt_requeue->skip == 1) { */
      /* rt_reschedule_task(e, rt_requeue, c, wait =1, callloc=6); */
    /* } else { */
    /*   if (c->hydro.rt_cycle != 1) */
    /*     error("Requeue not skipped cell %lld cycle %d skip %d | %d", c->cellID, */
    /*           c->hydro.rt_cycle, rt_requeue->skip, c->hydro.parts[0].rt_data.debug_thermochem_done); */
    /* } */

  else if (c->hydro.rt_cycle == RT_RESCHEDULE_MAX) {
    /* We're done with the subcycling. */

    celltrace(c->cellID, "cycle %d - not rescheduling", c->hydro.rt_cycle);
    /* Reset and stop the rescheduling. */
    c->hydro.rt_cycle = 0;
    return 0;
  } 
  else {
    /* Set all RT tasks that do actual work back to a re-queueable state. */

    struct task *rt_transport_out = c->hydro.rt_transport_out;
    rt_reschedule_task(e, rt_transport_out, c, /*wait =*/0, /*callloc=*/1);

    struct task *rt_tchem = c->hydro.rt_tchem;
    rt_reschedule_task(e, rt_tchem, c, /*wait =*/1, /*callloc=*/1);

    /* Make sure we don't fully unlock the dependency that follows
     * after the rt_reschedule task */
    struct task *rt_out = c->hydro.rt_out;
    atomic_inc(&rt_out->wait);

    celltrace(c->cellID, "cycle %d - rescheduled tasks", c->hydro.rt_cycle - 1);
    return 1;
  }

  error("You shouldn't be here, yo");
}

/**
 * @brief Enqueue the RT task at the top of the hierarchy, and re-schedule
 * the rescheduler task itself.
 * @param r The #runner thread.
 * @param c The #cell.
 */
int rt_requeue(struct engine *e, struct cell *c) {
/* int rt_requeue(struct runner *r, struct cell *c) { */
  /* struct engine *e = r->e; */
  if (!e->subcycle_rt) return 0;

  /* rt_cycle is == 0 only if we're done subcycling. It would've
   * been increased in rt_reschedule otherwise. */
  if (c->hydro.rt_cycle == 0) return 0;

  /* Re-schedule the rescheduler task. */
  struct task *rt_reschedule = c->hydro.rt_reschedule;
  rt_reschedule_task(e, rt_reschedule, c, /*wait =*/1, /*callloc=*/5);
  celltrace(c->cellID, "Rescheduled the rescheduler");

  /* Finally, enqueue the RT task at the top of the hierarchy. */
  struct task *rt_transport_out = c->hydro.rt_transport_out;
  celltrace(c->cellID, "cycle %d - requeueing", c->hydro.rt_cycle);
  scheduler_enqueue(&e->sched, rt_transport_out, 4);

  return 1;
}