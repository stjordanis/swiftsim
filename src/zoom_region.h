#include "space.h"

#ifndef SWIFT_ZOOM_H
#define SWIFT_ZOOM_H

void zoom_region_init(struct swift_params *params, struct space *s);
void construct_zoom_region(struct swift_params *params, struct space *s);
double cell_min_dist2(const struct cell *restrict ci,
                      const struct cell *restrict cj, const int periodic,
                      const double dim[3]);

#endif /* SWIFT_ZOOM_H */
