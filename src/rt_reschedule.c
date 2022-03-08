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
#include "rt.h"
#include "scheduler.h"

/**
 * @file src/rt_reschedule.c
 * @brief rescheduling of RT tasks for RT subcycling w.r.t. hydro/gravity.
 */

/**
 * @brief Set a task that has been run to a state where it can
 * run (ie enqueued) again.
 * @param t the #task to re-schedule
 * @param wait number of dependencies to set for this task.
 */
void rt_reschedule_task(struct task *t, int wait) {

  if (t == NULL) error("Passed NULL instead of task!?");

#ifdef SWIFT_DEBUG_CHECKS
  long long cellID = t->ci->cellID;
#else
  long long cellID = -1;
#endif

  if (wait < 0)
    error("Got negative wait (%d) to reschedule: %s/%s cellID %lld cycle %d", wait,
          taskID_names[t->type], subtaskID_names[t->subtype], cellID, t->ci->hydro.rt_cycle);

  if (atomic_cas(&t->skip, 1, 0) != 1)
    error(
        "Trying to reschedule a task with skip = %d; Should be 1 when "
        "rescheduling ; task %s/%s cell %lld cycle %d",
        t->skip, taskID_names[t->type], subtaskID_names[t->subtype], cellID, t->ci->hydro.rt_cycle);

  if (atomic_cas(&t->wait, 0, wait) != 0)
    error("Got t->wait = %d? Should be zero when rescheduling %s/%s cellID %lld cycle %d",
          t->wait, taskID_names[t->type], subtaskID_names[t->subtype], cellID, t->ci->hydro.rt_cycle);
}


/**
 * @brief Set a task that has been run and is part of a link group to a 
 * state where it can run (ie enqueued) again.
 * @param t the #task to re-schedule
 * @param wait number of dependencies to set for this task.
 */
