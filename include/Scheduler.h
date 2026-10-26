#ifndef SCHEDULER_H
#define SCHEDULER_H
#include "Task.h"
#include <vector>

/*Cooperative scheduler: a task is NEVER preempted mid-execution.
Each tick selects exactly one READY task and lets it run (a task can also
yield early by changing its own state inside run()).

Scheduling policy: fixed priority with linear aging.
    effective_priority = base_priority - agingRatePerTick * ticks_waited
(0 = most urgent; the LOWEST effective value wins).

Ties are resolved deterministically by registration order (the task added
first wins). Fairness does NOT come from that tie-break -- it comes from
aging: every tick a task waits without running, its effective priority
improves by `agingRatePerTick`, so after enough ticks it outranks a task
that just ran. That is what bounds worst-case scheduling latency.
(See README "Measured Results" for the measured bound: 3 ticks.)*/

class Scheduler{
public:
    //agingRatePerTick: priority levels a READY task gains per tick it waits
    //  1 = aging enabled (default): prevents starvation, bounds latency
    //  0 = aging disabled: pure fixed priority, reproduces the original bug
    explicit Scheduler(int agingRatePerTick = 1);
    void addTask(Task* task);
    //picks the next READY task by priority and calls its run(); returns false if no READY task was available to run this tick
    bool tick();
    //run scheduler for fixed number of ticks
    void run(int numTicks);
    int getTickCount() const;

private:
    std::vector<Task*> tasks_;      //NON-OWNING pointers: callers own the tasks
    std::vector<int>waitTicks_;//tells how long each task has waited since last run
    int tickCount_;
    int agingRatePerTick_;
    //pick the index of the next ready task to run
    int selectNextTask();
};

#endif
