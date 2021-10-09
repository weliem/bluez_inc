//
// Created by martijn on 25/9/21.
//

#ifndef TEST_AGENT_H
#define TEST_AGENT_H

#include <gio/gio.h>
#include "adapter.h"

typedef gboolean (*AgentRequestAuthorizationCallback)(Device *device);
typedef guint32 (*AgentRequestPasskeyCallback)(Device *device);

typedef enum {
    DISPLAY_ONLY, DISPLAY_YES_NO, KEYBOARD_ONLY, NO_INPUT_NO_OUTPUT, KEYBOARD_DISPLAY
} IoCapability;

typedef struct binc_agent Agent;

Agent *binc_agent_create(Adapter *adapter, const char *path, IoCapability io_capability);
void binc_agent_set_request_authorization_callback(Agent *agent, AgentRequestAuthorizationCallback callback);
void binc_agent_set_request_passkey_callback(Agent *agent, AgentRequestPasskeyCallback callback);

void binc_agent_free(Agent *agent);

#endif //TEST_AGENT_H
