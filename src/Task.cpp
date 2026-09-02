#include "Task.h"

Task::Task(int id, int priority, const std::string& name)
    : id_(id), priority_(priority), name_(name), state_(TaskState::READY) {
}

int Task::getId() const {
    return id_;
}

int Task::getPriority() const {
    return priority_;
}

const std::string& Task::getName() const {
    return name_;
}

TaskState Task::getState() const {
    return state_;
}

void Task::setState(TaskState newState) {
    state_ = newState;
}