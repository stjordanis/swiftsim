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
#include "scheduler.h"

/**
 * @file src/rt_reschedule.c
 * @brief rescheduling of RT tasks for RT subcycling w.r.t. hydro/gravity.
 */

/**
 * @brief Set a task that has been run to a state where it can
 * run (ie enqueued) again.
 * @param e the #engine
 * @param t the #task to re-schedule
 * @param wait number of dependencies to set for this task.
 */
void rt_reschedule_task(struct engine *e, struct task *t, int wait) {

  if (t == NULL) return;

#ifdef SWIFT_DEBUGGING_CHECKS
  long long cellID = t->ci->cellID;
#else
  long long cellID = -1;
#endif

  int w, s;
  if ((w = atomic_cas(&t->wait, 0, wait)) != 0)
    error("Got t->wait = %d? Should be zero when rescheduling %s cellID %lld",
          w, taskID_names[t->type], cellID);
  if ((s = atomic_cas(&t->skip, 1, 0)) != 1)
    error(
        "Trying to reschedule a task with skip = %d; Should be 1 when "
        "rescheduling ; task %s cell %lld cycle %d",
        s, taskID_names[t->type], cellID, t->ci->hydro.rt_cycle);
}

/**
 * @brief Re-schedule the entire RT cycle (that does the actual work)
 * for the given cell.
 * @param r The #runner thread.
 * @param c The #cell whose tasks we're re-scheduling.
 * @return 1 if we rescheduled, 0 if not.
 */
int rt_reschedule(struct runner *r, struct cell *c) {
  struct engine *e = r->e;
  if (!e->subcycle_rt) return 0;

  /* increment the counter */
  c->hydro.rt_cycle += 1;

  if (c->hydro.rt_cycle > RT_RESCHEDULE_MAX) {
    error("trying to subcycle too much?");
  } else if (c->hydro.rt_cycle == RT_RESCHEDULE_MAX) {

    /* We're done with the subcycling. */
    c->hydro.rt_cycle = 0;
    return 0;
  } else {

    /* Set all RT tasks that do actual work back to a re-queueable state. */

    struct task *rt_transport_out = c->hydro.rt_transport_out;
    rt_reschedule_task(e, rt_transport_out, /*wait =*/0);

    struct task *rt_tchem = c->hydro.rt_tchem;
    rt_reschedule_task(e, rt_tchem, /*wait =*/1);

    /* Make sure we don't fully unlock the dependency that follows
     * after the rt_reschedule task: block the implicit rt_out */
    struct task *rt_out = c->hydro.rt_out;
    atomic_inc(&rt_out->wait);

    return 1;
  }
}

/**
 * @brief Enqueue the RT task at the top of the hierarchy, and re-schedule
 * the rescheduler task itself.
 * @param e The #engine
 * @param c The #cell whose task we're re-queueing.
 */
int rt_requeue(struct engine *e, struct cell *c) {
  if (!e->subcycle_rt) return 0;

  /* rt_cycle is == 0 only if we're done subcycling. It would've
   * been increased in rt_reschedule otherwise. */
  if (c->hydro.rt_cycle == 0) return 0;

  /* Re-schedule the rescheduler task. */
  struct task *rt_reschedule = c->hydro.rt_reschedule;
  rt_reschedule_task(e, rt_reschedule, /*wait =*/1);

  /* Finally, enqueue the RT task at the top of the hierarchy. */
  struct task *rt_transport_out = c->hydro.rt_transport_out;
  scheduler_enqueue(&e->sched, rt_transport_out);

  return 1;
}

/**
 * @brief #threadpool_map function which runs through the task
 *        graph and re-computes the task wait counters for the
 *        RT subcycling.
 */
void rt_subcycle_rewait_mapper(void *map_data, int num_elements,
                                 void *extra_data) {
  struct scheduler *s = (struct scheduler *)extra_data;
  const int *tid = (int *)map_data;

  for (int ind = 0; ind < num_elements; ind++) {
    struct task *t = &s->tasks[tid[ind]];

    /* Ignore skipped tasks. */
    if (t->skip) continue;

    /* Select here which tasks you need. */
    enum task_types tt = t->type;
    enum task_subtypes ts = t->subtype;
    if (tt == task_type_rt_ghost1 ||
        tt == task_type_rt_ghost2 || 
        tt == task_type_rt_transport_out || 
        task_type_rt_tchem || 
        task_type_rt_reschedule ||
        ((tt == task_type_pair || tt == task_type_sub_pair || tt == task_type_self || tt == task_type_sub_self) && (ts == task_subtype_rt_gradient || ts == task_subtype_rt_transport)) 
       ) {

      /* First increase your own wait. We initialize to -1 to catch possible errors. */
      atomic_inc(&t->rt_subcycle_wait);

      /* Sets the subcycle_waits of the dependances */
      for (int k = 0; k < t->nr_unlock_tasks; k++) {
        struct task *u = t->unlock_tasks[k];
        atomic_inc(&u->rt_subcycle_wait);
      }
    }
  }
}

/**
 * @brief #threadpool_map function which runs through the task
 *        graph and re-computes the task wait counters for the
 *        RT subcycling.
 */
void rt_subcycle_reset_wait_mapper(void *map_data, int num_elements,
                                   void *extra_data) {
  struct scheduler *s = (struct scheduler *)extra_data;

  for (int ind = 0; ind < num_elements; ind++) {
    struct task *t = &s->tasks[ind];
    /* Ignore skipped tasks. */
    t->rt_subcycle_wait = -1;
  }
}
