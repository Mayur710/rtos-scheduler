# Cooperative Priority Scheduler with Aging (RTOS Task Model, C++)

[![build-and-test](https://github.com/Mayur710/rtos-scheduler/actions/workflows/ci.yml/badge.svg)](https://github.com/Mayur710/rtos-scheduler/actions/workflows/ci.yml)

**Repository:** https://github.com/Mayur710/rtos-scheduler

A lightweight, object-oriented, **cooperative (non-preemptive)** task scheduler
in C++ that *models* core RTOS concepts — task states, priority-based
selection with linear aging for starvation prevention — plus a hardware
abstraction layer (HAL) with abstract driver interfaces (UART, I2C, SPI) and
mock implementations.

The word "model" is deliberate: this is not a hard real-time system. See
[What this is *not*](#what-this-is-not) for the exact scope, and
[Measured Results](#measured-results-1000-tick-run) for numbers you can
reproduce yourself with `make compare`.

Built as a learning/portfolio project targeting embedded software roles that
require RTOS familiarity, OOP-based embedded design, and protocol-level
driver interfacing.

---

## Objective

Demonstrate, in software (no physical hardware required), the core building
blocks of an RTOS-based embedded system:

- A **task abstraction** with a proper state machine (`READY`, `RUNNING`,
  `BLOCKED`, `DONE`)
- A **scheduler** that selects tasks using a real scheduling policy
- A **HAL (Hardware Abstraction Layer)** that decouples application/task
  logic from the underlying driver implementation — so a `MockUART` today
  can be swapped for a real hardware UART driver later with zero changes
  to task code

---

## What this is *not*

Stated up front so the scope is unambiguous:

- **Not preemptive.** Once selected, a task runs to completion for that tick.
  No context switch, no interrupt-driven rescheduling, no preemption point.
- **No deadlines, no EDF, no rate-monotonic analysis.** Tasks have a static
  priority, not a deadline or a period, so no schedulability (response-time)
  analysis is claimed. "Real-time" here means *real-time concepts*, not a
  hard real-time guarantee.
- **No time base.** A "tick" is a scheduling quantum, not a wall-clock
  interval — there is no hardware timer and no WCET instrumentation.
- **No synchronization primitives.** No mutexes, therefore no priority
  inheritance / priority-inversion handling (see
  [Porting to a real MCU](#porting-to-a-real-mcu-hal-design-notes) for how
  that would be added).
- **`BLOCKED` and `DONE` are declared, not exercised.** Every current task
  runs forever, so only `READY`/`RUNNING` are reached in practice, and there
  is no wake-up path out of `BLOCKED` yet.

What it *does* provide is a **measured** guarantee: with aging enabled the
worst-case number of ticks a task can be skipped is bounded (3 ticks,
measured over 1,000 ticks), while fixed-priority selection starves
low-priority tasks indefinitely (0 of 1,000 runs). See
[Measured Results](#measured-results-1000-tick-run).

---

## Architecture

```
main.cpp
   |
   v
Scheduler ---- manages ----> Task (abstract base class)
                                 |
                +----------------+----------------+
                |                |                 |
         SensorReadTask       LogTask          BlinkTask
           (uses I2C)        (uses UART)      (uses GPIO, no HAL)
                |                |
                v                v
           I2CDriver        UARTDriver          SPIDriver
          (interface)       (interface)        (interface, unused
                |                |               by current tasks)
                v                v
            MockI2C          MockUART            MockSPI
        (console-based)   (console-based)    (console-based)
```

**Design pattern used throughout:** every layer depends on an abstract
interface, not a concrete implementation (dependency injection). The
`Scheduler` only ever holds `Task*` pointers and calls `run()`
polymorphically. `SensorReadTask` and `LogTask` are given their drivers
(`I2CDriver&`, `UARTDriver&`) through their constructors rather than
creating them internally — meaning swapping `MockI2C` for a real hardware
I2C driver requires no changes to `SensorReadTask` at all.

---

## Project Structure

```
rtos-scheduler-cpp/
├── include/
│   ├── Task.h                  # Abstract Task base class
│   ├── Scheduler.h             # Scheduler class
│   ├── HAL/
│   │   ├── UARTDriver.h        # Abstract UART interface
│   │   ├── I2CDriver.h         # Abstract I2C interface
│   │   ├── SPIDriver.h         # Abstract SPI interface
│   │   ├── MockUART.h          # Console-based UART implementation
│   │   ├── MockI2C.h           # Console-based I2C implementation
│   │   └── MockSPI.h           # Console-based SPI implementation
│   └── tasks/
│       ├── SensorReadTask.h    # Concrete task: simulated I2C sensor poll
│       ├── LogTask.h           # Concrete task: simulated UART logging
│       └── BlinkTask.h         # Concrete task: simulated GPIO toggle
├── src/
│   ├── Task.cpp
│   ├── Scheduler.cpp
│   └── main.cpp
├── tests/
│   └── test_scheduler.cpp     # unit + regression tests (no framework, assert-style)
├── tools/
│   └── compare_aging.cpp      # prints the starvation before/after table
├── .github/workflows/ci.yml   # CI: strict-warning build + tests + comparison
├── Makefile
└── README.md
```

---

## Scheduling Policy: Priority Scheduling with Aging

Tasks are picked by lowest **effective priority** (0 = most urgent):

```
effective_priority = base_priority - agingRatePerTick * ticks_waited_since_last_run
```

A pure "always pick lowest base priority" scheduler causes **starvation** —
a high-priority task (`SensorReadTask`, priority 0) would run on every
single tick forever, and lower-priority tasks would never execute. This was
an actual bug encountered and fixed during development (see
[Challenges](#challenges--what-i-learned) below).

**Aging** solves this: every tick a task waits without running, its
effective priority number decreases, making it look progressively more
urgent. Eventually even a low-priority task's effective priority beats a
high-priority task that just ran — guaranteeing every task gets scheduled
periodically, without ever fully ignoring priority.

### Parameters (exactly as implemented in `Scheduler`)

| Parameter | Value |
|---|---|
| Priority range | `0 … 2` — **0 = most urgent** (`SensorReadTask`=0, `LogTask`=1, `BlinkTask`=2) |
| Aging rate | **1 priority level per tick waited** (`agingRatePerTick`, constructor argument, default `1`) |
| Reset | a task's waited-tick counter resets to `0` on the tick it runs |
| Tie-break | deterministic: **registration order** (the task passed to `addTask` first wins) |
| Fairness source | aging, *not* the tie-break — equal-priority tasks alternate because each missed tick improves their effective priority |
| Aging disabled | `Scheduler scheduler(0);` → pure fixed priority → the original starvation bug |

### Worst-case latency bound

A task with base priority `p` that has waited `w` ticks has effective
priority `p − w`, while a task that just ran sits at its base priority. The
waiting task wins once `p − w < p_just_ran`, i.e. after
`p − p_just_ran + 1` ticks. For the lowest-priority task
(`p = 2`) against a just-run `p = 0` task that is **3 ticks**. Because every
other task also ages while waiting, ties can add a tick, so the tests assert
the *measured* bound (3) rather than the closed form.

---

## Measured Results (1,000-tick run)

Produced by `make compare` (`tools/compare_aging.cpp`), which drives the real
`SensorReadTask` / `LogTask` / `BlinkTask` through the production code path
twice — aging off and aging on — over 1,000 ticks, and counts which task the
scheduler picked on every tick:

| Scenario | SensorReadTask (p=0) | LogTask (p=1) | BlinkTask (p=2) | Max ticks skipped |
|---|---|---|---|---|
| **BEFORE** — aging off (`agingRatePerTick = 0`), the original bug | 1000 / 1000 (100%) | **0 — starved** | **0 — starved** | unbounded |
| **AFTER** — aging on (`agingRatePerTick = 1`, default) | **501 (50.1%)** | **250 (25.0%)** | **249 (24.9%)** | **3 ticks** (Log, Blink), 1 tick (Sensor) |

Additional facts from the same run:

- After a 2-tick transient the schedule becomes **periodic with period 4**:
  `Sensor, Log, Sensor, Blink` repeating → 50% / 25% / 25%.
- **Cold-start (first run) latency:** Sensor = tick 1, Log = tick 3,
  Blink = tick 5.
- Priority semantics are preserved: the urgent sensor task still gets **half**
  the ticks, while the low-priority tasks go from **∞ (starvation) to a
  bounded 3 ticks**.

### Reproduce

```bash
make compare     # prints both scenarios + PASS/FAIL, exits non-zero on mismatch
make test        # unit + regression tests, incl. the starvation before/after
```

Both are also run by CI (`.github/workflows/ci.yml`) with
`-Wall -Wextra -Wpedantic -Werror`.

---

## Sample Output (`./rtos_scheduler`, 9 ticks, verbatim)

```
=== RTOS-style Cooperative Scheduler Simulation ===
[Tick 1] Scheduler: Running Task(SensorReadTask, priority=0)
[I2C] Read 2 bytes from addr 0x48 -> [0x1a, 0x1b]
[SensorReadTask] Parsed sensor value: 6683
[Tick 2] Scheduler: Running Task(SensorReadTask, priority=0)
[I2C] Read 2 bytes from addr 0x48 -> [0x1a, 0x1b]
[SensorReadTask] Parsed sensor value: 6683
[Tick 3] Scheduler: Running Task(LogTask, priority=1)
[UART] TX: "Log entry #1"
[Tick 4] Scheduler: Running Task(SensorReadTask, priority=0)
[I2C] Read 2 bytes from addr 0x48 -> [0x1a, 0x1b]
[SensorReadTask] Parsed sensor value: 6683
[Tick 5] Scheduler: Running Task(BlinkTask, priority=2)
[GPIO] Pin 13 toggled -> HIGH
[Tick 6] Scheduler: Running Task(SensorReadTask, priority=0)
[I2C] Read 2 bytes from addr 0x48 -> [0x1a, 0x1b]
[SensorReadTask] Parsed sensor value: 6683
[Tick 7] Scheduler: Running Task(LogTask, priority=1)
[UART] TX: "Log entry #2"
[Tick 8] Scheduler: Running Task(SensorReadTask, priority=0)
[I2C] Read 2 bytes from addr 0x48 -> [0x1a, 0x1b]
[SensorReadTask] Parsed sensor value: 6683
[Tick 9] Scheduler: Running Task(BlinkTask, priority=2)
[GPIO] Pin 13 toggled -> LOW
 Simulation complete after 9 ticks
```

`SensorReadTask` (highest priority) still runs most frequently, but
`LogTask` and `BlinkTask` are no longer starved — they run periodically as
their effective priority "ages up."

---

## Build & Run

### Requirements
- A C++17-capable compiler (`g++` or equivalent)

### Option 1 — Manual compile (works everywhere, incl. Windows without `make`)

```bash
g++ -std=c++17 -Wall -Wextra -Iinclude src/Task.cpp src/Scheduler.cpp src/main.cpp -o rtos_scheduler
./rtos_scheduler        # Linux/macOS
.\rtos_scheduler.exe    # Windows PowerShell
```

### Option 2 — Using the Makefile (Linux/macOS, or Windows with `make` installed)

```bash
make run
make test        # build + run the unit/regression tests
make compare     # build + print the starvation before/after table
```

### Option 3 — Tests, without `make`

```bash
g++ -std=c++17 -Wall -Wextra -Wpedantic -Iinclude \
    src/Task.cpp src/Scheduler.cpp tests/test_scheduler.cpp -o test_scheduler
./test_scheduler
```

---

## Key Concepts Demonstrated

- **OOP in embedded software design** — abstract base classes, inheritance,
  polymorphism (`Task`, `UARTDriver`, `I2CDriver`, `SPIDriver`)
- **RTOS task state machine** — `READY` / `RUNNING` / `BLOCKED` / `DONE`
  (`BLOCKED`/`DONE` are declared but not exercised yet — no wake-up path,
  see [What this is *not*](#what-this-is-not))
- **Priority-based scheduling with starvation prevention (aging)** — the
  guarantee is measured, not asserted: worst-case skip of **3 ticks** vs.
  unbounded without aging ([Measured Results](#measured-results-1000-tick-run))
- **Hardware Abstraction Layer (HAL) pattern** — driver interfaces separated
  from concrete (mock) implementations, enabling hardware-free testing
- **Dependency injection** — tasks receive their drivers via constructor
  rather than instantiating them internally
- **Simulated protocol-level interfacing** — UART (byte stream) and I2C
  (addressed, byte buffer) are exercised end-to-end by tasks. The SPI
  interface (`SPIDriver::transfer`, full-duplex + chip-select) is defined
  with a mock implementation but **is not yet driven by any task** — listed
  under [Possible Extensions](#possible-extensions)

---

## Challenges & What I Learned

During development, the initial priority scheduler caused **task
starvation** — the highest-priority task ran on every tick, and lower-
priority tasks never executed. I diagnosed this by tracing scheduler
selection logic tick-by-tick and fixed it by implementing **priority
aging**, a technique used in real RTOS schedulers to guarantee fairness
without discarding priority semantics. This also surfaced a subtle sign
error (`+` instead of `-` in the aging formula) and a missing state-reset
bug, both fixed after tracing actual vs. expected output — a good example
of debugging scheduling logic the way you would on real embedded firmware.

**How it is verified now** (rather than asserted from memory):

- `test_fixed_priority_starves_low_priority` runs the same three tasks with
  `agingRatePerTick = 0` and checks **1000 / 0 / 0** — the bug, reproduced
  on demand.
- `test_aging_prevents_starvation` runs them with the default
  `agingRatePerTick = 1` and checks **501 / 250 / 249** plus **max 3-tick
  skip** — the fix.
- `make compare` prints both tables side by side through the production code
  path (mock HAL included) and exits non-zero if either number changes.
- CI runs the build with `-Wall -Wextra -Wpedantic -Werror` and both of the
  above on every push.

---

## Porting to a real MCU (HAL design notes)

Anticipated questions about an OOP / virtual-function HAL on bare metal, and
the answers as this project stands:

**Heap**
- Every driver and task object is **stack-allocated in `main`** and injected
  **by reference** (`UARTDriver&`, `I2CDriver&`). There is no `new`, no
  factory, and **no heap use for object lifetime**. The `Scheduler` holds
  **non-owning** `Task*` — it never deletes them; `main` owns their lifetime.
- What *does* allocate today is `std::vector` (returned by
  `I2CDriver::read`) and `std::string` (passed to `UARTDriver::write`).
  On target these become `std::array<uint8_t, N>` / a read-only view and
  `const char*` + fixed buffer, so no allocation per transfer — and none in
  ISR context.

**Virtual dispatch**
- Cost is one `vptr` load + one indirect call **per transfer, not per byte**.
  An I2C byte at 400 kHz costs ~24 µs on the wire; the dispatch is a few
  cycles. The scheduler issues at most one `run()` dispatch per task per tick.
- No RTTI anywhere (`dynamic_cast` / `typeid` unused); on target the build
  would use `-fno-exceptions -fno-rtti` plus static allocation.
- If WCET analyzability mattered more than runtime extensibility, the
  escalation path is: (1) LTO + `-fvirtual-function-elimination` when the
  concrete type is visible at the call site, (2) CRTP / templates for
  compile-time polymorphism with devirtualized calls, (3) a `const`
  function-pointer table in flash — same indirection cost, no vtable in RAM,
  and every possible target address statically enumerable for analysis.

**Blocking / priority inversion**
- Not reachable today: the scheduler is cooperative, so a "lock holder" always
  runs to completion (the middle step of the classic inversion chain needs
  preemption), and there are no locks to contend over.
- Adding them would mean a mutex with **priority inheritance** (holder boosted
  to the highest waiter's priority until release) or the **priority ceiling
  protocol** — applied *on top of* aged priorities, since aging already makes
  effective priority dynamic.

**Scaling**
- `selectNextTask()` is an O(N) scan over a `std::vector` — appropriate for
  small task counts and trivially auditable. For a larger fixed set I'd move
  to priority-indexed ready queues plus a ready-priority bitmap (`CLZ` to find
  the highest ready priority) for O(1) selection, the way FreeRTOS does it.

---

## Possible Extensions

- Port `BlinkTask`/`MockUART` to real hardware (Arduino Uno) for a
  hardware-validated version
- Add a preemptive mode with simulated context switching
- Add task delays / periodic scheduling (`BLOCKED` state with wake-up ticks)
- Drive `SPIDriver`/`MockSPI` from a task so SPI is exercised end-to-end
- Priority-inheritance mutex, to demonstrate priority-inversion handling
- Priority-indexed ready queue + bitmap for O(1) task selection
