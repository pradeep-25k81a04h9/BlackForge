#ifndef HINDSIGHT_INCIDENT_H
#define HINDSIGHT_INCIDENT_H

#include "event.h"

typedef struct
{
    int found;
    unsigned int event_index;
    HindsightEvent event;
} HindsightIncident;

void hindsight_incident_init(void);

int hindsight_incident_find_latest(HindsightIncident *incident);

void hindsight_incident_print(const HindsightIncident *incident);

#endif

