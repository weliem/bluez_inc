/*
 *   Copyright (c) 2022 Martijn van Welie
 *
 *   Permission is hereby granted, free of charge, to any person obtaining a copy
 *   of this software and associated documentation files (the "Software"), to deal
 *   in the Software without restriction, including without limitation the rights
 *   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *   copies of the Software, and to permit persons to whom the Software is
 *   furnished to do so, subject to the following conditions:
 *
 *   The above copyright notice and this permission notice shall be included in all
 *   copies or substantial portions of the Software.
 *
 *   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *   SOFTWARE.
 *
 */

#ifndef BINC_AGENT_H
#define BINC_AGENT_H

#include <gio/gio.h>
#include "adapter.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef gboolean (*AgentRequestAuthorizationCallback)(Device *device);

typedef guint32 (*AgentRequestPasskeyCallback)(Device *device);

typedef enum {
    DISPLAY_ONLY, DISPLAY_YES_NO, KEYBOARD_ONLY, NO_INPUT_NO_OUTPUT, KEYBOARD_DISPLAY
} IoCapability;

typedef struct binc_agent Agent;

Agent *binc_agent_create(Adapter *adapter, const char *path, IoCapability io_capability);

void binc_agent_set_request_authorization_cb(Agent *agent, AgentRequestAuthorizationCallback callback);

void binc_agent_set_request_passkey_cb(Agent *agent, AgentRequestPasskeyCallback callback);

void binc_agent_free(Agent *agent);

const char *binc_agent_get_path(const Agent *agent);

Adapter *binc_agent_get_adapter(const Agent *agent);

#ifdef __cplusplus
}
#endif

#endif //BINC_AGENT_H
