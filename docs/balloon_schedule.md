# Flight Software V1 — Balloon Launch Schedule (Aug 12 – Nov 1)

**Status:** Companion to `balloon_launch_plan.md` — that document defines *what* to build and
in what priority order; this document maps it onto a calendar. Task IDs below (e.g. `1.6`,
`B3.2`, `2C.4`) refer directly to `balloon_launch_plan.md`'s tables — read this alongside that
document, not instead of it.

**Governing constraint:** physical boards (OBC/Zynq, Comms/STM32H743, and the four subsystem
MCUs) don't arrive until September. Everything hardware-dependent is pushed as late as it can
be without threatening the November date; everything that *can* be done in SIM is front-loaded
into August so the moment hardware lands, the team is bringing up already-working software on
real silicon, not still writing it.

**Assumptions made to build this schedule** (update immediately if any of these turn out
wrong — the whole schedule shifts with them):
- Boards arrive **by the end of Week 4 (~Sep 8)**. If you get a firmer ship date, move the
  Week 4/5 boundary to match it — everything after that point is written relative to "hardware
  in hand," not a fixed date.
- Phase B2 (subsystem scaffolding) is worked by **multiple owners in parallel** once Phase B1
  lands, per `balloon_launch_plan.md`'s parallelization map — this schedule doesn't fit in 12
  weeks if one person does it serially.
- Nov 1 is the **flight-ready / go-no-go target**, not necessarily the literal launch day —
  `balloon_launch_plan.md` already recommends 2–3 weeks of buffer between "ready" and
  "launch." If the actual launch date is later than Nov 1, that gap is your buffer, not slack
  to spend on scope creep.

---

## Progress Milestones

The clear markers, in order. Each one is a fact about the system, not a date on a calendar —
use these to tell whether the schedule below is on track, not just whether the calendar pages
have turned.

| # | Milestone | Target week | Signal you've hit it |
|---|---|---|---|
| **M0** | Kickoff & hardware ordered | W1 | Plan locked; every long-lead item (boards, GPS module, battery hardware) ordered *this week*, not "soon" |
| **M1** | Foundation complete | W3 | `balloon_launch_plan.md` Phase B1 (roadmap.md 1.1–1.22) done; Foundation DoD dry run passes |
| **M2** | All-SIM full bus validated | W4 | Every board — ADCS, EPS, Thermals, Camera, Comms — passes Phase 2 DoD in SIM |
| **M3** | Boards arrive / bring-up begins | W4 (assumption) | Hardware physically on hand; Phase B5 kicked off the same day, not the same week |
| **M4** | OBC Linux boots on real hardware | W6 | PetaLinux/Yocto image boots on the real Zynq PS |
| **M5** | Comms real Zephyr build running | W6 | STM32H743 boots Zephyr + `libsatnogs-comms`, runs its SIM-validated scaffolding logic for real |
| **M6** | Real I2C bus verified | W7 | Real I2C traffic observed between two real boards |
| **M7** | First real cross-board CSP cycle | W7 | A real command/telemetry round trip between OBC and one subsystem board, on hardware |
| **M8** | All boards individually bring-up complete | W9 | Every subsystem board + Comms passes Phase 2 DoD on real hardware |
| **M9** | Real GPS integrated & field-tested | W9 | Outdoor lock achieved, position visible in downlinked telemetry |
| **M10** | Real EPS battery telemetry integrated | W9 | Voltage/current visible and plausible across a multi-hour bench run |
| **M11** | Full end-to-end system test passes | W10 | All boards + real RF ground contact, simultaneously, on real hardware |
| **M12** | Flight readiness checklist complete | W11–12 | Soak, range, cold, fault-recovery, power-budget, and onboard-logging checks all pass |
| **M13** | Go/No-Go: flight-ready | W12 (by Nov 1) | Final checklist signed off; dry run of the real flight-day commands executes clean |

---

## Week-by-Week Plan

### Phase: SIM-only (no hardware yet) — Weeks 1–4, Aug 12 – Sep 8

