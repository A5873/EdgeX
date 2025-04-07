/*
 * EdgeX OS - Event Interface
 *
 * This file defines the interface for events in EdgeX OS,
 * which are used for signaling and waiting between tasks.
 */

#ifndef EDGEX_IPC_EVENT_H
#define EDGEX_IPC_EVENT_H

#include <edgex/ipc/common.h>

/* Event handle (opaque) */
typedef struct event* event_t;

/* Event functions */
event_t create_event(const char* name);
void destroy_event(event_t event);
int event_wait(event_t event);
int event_timedwait(event_t event, uint64_t timeout_ms);
int event_signal(event_t event);
int event_broadcast(event_t event);
int event_reset(event_t event);

/* Event set (for waiting on multiple events) */
typedef struct event_set* event_set_t;

/* Event set functions */
event_set_t create_event_set(const char* name, uint32_t max_events);
void destroy_event_set(event_set_t event_set);
int event_set_add(event_set_t event_set, event_t event);
int event_set_remove(event_set_t event_set, event_t event);
int event_set_wait(event_set_t event_set, event_t* signaled_event);
int event_set_timedwait(event_set_t event_set, event_t* signaled_event, uint64_t timeout_ms);

/* Internal functions */
void init_event_subsystem(void);
void cleanup_task_events(pid_t pid);
void check_event_timeouts(void);
void dump_all_events(void);

#endif /* EDGEX_IPC_EVENT_H */

