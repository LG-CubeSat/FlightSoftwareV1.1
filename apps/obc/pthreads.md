# How to Make a pthread (OBC)

`threading.md` decides *which* pattern a thread should use — reactive or
periodic. This doc is the *how*: the mechanics of actually standing one up
correctly. Same spirit as `apps/adcs/include/tasks/building_a_task.md`, just
for POSIX threads instead of FreeRTOS tasks.

## The init/thread pair convention

Same shape as the FreeRTOS task convention, adapted:

```c
void some_thread_init(void);       // creates the thread, one-time setup
void *some_thread(void *arg);      // the actual loop body
```

`_init` creates the thread (and anything it needs — a mutex, a queue) and
returns immediately; it does not run the loop itself. Keep it in the same
`.c` file as the thread it starts, same as ADCS keeps `control_task_init`
next to `control_task`.

## `pthread_create`, and what to check

```c
int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                    void *(*start_routine)(void *), void *arg);
```

- **Always check the return value.** Unlike most POSIX calls, `pthread_create`
  returns the error code directly — it does not set `errno`. `if
  (pthread_create(...) != 0) { /* handle it */ }`, not `perror()`.
- **`attr`**: `NULL` is fine for default stack size on every OBC thread so
  far. Only build a real `pthread_attr_t` if you specifically know you need
  a bigger stack.
- **`arg`**: whatever you pass must outlive the thread. Never pass the
  address of a stack-local variable that goes out of scope once
  `_init` returns — pass a pointer to something `static`, heap-allocated,
  or otherwise guaranteed to live as long as the thread does.

## Join vs. detach

- A thread that owns the process's whole runtime (the daemon superloop that
  never exits under normal operation) should be **detached**:
  `pthread_detach(tid)` right after create. Nobody is going to join it, and
  detaching means its resources are reclaimed automatically if it ever does
  exit, instead of leaking.
- If the process needs an orderly shutdown (Supervisor asking a role to
  stop before its `ipc` connection closes), keep the `pthread_t` and
  `pthread_join` it after signaling the loop to exit. Signal with a plain
  flag the loop checks each iteration (`volatile sig_atomic_t` or a
  `stdatomic.h` flag) — **don't use `pthread_cancel`**; if the thread is
  mid-mutex when cancelled, whatever it was protecting can be left
  inconsistent.

## Reactive thread skeleton

```c
/* some_reactive.h */
#ifndef SOME_REACTIVE_H
#define SOME_REACTIVE_H
void some_reactive_thread_init(void);
#endif
```

```c
/* some_reactive.c */
#include "some_reactive.h"
#include <pthread.h>
#include "obc_ipc.h"

static pthread_t tid;

static void *some_reactive_thread(void *arg)
{
    (void)arg;
    for (;;) {
        ipc_msg_t msg;
        if (obc_ipc_recv(&msg) < 0) {   /* blocks here */
            continue;                    /* disconnect/error: loop and retry */
        }
        handle(&msg);
        maybe_reply(&msg);
    }
    return NULL;
}

void some_reactive_thread_init(void)
{
    int rc = pthread_create(&tid, NULL, some_reactive_thread, NULL);
    if (rc != 0) {
        fprintf(stderr, "some_reactive: pthread_create failed: %d\n", rc);
        exit(1);
    }
    pthread_detach(tid);
}
```

## Periodic thread skeleton

```c
/* some_periodic.c */
#include <time.h>
#include <pthread.h>

#define PERIOD_SEC 1

static void *some_periodic_thread(void *arg)
{
    (void)arg;
    struct timespec next;
    clock_gettime(CLOCK_MONOTONIC, &next);

    for (;;) {
        do_periodic_work();

        next.tv_sec += PERIOD_SEC;
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL);
    }
    return NULL;
}
```

Use `clock_nanosleep(..., TIMER_ABSTIME, ...)` against an absolute wake
time, not `sleep()`/`usleep()`. `sleep()` measures from "when I finished,"
so the work time silently accumulates as drift on every cycle;
`TIMER_ABSTIME` measures from "when I should have woken," so it doesn't.

## Sharing state between threads

Every OBC process with more than one thread (which is most of them, per
`threading.md`) has to protect whatever state both threads touch. The rule,
already used in `platform/sim/drivers/comms_i2c.c`'s `connection_lock`:

**Lock only long enough to read or update the shared struct — never hold a
lock across a blocking call.** Snapshot what you need, unlock, then do the
`recv()`/`send()`/`write()`:

```c
pthread_mutex_lock(&state_lock);
int snapshot = shared_value;
pthread_mutex_unlock(&state_lock);

do_blocking_io(snapshot);   /* lock is free for other threads while this runs */
```

Holding a lock across blocking I/O means any other thread that needs that
lock stalls for as long as the I/O takes — on a reactive thread blocked in
`recv()`, that can be indefinite.

## Checklist

- [ ] `pthread_create`'s return value is checked (it's a return code, not `errno`)
- [ ] Anything passed as `arg` outlives the thread (static/heap, never a stack local)
- [ ] Detached if the thread runs for the process's whole life; joined with a flag-based
      shutdown (not `pthread_cancel`) if it needs to stop cleanly
- [ ] No blocking I/O while holding a mutex another thread needs
- [ ] Reactive threads block in `recv()`/`read()` — no `sleep()`-based polling
- [ ] Periodic threads use `clock_nanosleep(TIMER_ABSTIME)`, not `sleep()`/`usleep()`
- [ ] `pthread` is linked in the target's `CMakeLists.txt`
      (`target_link_libraries(... PRIVATE pthread)`)

The last item is already true for `commands`, `data`, `compute`, `fdir`,
`mission`, and `time`'s `CMakeLists.txt` — but `apps/obc/supervisor/CMakeLists.txt`
currently has no `target_link_libraries` at all, so it won't link `pthread`
or `obc_ipc` yet. Worth fixing before writing Supervisor's threads.
