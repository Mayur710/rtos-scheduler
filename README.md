# RTOS-Style Cooperative Task Scheduler (C++)

A lightweight, object-oriented, cooperative task scheduler in C++ that simulates
core real-time operating system (RTOS) concepts — task states, priority-based
scheduling with starvation prevention, and a hardware abstraction layer (HAL)
for common embedded protocols (UART, I2C, SPI).

---

## Objective

Demonstrate, in software (no physical hardware required), the core building
blocks of an embedded real-time system:

- A **task abstraction** with a proper state machine (`READY`, `RUNNING`,
  `BLOCKED`, `DONE`)
- A **scheduler** that selects tasks using a real scheduling policy
- A **HAL (Hardware Abstraction Layer)** that decouples application/task
  logic from the underlying driver implementation — so a `MockUART` today
  can be swapped for a real hardware UART driver later with zero changes
  to task code

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
├── Makefile
└── README.md
```

---

## Scheduling Policy: Priority Scheduling with Aging

Tasks are picked by lowest **effective priority** (0 = most urgent):

```
effective_priority = base_priority - ticks_waited_since_last_run
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

---

## Sample Output

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
=== Simulation complete after 9 ticks ===
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
```

---

## Key Concepts Demonstrated

- **OOP in embedded software design** — abstract base classes, inheritance,
  polymorphism (`Task`, `UARTDriver`, `I2CDriver`, `SPIDriver`)
- **RTOS task state machine** — `READY` / `RUNNING` / `BLOCKED` / `DONE`
- **Priority-based scheduling with starvation prevention (aging)**
- **Hardware Abstraction Layer (HAL) pattern** — driver interfaces separated
  from concrete (mock) implementations, enabling hardware-free testing
- **Dependency injection** — tasks receive their drivers via constructor
  rather than instantiating them internally
- **Simulated protocol-level interfacing** — UART (byte stream), I2C
  (addressed, byte buffer), SPI (full-duplex transfer with chip-select)

---



---

## Possible Extensions

- Port `BlinkTask`/`MockUART` to real hardware (Arduino Uno) for a
  hardware-validated version
- Add a preemptive mode with simulated context switching
- Add task delays / periodic scheduling (`BLOCKED` state with wake-up ticks)
- Unit tests for `Scheduler::selectNextTask()` scheduling decisions
