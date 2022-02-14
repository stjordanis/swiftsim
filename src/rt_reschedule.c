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

/* #include "rt.h" */
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

#ifdef SWIFT_DEBUG_CHECKS
  long long cellID = t->ci->cellID;
#else
  long long cellID = -1;
#endif
  if (wait < 0)
    error("Got negative wait (%d) to reschedule: %s/%s cellID %lld", wait,
          taskID_names[t->type], subtaskID_names[t->subtype], cellID);

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

#ifdef SWIFT_DEBUG_CHECKS
  long long cellID = c->cellID;
#else
  long long cellID = -1;
#endif

  /* increment the counter */
  c->hydro.rt_cycle += 1;

  if (c->cellID == 1) message("Calling loc 1 reschedule cell %lld %d", c->cellID, c->hydro.rt_cycle);

  /* if (c->hydro.rt_cycle > RT_RESCHEDULE_MAX) { */
  /*   if (c->cellID == 1) message("Calling 2.1 reschedule cell %lld", c->cellID); */
  /*   error("CellID %lld trying to subcycle too much?", cellID); */
  /* } else  */
  if (c->hydro.rt_cycle == RT_RESCHEDULE_MAX) {
    /* if (c->cellID == 1) message("Calling 2.2 reschedule cell %lld", c->cellID); */
    /* fflush(stdout); */
    /* if (cellID == 1) message("Exiting rescheduling loop"); */
    /* fflush(stdout); */

    if (c->cellID == 1) message("Calling loc 2.1 cell %lld, %d", c->cellID, c->hydro.rt_cycle);
    /* We're done with the subcycling. */
    c->hydro.rt_cycle = 0;
    return 0;
  } else {
    if (c->cellID == 1) message("Calling loc 2.2 cell %lld, %d", c->cellID, c->hydro.rt_cycle);

    /* Set all RT tasks that do actual work back to a re-queueable state. */

    /* rt_ghost1 is the task at the top of the hierarchy. Don't let it
     * wait for any dependencies. */
    struct task *rt_ghost1 = c->hydro.rt_ghost1;
    if (c->cellID == 1ll) message("CellID %lld Got rt_ghost1->wait = %d", cellID, rt_ghost1->rt_subcycle_wait);
    rt_reschedule_task(e, rt_ghost1, /*wait=*/0);

    if (cellID == 1) message("Checking gradients?");
    
    for (struct link *l = c->hydro.rt_gradient; l != NULL; l = l->next) {
      struct task *t = l->t;
      if (cellID == 1) message("Checking gradients! %d", t->rt_subcycle_wait);

      /* If task wasn't active this step, it should have rt_subcycle_wait = -1. */
      /* If that is the case, don't reschedule it. */
      if (t->rt_subcycle_wait == -1) {
        message("Caught cell %lld rt gradient task with rt_subcycle_wait == -1",
                cellID);
        continue;
      } else if (t->rt_subcycle_wait == 0) {
        /* These tasks must have at least one dependency. */
        error("How did we get here?");
      } else {
        if (t->type == task_type_self || t->type == task_type_sub_self) {
          rt_reschedule_task(e, t, t->rt_subcycle_wait);
          if (cellID == 1) message("Cell %lld Rescheduled rt_gradient %s", cellID, taskID_names[t->type]);
        } else if (t->type == task_type_pair || t->type == task_type_sub_pair) {
          /* The task might have already been rescheduled through the other */
          /* cell. Make sure you don't reset the waits then, because some */
          /* dependencies might've already been unlocked. */
          if (t->skip == 0) {
            /* Task already unlocked through the other cell. */
            if (t->wait < 0 || t->wait > t->rt_subcycle_wait)
              error(
                  "Cell %lld: Inconsistent wait for task while rescheduling: "
                  "wait=%d, rt_subcycle_wait=%d, skip=%d",
                  cellID, t->wait, t->rt_subcycle_wait, t->skip);
          } else if (t->skip == 1) {
            if (t->wait != 0)
              error(
                  "Cell %lld: Inconsistent wait for task while rescheduling: "
                  "wait=%d, rt_subcycle_wait=%d, skip=%d",
                  cellID, t->wait, t->rt_subcycle_wait, t->skip);
            rt_reschedule_task(e, t, t->rt_subcycle_wait);
            if (cellID == 1) message("Cell %lld Rescheduled rt_gradient %s", cellID, taskID_names[t->type]);
          }
        } else {
          error("Wrong task type in gradient link?????");
        }
      }
    }

    struct task *rt_ghost2 = c->hydro.rt_ghost2;
    if (cellID == 1)
      message("CellID %lld Got rt_ghost2->wait = %d", cellID, rt_ghost2->rt_subcycle_wait);
    rt_reschedule_task(e, rt_ghost2, rt_ghost2->rt_subcycle_wait);

    for (struct link *l = c->hydro.rt_transport; l != NULL; l = l->next) {
      struct task *t = l->t;

      /* If task wasn't active this step, it should have rt_subcycle_wait = -1. */
      /* If that is the case, don't reschedule it. */
      if (t->rt_subcycle_wait == -1) {
        message( "Caught cell %lld rt transport task with rt_subcycle_wait == -1", cellID);
        continue;
      } else if (t->rt_subcycle_wait == 0) {
        /* These tasks must have at least one dependency. */
        error("How did we get here?");
      } else {
        if (t->type == task_type_self || t->type == task_type_sub_self) {
          rt_reschedule_task(e, t, t->rt_subcycle_wait);
        } else if (t->type == task_type_pair || t->type == task_type_sub_pair) {
          /* The task might have already been rescheduled through the other */
          /* cell. Make sure you don't reset the waits then, because some */
          /* dependencies might've already been unlocked. */
          if (t->skip == 0) {
            /* Task already unlocked through the other cell. */
            if (t->wait < 0 || t->wait > t->rt_subcycle_wait)
              error(
                  "Cell %lld: Inconsistent wait for task while rescheduling: "
                  "wait=%d, rt_subcycle_wait=%d, skip=%d",
                  cellID, t->wait, t->rt_subcycle_wait, t->skip);
          } else if (t->skip == 1) {
            if (t->wait != 0)
              error(
                  "Cell %lld: Inconsistent wait for task while rescheduling: "
                  "wait=%d, rt_subcycle_wait=%d, skip=%d",
                  cellID, t->wait, t->rt_subcycle_wait, t->skip);
            rt_reschedule_task(e, t, t->rt_subcycle_wait);
          }
        } else {
          error("Wrong task type in transport link?????");
        }
      }
    }

    struct task *rt_transport_out = c->hydro.rt_transport_out;
    if (cellID == 1)
      message("CellID %lld Got rt_transport_out->wait = %d", cellID,
              rt_transport_out->rt_subcycle_wait);
    rt_reschedule_task(e, rt_transport_out, rt_transport_out->rt_subcycle_wait);

    struct task *rt_tchem = c->hydro.rt_tchem;
    if (cellID == 1)
      message("CellID %lld Got rt_tchem->wait = %d", cellID, rt_tchem->rt_subcycle_wait);
    rt_reschedule_task(e, rt_tchem, rt_tchem->rt_subcycle_wait);

    /* Make sure we don't fully unlock the dependency that follows */
    /* after the rt_reschedule task: block the implicit rt_out */
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
  rt_reschedule_task(e, rt_reschedule, /*wait =*/rt_reschedule->rt_subcycle_wait);

  /* Finally, enqueue the RT task at the top of the hierarchy. */
  struct task *rt_ghost1 = c->hydro.rt_ghost1;
  scheduler_enqueue(&e->sched, rt_ghost1);
  if (c->cellID == 1)
    message("CellID %lld enqueued rt_ghost1", c->cellID);

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
    if (tt == task_type_rt_ghost1 || tt == task_type_rt_ghost2 ||
        tt == task_type_rt_transport_out || task_type_rt_tchem ||
        task_type_rt_reschedule ||
        ((tt == task_type_pair || tt == task_type_sub_pair ||
          tt == task_type_self || tt == task_type_sub_self) &&
         (ts == task_subtype_rt_gradient || ts == task_subtype_rt_transport))) {

      /* First increase your own rt_subcycle_wait. We initialize to -1 to catch possible errors. */
      atomic_inc(&t->rt_subcycle_wait);
      if (t->ci->cellID == 1 && t->type == task_type_rt_tchem)
        message("CellID %lld Got rt_tchem->rt_subcycle_wait = %d after call on " "it self", t->ci->cellID, t->rt_subcycle_wait);

      /* Sets the subcycle_waits of the dependances */
      for (int k = 0; k < t->nr_unlock_tasks; k++) {
        struct task *u = t->unlock_tasks[k];
        atomic_inc(&u->rt_subcycle_wait);
        if (u->ci->cellID == 1 && u->type == task_type_rt_tchem)
          message(
              "CellID %lld Got rt_tchem->rt_subcycle_wait = %d after call "
              "from unlock",
              u->ci->cellID, u->rt_subcycle_wait);
        if (u->subtype == task_subtype_rt_gradient) {
          int check_i = (u->ci->cellID == 1) || (u->ci->cellID == 2);
          int check_j = 0;
          if (u->cj != NULL) check_j = (u->cj->cellID == 1) || (u->cj->cellID == 2);
          if (check_i && check_j)
            message("Adding unlock for cell 1 from %s %s", taskID_names[u->type], subtaskID_names[u->subtype]);
        }
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

  struct scheduler *s = (struct scheduler *)extra_data;
  const int *tid = (int *)map_data;
  struct task *tasks = s->tasks;

  for (int ind = 0; ind < num_elements; ind++) {
    struct task *t = &tasks[tid[ind]];
    t->rt_subcycle_wait = -1;
    if (t->ci->cellID == 1 && t->type == task_type_rt_tchem)
      message("CellID %lld Set rt_tchem->rt_subcycle_wait = %d", t->ci->cellID,
              t->rt_subcycle_wait);
  }
}


/**
 * @brief Run some checks/resets on a single particle at the end
 * of each subcycling loop.
 */
void rt_reschedule_particle_checks(struct part* restrict p){

#if defined(RT_DEBUG) || defined(RT_GEAR)
  rt_debugging_reset_each_subcycle(p);
  rt_debugging_count_subcycle(p);
#endif
}
