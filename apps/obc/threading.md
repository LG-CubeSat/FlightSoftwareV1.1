# OBC Threading Model

Each OBC process (see `roles.md`) is internally built from two kinds of
thread. Pick per-thread, not per-process — most processes end up with one
of each.

**Reactive thread** — the thread's whole job is "wait for a message, act on
it." Block on `recv()`/`read()` from `ipc` (or the external bus, for
`commands`). No sleep, no polling: the kernel wakes the thread when data
arrives, so idle cost is zero and response latency is just arrival latency.

```c
for (;;) {
    ssize_t n = recv(sock_fd, buf, sizeof(buf), 0);   // blocks here
    if (n <= 0) { /* handle disconnect/error, reconnect */ continue; }
    decode(buf, n, &msg);
    handle(&msg);
    maybe_reply(&msg);
}
```

**Periodic thread** — the thread has to do something on a clock regardless
of whether a message ever arrives (a heartbeat, a watchdog deadline). Use
`clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, ...)` against an absolute
next-wake time, not `sleep()` — otherwise processing time accumulates as
drift on every cycle.

```c
struct timespec next; clock_gettime(CLOCK_MONOTONIC, &next);
for (;;) {
    do_periodic_work();
    next.tv_sec += period_sec;
    clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL);
}
```

A process can, and often should, run both at once — one thread blocked on
`ipc`, another on a timer.

## Per-process breakdown

| Process | Reactive | Periodic |
|---|---|---|
| **Commands** | Two threads: one blocks on the external bus and forwards decoded messages onto `ipc`; the other blocks on `ipc` and forwards outbound messages onto the external bus. | — |
| **IPC** | — (library, not a process; provides the transport every thread above blocks on) | — |
| **Data** | One thread blocks on `ipc` for read/write/list requests. Actual filesystem access is mutex-serialized so two requests can never race on the same file. | — |
| **Compute** | Two threads: a dispatcher blocks on `ipc` for work requests; a separate worker thread does the actual compute (e.g. image compression), so a long job never blocks the dispatcher from accepting a new request or a cancellation. | — |
| **FDIR** | Worker thread that executes a recovery action (fallback script, mode change, or a restart request to Supervisor) once a fault is confirmed. | Two threads: a checker that evaluates incoming health/telemetry for anomalies, and a watchdog that expects a heartbeat from every process/board and flags a timeout. |
| **Mission** | Manual-override thread: when mission state is overridden (e.g. by ground command), updates local state and pushes it out to every other role over `ipc`. | Scheduler thread that dispatches time-tagged mission events. Not a fixed tick — sleep until the next due timestamp (min-heap top); the effective rate changes with mission phase, and a newly-scheduled near-term event must be able to wake the thread early rather than waiting out a long sleep. |
| **Time** | Sync-request thread: pushes an on-demand time sync reply when asked. | Beacon thread: pushes a time sync out on a fixed interval. |
| **Supervisor** | Control thread: when queried (by FDIR, or a ground command), can start/stop/restart another process. | Heartbeat-sweep thread: periodically polls every other process over `ipc` for liveness; a missed heartbeat triggers a restart with backoff. |
