/*******************************************************************************
 * This file is part of SWIFT.
 * Copyright (c) 2017 James S. Wills (james.s.willis@durham.ac.uk)
 *                    Matthieu Schaller (matthieu.schaller@durham.ac.uk)
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
#ifndef SWIFT_SORT_PART_H
#define SWIFT_SORT_PART_H

/**
 * @brief Entry in a list of sorted indices.
 */
struct entry {

  /*! Distance on the axis */
  float d;

  /*! Particle index */
  int i;
};

/**
 * @brief Determines whether a pair of cells are corner to corner.
 *
 * @param sid sort ID
 *
 * @return 1 if corner to corner, 0 otherwise.
 */
__attribute__((always_inline)) INLINE static int sort_is_corner(int sid) {
  return (sid == 0 || sid == 2 || sid == 6 || sid == 8);
}

/**
 * @brief Determines whether a pair of cells are edge to edge.
 *
 * @param sid sort ID
 *
 * @return 1 if edge to edge, 0 otherwise.
 */
__attribute__((always_inline)) INLINE static int sort_is_edge(int sid) {
  return (sid == 1 || sid == 3 || sid == 5 || sid == 7 || sid == 9 || sid == 11);
}

/**
 * @brief Determines whether a pair of cells are face to face.
 *
 * @param sid sort ID
 *
 * @return 1 if face to face, 0 otherwise.
 */
__attribute__((always_inline)) INLINE static int sort_is_face(int sid) {
  return (sid == 4 || sid == 10 || sid == 12);
}


#endif /* SWIFT_SORT_PART_H */
