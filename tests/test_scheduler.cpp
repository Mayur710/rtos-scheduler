/*
 * Unit / regression tests for the cooperative scheduler.
 *
 * No external framework: every CHECK records file:line on failure (to stderr,
 * so it survives the output-capturing helpers) and main() exits non-zero if
 * anything failed, so CI can gate on it.
 *
 * The two headline tests back the resume claims directly:
 *   test_fixed_priority_starves_low_priority()  -> 1000 / 0 / 0    (the bug)
 *   test_aging_prevents_starvation()            -> 501 / 250 / 249 (the fix)
 *                                                 max 3-tick skip
 *
 * Build:  g++ -std=c++17 -Wall -Wextra -Wpedantic -Iinclude \
 *             src/Task.cpp src/Scheduler.cpp tests/test_scheduler.cpp
 */
#include "Scheduler.h"
#include "Task.h"

#include <algorithm>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

int g_checks = 0;
int g_failures = 0;

void check(bool cond, const char* expr, const char* file, int line) {
    ++g_checks;
    if (!cond) {
        ++g_failures;
        std::cerr << "  FAIL " << file << ":" << line << "  " << expr << "\n";
    }
}

#define CHECK(cond) check((cond), #cond, __FILE__, __LINE__)

// Mutes the scheduler's per-tick tracing while a scenario runs.
// Failures go to stderr, so they are never swallowed by this.
class Quiet {
public:
    Quiet() : saved_(std::cout.rdbuf(captured_.rdbuf())) {}
    ~Quiet() { std::cout.rdbuf(saved_); }
private:
    std::ostringstream captured_;
    std::streambuf* saved_;
};

// Minimal concrete task: counts how many times the scheduler ran it.
class CountingTask : public Task {
public:
    CountingTask(int id, int priority, const std::string& name)
        : Task(id, priority, name), runs_(0) {}
    void run() override { ++runs_; }
    int runs() const { return runs_; }
private:
    int runs_;
};

struct Stats {
    int runs = 0;
    int maxSkipped = 0;      // longest run of consecutive ticks NOT selected
    int firstRunTick = -1;   // cold-start latency (1-based)
};

// Runs `ticks` ticks at the given aging rate and records per-task behaviour.
std::vector<Stats> measure(int agingRatePerTick, int ticks,
                           std::vector<CountingTask*>& tasks) {
    Quiet quiet;

    Scheduler scheduler(agingRatePerTick);
    for (size_t k = 0; k < tasks.size(); ++k) {
        scheduler.addTask(tasks[k]);
    }

    std::vector<Stats> stats(tasks.size());
    std::vector<int> lastRun(tasks.size(), -1);

    for (int i = 0; i < ticks; ++i) {
        scheduler.tick();
        const int tick = scheduler.getTickCount();  // 1-based
        for (size_t k = 0; k < tasks.size(); ++k) {
            const int now = tasks[k]->runs();
            if (now > stats[k].runs) {
                if (lastRun[k] >= 0) {
                    stats[k].maxSkipped =
                        std::max(stats[k].maxSkipped, tick - lastRun[k] - 1);
                } else {
                    stats[k].firstRunTick = tick;
                }
                lastRun[k] = tick;
                stats[k].runs = now;
            }
        }
    }
    return stats;
}

// --- tests ---------------------------------------------------------------

// Highest priority wins the first tick; state is reset to READY after run();
// the tick counter advances exactly once per tick.
void test_selection_state_and_tick_count() {
    CountingTask hi(1, 0, "hi");
    CountingTask lo(2, 2, "lo");

    bool tickReturned = false;
    int ticksAfterRun = 0;
    TaskState stateAfterRun = TaskState::DONE;
    int hiRuns = 0;
    int loRuns = 0;
    {
        Quiet quiet;
        Scheduler scheduler;                    // default aging rate
        scheduler.addTask(&hi);
        scheduler.addTask(&lo);
        tickReturned = scheduler.tick();
        hiRuns = hi.runs();
        loRuns = lo.runs();
        stateAfterRun = hi.getState();
        scheduler.run(9);                       // 10 ticks total
        ticksAfterRun = scheduler.getTickCount();
    }

    CHECK(tickReturned);
    CHECK(hiRuns == 1);
    CHECK(loRuns == 0);                          // lower priority waits its turn
    CHECK(stateAfterRun == TaskState::READY);    // RUNNING -> READY reset
    CHECK(ticksAfterRun == 10);
}

