#include <iostream>
#include "Scheduler.h"

Scheduler::Scheduler() : tickCount_(0) {
}

void Scheduler::addTask(Task* task) {
    tasks_.push_back(task);
    waitTicks_.push_back(0);
}
int Scheduler::selectNextTask() {
    int bestIndex = -1;
    int bestPriority = 999999; // worse than any real priority

    for (size_t i = 0; i < tasks_.size(); i++) {
        Task* t = tasks_[i];
        if (t->getState() == TaskState::READY) {
            int effectivePriority = t->getPriority() - waitTicks_[i];
            if (effectivePriority < bestPriority) {
                bestPriority = effectivePriority;
                bestIndex = static_cast<int>(i);
            }
        }
    }
    return bestIndex;
}
bool Scheduler::tick() {
    tickCount_++;

    int idx = selectNextTask();
    if (idx == -1) {
        std::cout << "[Tick " << tickCount_ << "] Scheduler: no READY tasks."
                  << std::endl;
        return false;
    }
    //every ready task that was not selected to run this tick should have its waitTicks incremented
    for (size_t i = 0; i < tasks_.size(); i++) {
        if (static_cast<int>(i) != idx && tasks_[i]->getState() == TaskState::READY) {
            waitTicks_[i]++;
        }
    }
    Task* chosen = tasks_[idx];
    chosen->setState(TaskState::RUNNING);

    std::cout << "[Tick " << tickCount_ << "] Scheduler: Running Task("
              << chosen->getName() << ", priority=" << chosen->getPriority()
              << ")" << std::endl;

    chosen->run();
    waitTicks_[idx] = 0;
    if (chosen->getState() == TaskState::RUNNING) {
        chosen->setState(TaskState::READY);
    }

    return true;
}

void Scheduler::run(int numTicks) {
    for (int i = 0; i < numTicks; ++i) {
        tick();
    }
}

int Scheduler::getTickCount() const {
    return tickCount_;
}