void rt_reschedule_linked_task(struct task* t, int wait){

  if (t == NULL) error("Passed NULL instead of task!?");

#ifdef SWIFT_DEBUG_CHECKS
  long long cellID = t->ci->cellID;
#else
  long long cellID = -1;
#endif

  /* If task wasn't active this step, it should have rt_subcycle_wait = -1. */
  /* If that is the case, don't reschedule it. */
  if (t->rt_subcycle_wait == -1) {
    /* Keep this for now to see that it actually occurs. */
    message("Caught cell %lld rt %s task with rt_subcycle_wait == -1", cellID, subtaskID_names[t->subtype]);
  } else if (t->rt_subcycle_wait == 0) {
    /* These tasks must have at least one dependency. */
    error("Subcycling %s with no dependencies?", subtaskID_names[t->subtype]);
  } else {
    if (t->type == task_type_self || t->type == task_type_sub_self) {
      rt_reschedule_task(t, wait);
    } 
    else if (t->type == task_type_pair || t->type == task_type_sub_pair) {

      /* Race conditions to modify pair type tasks are common. 
       * Guard with a lock. */
      lock_lock(&t->reschedule_lock);

      if (t->skip == 0) {
        /* The task is already rescheduled by the other cell's call. Don't reset
         * the waits then, because some dependencies might already be unlocked. */
        if ((t->ci->cellID == 27 || t->cj->cellID == 27) && t->subtype == task_subtype_rt_gradient) 
          message("Cells %lld/%lld skip=0 NOT rescheduling gradient with c->rt_cycles %d / %d", t->ci->cellID, t->cj->cellID, t->ci->hydro.rt_cycle, t->cj->hydro.rt_cycle);
#ifdef SWIFT_DEBUG_CHECKS
        if (t->wait < 0 || t->wait > t->rt_subcycle_wait) error("Cell %lld: Inconsistent wait for task while rescheduling: " "wait=%d, rt_subcycle_wait=%d, skip=%d", cellID, t->wait, t->rt_subcycle_wait, t->skip);
#endif
      } else {
        if ((t->ci->cellID == 27 || t->cj->cellID == 27) && t->subtype == task_subtype_rt_gradient) 
          message("Cells %lld/%lld skip=1 rescheduling gradient with c->rt_cycles %d / %d", t->ci->cellID, t->cj->cellID, t->ci->hydro.rt_cycle, t->cj->hydro.rt_cycle);
        /* Skipped tasks should have wait = 0. If this is not the case, something is wrong. */
        if (t->wait != 0) error("Cell %lld: Inconsistent wait for task while rescheduling: wait=%d, rt_subcycle_wait=%d, skip=%d, %s/%s", cellID, t->wait, t->rt_subcycle_wait, t->skip, taskID_names[t->type], subtaskID_names[t->subtype]);
        rt_reschedule_task(t, wait);
      }

      if (lock_unlock(&t->reschedule_lock) != 0) error("Failed to unlock %s cell %lld during rescheduling", subtaskID_names[t->subtype], cellID);

    }
  }
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

#ifdef SWIFT_DEBUG_CHECKS
  long long cellID = c->cellID;
#else
  long long cellID = -1;
#endif

  /* increment the counter */
  c->hydro.rt_cycle += 1;

  if (c->hydro.rt_cycle == RT_RESCHEDULE_MAX) {
    /* We're done with the subcycling. */
    c->hydro.rt_cycle = 0;
    return 0;
  } else {

    /* Set all RT tasks that do actual work back to a re-queueable state. */

    /* rt_ghost1 is the task at the top of the hierarchy. Don't let it
     * wait for any dependencies. Don't enqueue it yet. */
    struct task *rt_ghost1 = c->hydro.rt_ghost1;
    if (rt_ghost1 != NULL) rt_reschedule_task(rt_ghost1, /*wait=*/0);
    /* if (cellID == 27) message("CellID %lld Rescheduled rt_ghost1", cellID); */
    message("CellID %lld Rescheduled rt_ghost1 cycle %d", cellID, c->hydro.rt_cycle);

    for (struct link *l = c->hydro.rt_gradient; l != NULL; l = l->next) {
      struct task *t = l->t;
      rt_reschedule_linked_task(t, t->rt_subcycle_wait);
    }
    if (cellID == 27) message("CellID %lld rescheduled gradient tasks", cellID);

    struct task *rt_ghost2 = c->hydro.rt_ghost2;
    if (rt_ghost2 != NULL) rt_reschedule_task(rt_ghost2, rt_ghost2->rt_subcycle_wait);
    if (cellID == 27) message("CellID %lld Rescheduled rt_ghost2; wait = %d", cellID, rt_ghost2->rt_subcycle_wait);

    for (struct link *l = c->hydro.rt_transport; l != NULL; l = l->next) {
      struct task *t = l->t;
      rt_reschedule_linked_task(t, t->rt_subcycle_wait);
    }
    if (cellID == 27) message("CellID %lld rescheduled transport tasks", cellID);

    struct task *rt_transport_out = c->hydro.rt_transport_out;
    if (rt_transport_out != NULL) rt_reschedule_task(rt_transport_out, rt_transport_out->rt_subcycle_wait);
    if (cellID == 27) message("CellID %lld Rescheduled rt_transport_out; wait = %d", cellID, rt_transport_out->wait);

    struct task *rt_tchem = c->hydro.rt_tchem;
    if (rt_tchem != NULL) rt_reschedule_task(rt_tchem, rt_tchem->rt_subcycle_wait);
    if (cellID == 27) message("CellID %lld Rescheduled rt_tchem; wait = %d", cellID, rt_tchem->wait);

    /* Make sure we don't fully unlock the dependency that follows */
    /* after the rt_reschedule task: block the implicit rt_out */
    struct task *rt_out = c->hydro.rt_out;
    if (rt_out != NULL) atomic_inc(&rt_out->wait);
    if (cellID == 27) message("CellID %lld blocking rt_out; wait = %d", cellID, rt_out->wait);

    /* There is an rt_ghost1 -> timestep dependency for cases where we have 
     * active stars, but no active gas particles in a cell. In these cases, 
     * the gradient, transport, and thermochemistry tasks will be skipped, 
     * an the dependency from ghost1 to timestep will be missing. Since we're 
     * re-scheduling rt_ghost1 now, we need to keep increasing the 
     * timestep->wait as well. */
    struct task *timestep = c->timestep;
    if (timestep != NULL) atomic_inc(&timestep->wait);
    if (cellID == 27) message("CellID %lld blocking timestep; wait = %d", cellID, timestep->wait);

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
  rt_reschedule_task(rt_reschedule, /*wait =*/rt_reschedule->rt_subcycle_wait);

  /* Finally, enqueue the RT task at the top of the hierarchy. */
  struct task *rt_ghost1 = c->hydro.rt_ghost1;
  scheduler_enqueue(&e->sched, rt_ghost1);
  if (c->cellID == 27) message("CellID %lld enqueued rt_ghost1", c->cellID);

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

    /* Select here which tasks you need. Note: We're choosing the ones that
     * will *unlock* others, not all tasks that will be unlocked during 
     * subcycling. */
    enum task_types tt = t->type;
    enum task_subtypes ts = t->subtype;
    if (tt == task_type_rt_ghost1 || 
        tt == task_type_rt_ghost2 ||
        tt == task_type_rt_transport_out || 
        tt == task_type_rt_tchem ||
        tt == task_type_rt_reschedule || 
        ts == task_subtype_rt_gradient || 
        ts == task_subtype_rt_transport) {

      /* First increase your own rt_subcycle_wait. We initialize to -1 to catch possible errors. */
      atomic_inc(&t->rt_subcycle_wait);

      /* Sets the subcycle_waits of the dependances */
      for (int k = 0; k < t->nr_unlock_tasks; k++) {
        struct task *u = t->unlock_tasks[k];
        atomic_inc(&u->rt_subcycle_wait);
        long long ciID = u->ci->cellID;
        long long cjID = -1;
        if (u->cj != NULL) cjID = u->cj->cellID;
        if (u->subtype == task_subtype_rt_gradient && (ciID == 27 || cjID == 27))
          message("RT gradient cells %lld %lld increased rt_wait to %d from task %s cell %lld", ciID, cjID, u->rt_subcycle_wait, taskID_names[t->type], t->ci->cellID);
      }
    }
  }
}

