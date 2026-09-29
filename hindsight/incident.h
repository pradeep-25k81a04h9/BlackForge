#ifndef HINDSIGHT_INCIDENT_H
#define HINDSIGHT_INCIDENT_H

#include <stdint.h>
#include "event.h"

/*
 * Returns the number of events associated
 * with the specified process ID.
 */
uint32_t hindsight_incident_count(uint32_t pid);

/*
 * Returns the event at the specified incident
 * index for the given process ID.
 *
 * Returns 0 if the index does not exist.
 */
const hindsight_event_t*
hindsight_incident_get(uint32_t pid, uint32_t index);

#endif
