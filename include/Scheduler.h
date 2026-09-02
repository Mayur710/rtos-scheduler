#ifndef SCHEDULER_H
#define SCHEDULER_H
#include "Task.h"
#include <vector>

/*this is a coorporative scheduler i.e it will not preempt a task mid execution 
scheduling policy used is priority-based round-robin
i.e - Among all tasks in ready state, pick the one with the lowest priority value (0 = highest priority).
On a tie, fall back to round-robin order (whoever was added first this round waits, next one runs) so no task starves*/

class Scheduler{
public:
    Scheduler();
    void addTask(Task* task);
    //picks the next READY task by priority and calls its run() returns false if no READY task was available to run this tick
    bool tick();
    //run scheduler for fixed number of ticks 
    void run(int numTicks);
    int getTickCount() const;

private:
    std::vector<Task*> tasks_;
    std::vector<int>waitTicks_;//tells how long each task has waited since last run 
    int tickCount_;
    //pick the index of the next ready task to run 
    int selectNextTask();
};

#endif