/**
 * @brief #threadpool_map function which runs through the task
 *        graph and resets the task wait counters for the RT subcycling.
 */
void rt_subcycle_reset_wait_mapper(void *map_data, int num_elements,
                                   void *extra_data) {
message("Resetting subcycle waits");
  struct scheduler *s = (struct scheduler *)extra_data;
  const int *tid = (int *)map_data;
  struct task *tasks = s->tasks;

  for (int ind = 0; ind < num_elements; ind++) {
    struct task *t = &tasks[tid[ind]];
    t->rt_subcycle_wait = -1;
  }
}

/**
 * @brief Run some checks/resets on a single particle at the end
 * of each subcycling loop.
 * @param p #part to work on
 * @param rescheduled whether we rescheduled in this subcycle.
 */
void rt_reschedule_particle_checks(struct part* restrict p, int rescheduled){

#if defined(RT_DEBUG) || defined(RT_GEAR)

#if defined SWIFT_RT_DEBUG_CHECKS
  if (p->rt_data.debug_kicked != 1)
    error("Trying to do rescheduling on unkicked particle %lld (count=%d)",
          p->id, p->rt_data.debug_kicked);
  if (p->rt_data.debug_injection_done != 1)
    error("Trying to do rescheduling when injection step hasn't been done");
  if (p->rt_data.debug_gradients_done != 1)
    error("Trying to do rescheduling when gradient step hasn't been done");
  if (p->rt_data.debug_transport_done != 1)
    error("Trying to do rescheduling when transport step hasn't been done");
  if (p->rt_data.debug_thermochem_done != 1)
    error("Trying to do rescheduling when transport step hasn't been done");
#endif

  /* Don't reset quantities at the end of the subcycling this step. */
  if (rescheduled) rt_debugging_reset_each_subcycle(p);

  /* Mark that the subcycling has happened */
  rt_debugging_count_subcycle(p);
#endif
}
