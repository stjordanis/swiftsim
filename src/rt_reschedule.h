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

#ifndef SWIFT_RT_RESCHEDULE_H
#define SWIFT_RT_RESCHEDULE_H

#include "task.h"

/**
 * @file src/rt_reschedule.h
 * @brief Main header file for the rescheduling of RT tasks
 */

/* TODO: temporary for dev */
#define RT_RESCHEDULE_MAX 10


/**
 * @brief Set a task that has been run to a state where it can
 * run again
 * TODO: docs
 */
INLINE static void rt_reschedule_task(struct engine* e, struct task *t, struct cell* c, int wait, int callloc) {
  /* TODO: remove cell parameter later . possibly engine too? */

  if (t == NULL) return;

  /* scheduler_done checks with atomic_dec(&t2->wait) == 1 to restart. */
  /* So set the wait of the first task that needs to be run to exactly 2. */
  /* Now it should be at 0 */
  /* scheduler_done is called after each task is done at the end of runner_main */
  int w = atomic_cas(&t->wait, 0, wait);
  if (w != 0)
    message("Got t->wait = %d? %s, callloc %d cellID %lld", 
    w, taskID_names[t->type], callloc, c->cellID);
  if (atomic_cas(&t->skip, 1, 0) != 1)
    error("Trying to reschedule a task with skip = 0");

  /* atomic_inc(&sched->waiting); */

}

/**
 * @brief Re-schedule the rescheduler task itself.
 * @param r The #runner thread.
 * @param c The #cell.
 * @param rescheduler_task The #task that reschedules the RT cycle
 */
INLINE static void rt_reschedule_rescheduler(struct runner *r, struct cell *c, struct task *rescheduler_task){
  struct engine *e = r->e;
  if (!e->subcycle_rt) return;

#ifdef SWIFT_DEBUG_CHECKS
  if (rescheduler_task->type != task_type_rt_reschedule)
    error("Rescheduling task which isn't rescheduler task");
#endif

  rt_reschedule_task(e, rescheduler_task, c, /*wait =*/3, /*callloc=*/2);
  message("Rescheduled the rescheduler cell %lld", c->cellID);
}


/**
 * @brief Re-schedule the entire RT cycle for the given cell.
 * @param r The #runner thread.
 * @param c The #cell.
 * @return 1 if done, 0 if not.
 */
INLINE static int rt_reschedule(struct runner *r, struct cell *c){
  struct engine *e = r->e;
  struct scheduler* sched = &e->sched;
  if (!e->subcycle_rt) return 0;

  message("called rt_reschedule cell %lld at cycle %d", c->cellID, c->hydro.rt_cycle);

  if (c->hydro.rt_cycle == RT_RESCHEDULE_MAX){
    /* Reset and stop the rescheduling. */
    c->hydro.rt_cycle = 0;
    return 0;
  } else {
    /* Note to self: We're only enqueueing the task at the top of the hierarchy,
     * and setting all others below to enqueueable states.
     * Pay attention to whether task you're enqueueing is implicit or not: They
     * are handled differently in scheduler_enqueue(). In particular, pay attention
     * to which value to re-set task->wait. */
    struct task *rt_tchem = c->hydro.rt_tchem;
    rt_reschedule_task(e, rt_tchem, c, /*wait =*/1, /*callloc=*/1);

    struct task *rt_transport_out = c->hydro.rt_transport_out;
    rt_reschedule_task(e, rt_transport_out, c, /*wait =*/1, /*callloc=*/1);


    message("Ran reschedule on cell %lld cycle %d, callloc %d", c->cellID, c->hydro.rt_cycle, 100);
    scheduler_enqueue(sched, rt_transport_out, 100);
    c->hydro.rt_cycle += 1;

    /* TODO: we need to increase the wains of the time step task
     * that is the rt_reschedule tasks's only dependency. */
    return 1;
  }

  error("You shouldn't be here, yo");
}

#endif /* defined SWIFT_RT_RESCHEDULE_H */
