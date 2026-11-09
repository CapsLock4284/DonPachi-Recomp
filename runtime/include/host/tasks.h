/**
 *    Name:        tasks.h
 *    Description: native stacks for the game's tasks: code that runs as a thread of its own and is set
 *                 aside and taken up again where it was
 *    Version:     0.1.0
 *    Created:     29-10-2026
 *    Modified:    29-10-2026
 *    Author:      Sebastian Toyos
 *
 *    license:BSD-3-Clause
 *    The translated program runs on a stack of its own (a fiber on Windows, a ucontext elsewhere), as deep as
 *    its calls nest with the generated code's locals, apart from the port's: program_stop goes back to the
 *    port from inside the program's frame, and only one runs at a time, so nothing is concurrent. A game whose
 *    interrupt handler returns into another task (OutRunners') keeps each task on a stack of its own the same
 *    way; TMNT's VBLANK handler returns into the code it interrupted, so its program is one task.
 */
#ifndef HOST_TASKS_H
#define HOST_TASKS_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct host_task host_task;

/* The task the calling thread already is: the one that starts the others and that they can go back to */
host_task *host_task_main(void);

/* A task that runs 'start' on a stack of its own the first time it is switched to; 'start' never returns */
host_task *host_task_create(void (*start)(void));

/* Sets the running task aside where it is and goes on with 'to' where it was set aside */
void host_task_switch(host_task *to);

host_task *host_task_current(void);

#ifdef __cplusplus
}
#endif

#endif