// With nothing READY, tick() must report it (false) and run nothing.
void test_no_ready_task() {
    CountingTask done(1, 0, "done");
    done.setState(TaskState::DONE);

    bool tickReturned = true;
    int ticks = 0;
    {
        Quiet quiet;
        Scheduler scheduler;
        scheduler.addTask(&done);
        tickReturned = scheduler.tick();
        ticks = scheduler.getTickCount();
    }

    CHECK(tickReturned == false);
    CHECK(done.runs() == 0);
    CHECK(ticks == 1);            // the tick still happened, nothing ran
}

// Equal priorities: registration order breaks the tie on the first tick,
// and aging makes the two tasks alternate afterwards (fairness from aging,
// not from the tie-break).
void test_tie_break_then_fairness_from_aging() {
    CountingTask first(1, 5, "first");
    CountingTask second(2, 5, "second");

    {
        Quiet quiet;
        Scheduler scheduler(1);
        scheduler.addTask(&first);
        scheduler.addTask(&second);
        scheduler.tick();                          // tick 1: tie -> first wins
        scheduler.run(3);                          // ticks 2..4
    }

    CHECK(first.runs() == 2);                      // strict priority alone
    CHECK(second.runs() == 2);                     // ...would starve `second`
    CHECK(first.runs() + second.runs() == 4);
}

// --- the two claims from the resume --------------------------------------

// BEFORE: aging disabled (rate 0) == pure fixed priority == the starvation bug.
// The p=0 task is selected on every single tick; the others never run.
void test_fixed_priority_starves_low_priority() {
    CountingTask sensor(1, 0, "SensorReadTask");
    CountingTask log(2, 1, "LogTask");
    CountingTask blink(3, 2, "BlinkTask");
    std::vector<CountingTask*> tasks{&sensor, &log, &blink};

    const std::vector<Stats> s = measure(0, 1000, tasks);

    CHECK(s[0].runs == 1000);   // sensor monopolises the CPU
    CHECK(s[1].runs == 0);      // starved: 0 of 1000 ticks
    CHECK(s[2].runs == 0);      // starved: 0 of 1000 ticks
}

// AFTER: default aging (1 level/tick).
//  - no task starves
//  - worst-case skip is bounded (measured 3 ticks for p=1 and p=2)
//  - priority is still respected: the urgent task runs most
void test_aging_prevents_starvation() {
    CountingTask sensor(1, 0, "SensorReadTask");
    CountingTask log(2, 1, "LogTask");
    CountingTask blink(3, 2, "BlinkTask");
    std::vector<CountingTask*> tasks{&sensor, &log, &blink};

    const std::vector<Stats> s = measure(1, 1000, tasks);

    // Golden values from the measured run (deterministic, see README table).
    CHECK(s[0].runs == 501);                    // 50.1%
    CHECK(s[1].runs == 250);                    // 25.0%
    CHECK(s[2].runs == 249);                    // 24.9%

    // Bounded latency. Analytical floor for p=2 vs p=just-ran=0 is
    // p - p_hi + 1 = 3 ticks; measured worst case is 3 for both low tasks.
    CHECK(s[1].maxSkipped <= 3);
    CHECK(s[2].maxSkipped <= 3);
    CHECK(s[0].maxSkipped <= 1);                // sensor runs every other tick

    // Cold start: sensor tick 1, log tick 3, blink tick 5.
    CHECK(s[0].firstRunTick == 1);
    CHECK(s[1].firstRunTick == 3);
    CHECK(s[2].firstRunTick == 5);

    // Priority semantics preserved (aging biases, it does not flatten).
    CHECK(s[0].runs > s[1].runs);
    CHECK(s[1].runs >= s[2].runs);

    // Every task gets a real share of the CPU (period-4 steady state).
    // Without aging these are 1000 / 0 / 0.
    CHECK(s[0].runs >= 490 && s[1].runs >= 245 && s[2].runs >= 240);
}

}  // namespace

int main() {
    std::cout << "Scheduler test suite\n";

    test_selection_state_and_tick_count();
    test_no_ready_task();
    test_tie_break_then_fairness_from_aging();
    test_fixed_priority_starves_low_priority();   // the bug  (before)
    test_aging_prevents_starvation();             // the fix  (after)

    const int passed = g_checks - g_failures;
    std::cout << passed << "/" << g_checks << " checks passed\n";
    if (g_failures != 0) {
        std::cerr << "FAILED: " << g_failures << " check(s)\n";
        return 1;
    }
    std::cout << "ALL TESTS PASSED\n";
    return 0;
}
