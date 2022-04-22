/*******************************************************************************
 * This file is part of SWIFT.
 * Coypright (c) 2017 Bert Vandenbroucke (bert.vandenbroucke@gmail.com)
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
#ifndef SWIFT_SHADOWSWIFT_HYDRO_VELOCITIES_H
#define SWIFT_SHADOWSWIFT_HYDRO_VELOCITIES_H

/**
 * @brief Initialize the GIZMO particle velocities before the start of the
 * actual run based on the initial value of the primitive velocity.
 *
 * @param p The particle to act upon.
 * @param xp The extended particle data to act upon.
 */
__attribute__((always_inline)) INLINE static void hydro_velocities_init(
    struct part* restrict p, struct xpart* restrict xp) {

#ifdef SHADOWSWIFT_FIX_PARTICLES
  p->v[0] = 0.0f;
  p->v[1] = 0.0f;
  p->v[2] = 0.0f;
#else
  p->v[0] = p->fluid_v[0];
  p->v[1] = p->fluid_v[1];
  p->v[2] = p->fluid_v[2];
#endif

  xp->v_full[0] = p->v[0];
  xp->v_full[1] = p->v[1];
  xp->v_full[2] = p->v[2];
}

/**
 * @brief Set the velocities based on the particles momentum.
 *
 * Velocities near vacuum are linearly suppressed.
 */
__attribute__((always_inline)) INLINE static void hydro_velocities_from_momentum(struct part* p, float* ret) {
  if (p->conserved.mass > 0.) {

    const float inverse_mass = 1.f / p->conserved.mass;

    const float rho = p->conserved.mass / p->geometry.volume;
    if (rho < 1e-10) {
      /* Suppress velocity linearly near vacuum */
      const double fac = rho * 1e10;
      ret[0] = fac * p->conserved.momentum[0] * inverse_mass;
      ret[1] = fac * p->conserved.momentum[1] * inverse_mass;
      ret[2] = fac * p->conserved.momentum[2] * inverse_mass;
    } else {
      /* Normal case: update fluid velocity and set particle velocity accordingly. */
      ret[0] = p->conserved.momentum[0] * inverse_mass;
      ret[1] = p->conserved.momentum[1] * inverse_mass;
      ret[2] = p->conserved.momentum[2] * inverse_mass;
    }
  } else {
    ret[0] = 0.;
    ret[1] = 0.;
    ret[2] = 0.;
  }
}

/**
 * @brief Set the velocity of a ShadowSWIFT particle, based on the values of its
 * primitive variables and the geometry of its voronoi cell.
 *
 * @param p The particle to act upon.
 * @param xp The extended particle data to act upon.
 */
__attribute__((always_inline)) INLINE static void hydro_velocities_set(
    struct part* restrict p, struct xpart* restrict xp) {

/* We first set the particle velocity. */
#ifdef SHADOWSWIFT_FIX_PARTICLES

  p->v[0] = 0.0f;
  p->v[1] = 0.0f;
  p->v[2] = 0.0f;

#else  // SHADOWSWIFT_FIX_PARTICLES

  if (p->conserved.mass > 0.0f && p->rho > 0.0f) {

    /* Normal case: set particle velocity to fluid velocity. */
    p->v[0] = p->fluid_v[0];
    p->v[1] = p->fluid_v[1];
    p->v[2] = p->fluid_v[2];

#ifdef SHADOWSWIFT_STEER_MOTION
    /* Add a correction to the velocity to keep particle positions close enough
       to
       the centroid of their mesh-free "cell". */
    /* The correction term below is the same one described in Springel (2010).
     */
    float ds[3];
    ds[0] = p->geometry.centroid[0];
    ds[1] = p->geometry.centroid[1];
    ds[2] = p->geometry.centroid[2];
    const float d = sqrtf(ds[0] * ds[0] + ds[1] * ds[1] + ds[2] * ds[2]);
    const float R = get_radius_dimension_sphere(p->geometry.volume);
    const float eta = 0.25f;
    const float etaR = eta * R;
    const float xi = 1.0f;
    const float soundspeed = sqrtf(hydro_gamma * p->P / p->rho);
    /* We only apply the correction if the offset between centroid and position
       is too large. */
    if (d > 0.9f * etaR) {
      float fac = xi * soundspeed / d;
      if (d < 1.1f * etaR) {
        fac *= 5.0f * (d - 0.9f * etaR) / etaR;
      }
      p->v[0] -= ds[0] * fac;
      p->v[1] -= ds[1] * fac;
      p->v[2] -= ds[2] * fac;
    }

#endif  // SHADOWSWIFT_STEER_MOTION
  } else {
    /* Vacuum particles have no fluid velocity. */
    p->v[0] = 0.0f;
    p->v[1] = 0.0f;
    p->v[2] = 0.0f;
  }

#endif  // SHADOWSWIFT_FIX_PARTICLES

  /* Now make sure all velocity variables are up to date. */
  xp->v_full[0] = p->v[0];
  xp->v_full[1] = p->v[1];
  xp->v_full[2] = p->v[2];

  if (p->gpart) {
    p->gpart->v_full[0] = p->v[0];
    p->gpart->v_full[1] = p->v[1];
    p->gpart->v_full[2] = p->v[2];
  }
}

#endif /* SWIFT_SHADOWSWIFT_HYDRO_VELOCITIES_H */