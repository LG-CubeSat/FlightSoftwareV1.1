# OBC Roles

The OBC runs as **7 separate Linux processes plus one shared library**, not one
monolithic binary. Process separation exists so a crash or hang in one role
(e.g. `compute` stuck on an image job) can't corrupt or block another role
(e.g. `fdir`'s watchdog) — `supervisor` can kill and restart a single role
without touching the rest. Every process talks to every other process only
through `ipc` (see `threading.md` for how each process is actually
structured internally, thread by thread).

## Commands
Owns the external bus: it is the master of the I2C/CSP link to the other
boards (will move to CAN in a later prototype iteration if that changes).
Its job is purely relaying, not interpreting — it decodes/forwards inbound
commands from ground or other boards to whichever internal role owns them
over `ipc`, and forwards outbound commands/telemetry from internal roles
back out over the external bus. No other process talks to the external bus
directly.

## Compute
Does the heavy lifting the MCUs can't — image compression and any other
compute-bound task. Dispatches work asynchronously so a long-running job
never blocks it from accepting a new request or a cancellation.

## Data
Manages the OBC's data storage and file system. Owns all filesystem access
so two callers can never race on the same file; every read/write goes
through `data` over `ipc` rather than another process touching the
filesystem directly.

## FDIR — Fault Detection, Isolation, and Recovery
Detects anomalies (health, memory, load) and expects a heartbeat from every
process/board (watchdog). FDIR *decides* what a fault means and what to do
about it — including running fallback scripts and mode changes — but it
does not manage OS-level process lifecycle itself. If recovering from a
fault requires restarting a process, FDIR asks `supervisor` to do it over
`ipc`; FDIR is policy, `supervisor` is mechanism (see Supervisor, below).

## Mission
In charge of the mission: runs the experiment timeline and research
autonomy by commanding the payload system. Mission state can also be
overridden manually (e.g. by ground command); when that happens, the new
state is pushed out to every other role that needs to know about it.

## Time
Keeps synchronization and time across the OBC and the other boards —
distributing a time sync on its own schedule, and answering on-demand sync
requests from anyone who needs one immediately.

## IPC — Internal Process Communication
Not a process — a shared library, linked by all 7 processes above. It is
the only way one role talks to another; it owns the local transport (a
Unix-domain-socket hub, the same shape as `platform/sim/drivers/comms_i2c.c`
uses for the external bus, just for localhost) so no process has to
implement its own socket framing. Nothing about mission/subsystem state
lives in `ipc` itself — it only moves messages.

## Supervisor
Owns OS-level process lifecycle for the other 6 processes: starts them,
periodically sweeps them for liveness over `ipc`, and restarts a
crashed or unresponsive one with backoff. Supervisor is the *mechanism*
for bringing a process up, down, or back — it does not interpret telemetry
or decide whether a subsystem is faulted; that judgment belongs to FDIR.
Supervisor acts either on its own liveness sweep (a process stopped
heartbeating) or on request (e.g. FDIR asking it to restart a specific
process).