| Week | Dates | Focus | Key tasks | Milestone(s) |
|---|---|---|---|---|
| **W1** | Aug 12–18 | Kickoff; start Foundation | Order all long-lead hardware **today**. `balloon_launch_plan.md` 1.1–1.5 (directory/bus convention migration) | M0 |
| **W2** | Aug 19–25 | Foundation continues | 1.6–1.10 (I2C SIM backend, ADCS+OBC ported and re-verified over it), 1.11–1.12 (shared envelope) | — |
| **W3** | Aug 26–Sep 1 | Foundation finishes; scaffolding starts | 1.13–1.22 (SPI peripheral pattern, scaffolding template, test harness, CI, Foundation DoD dry run). Start Phase B2 SIM work: 2.1–2.5 for EPS/Thermals/Camera, 2C.1–2C.2 for Comms | **M1** |
| **W4** | Sep 2–8 | Full-SIM validation; hardware lands | Finish Phase B2 SIM work: 2.6–2.8 (EPS/Thermals/Camera), 2C.3–2C.6 (Comms), ADCS re-verification against the new I2C SIM backend (~3h, see `balloon_launch_plan.md`'s ADCS note). **The moment boards arrive:** kick off 6.1 (STM32 toolchain) and 6.2 (PetaLinux/Yocto — start this first, it's the longest single lead time in the whole plan) | **M2, M3** |

### Phase: Hardware bring-up & integration — Weeks 5–9, Sep 9 – Oct 13

| Week | Dates | Focus | Key tasks | Milestone(s) |
|---|---|---|---|---|
| **W5** | Sep 9–15 | OBC + Comms bring-up begins | 6.2 continues (PetaLinux/Yocto — expect this to spill past one session), 6.3 starts (Comms real Zephyr build). Start B3.1–B3.2 (GPS module confirmed, SPI driver) if OBC hardware allows | — |
| **W6** | Sep 16–22 | OBC and Comms both boot for real | Finish 6.2 and 6.3. Start 6.6 (real I2C driver, both sides) | **M4, M5** |
| **W7** | Sep 23–29 | First real bus traffic | Finish 6.6. First real I2C test between two boards. Begin 6.7 (ADCS bench bring-up — start with ADCS since its software is most mature) | **M6, M7** |
| **W8** | Sep 30–Oct 6 | Per-board bring-up fans out | 6.8 for EPS, Thermals, Camera (parallel across owners if the team allows). Continue B3.2–B3.3 (GPS wired into OBC telemetry) | — |
| **W9** | Oct 7–13 | Bring-up wraps; balloon-critical sensors go live | Finish 6.8 (all boards). B3.4 (GPS field test). B3.5–B3.6 (real EPS battery telemetry). Phase B4 (CI + TO, `balloon_launch_plan.md` 3.1–3.3 / 3.9–3.13) should be substantially done by now — it's mostly software work that only needed real Comms to point at | **M8, M9, M10** |

### Phase: Full integration & flight readiness — Weeks 10–12, Oct 14 – Nov 1

| Week | Dates | Focus | Key tasks | Milestone(s) |
|---|---|---|---|---|
| **W10** | Oct 14–20 | Everything, together, for real | 6.9: full end-to-end system test — all boards + real RF ground contact, simultaneously. Budget real slack this week; this is the first time everything runs together and will surface integration bugs | **M11** |
| **W11** | Oct 21–27 | Flight readiness checks | B6.1 (long-duration soak — start early in the week, it needs to run unattended), B6.2 (range/link test), B6.3 (cold-environment check), B6.5 (fault-recovery check), B6.6 (onboard logging backup) | — |
| **W12** | Oct 28–Nov 1 | Final checks & go/no-go | B6.4 (power budget, can pull earlier if B3.6 finished sooner), any B6 items still open, B6.7 (final go/no-go checklist + dry run) | **M12, M13** |

---

## If Hardware Slips Past Early September

The single biggest risk to this schedule is boards arriving later than Week 4. If that
happens, **don't try to compress Phase B5 — cut scope instead**, in this order (matching
`balloon_launch_plan.md`'s own must-have/should-have split):

1. **First to defer:** Camera and Thermals real bring-up. Both are scaffolding-only for this
   flight already (no real control logic) — a late arrival can leave them SIM-validated and
   physically un-flown if it comes to that, without breaking the flight's core purpose.
2. **Second to defer:** the Should-have items (real IMU/magnetometer logging on ADCS, real
   thermistor logging on Thermals) — cut these first, they were never must-have.
3. **Never cut, compress the schedule around instead:** OBC, Comms (with real RF), ADCS (as
   the bus-validation reference), EPS (with real battery telemetry), and GPS. These five are
   the actual must-have list from `balloon_launch_plan.md` — if the date gets tight, everything
   else bends around protecting these.
4. **If it's genuinely not going to fit:** raise that explicitly rather than quietly dropping
   Phase B6's flight-readiness checks to make room. A flight that skips the soak test or the
   fault-recovery check to hit a date is a worse outcome than a flight that slips a week or two.

---

Flight Software V1 — Balloon Launch Schedule. Companion to `balloon_launch_plan.md`; both are
living documents — update the week boundaries the moment a real hardware ship date is known.
