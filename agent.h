//
// Created by martijn on 25/9/21.
//

#ifndef TEST_AGENT_H
#define TEST_AGENT_H

#include <gio/gio.h>
#include "adapter.h"

typedef enum {
    DISPLAY_ONLY, DISPLAY_YES_NO, KEYBOARD_ONLY, NO_INPUT_NO_OUTPUT, KEYBOARD_DISPLAY
} IoCapability;

typedef struct binc_agent Agent;

Agent *binc_agent_create(Adapter *adapter, IoCapability io_capability);

void binc_agent_free(Agent *agent);

#endif //TEST_AGENT_H
