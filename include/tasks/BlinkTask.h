#ifndef BLINK_TASK_H
#define BLINK_TASK_H

#include <iostream>
#include "Task.h"

//simulates toggling a GPIO pin 
class BlinkTask : public Task {
public:
    BlinkTask(int id, int priority, int pin)
        : Task(id, priority, "BlinkTask"),
          pin_(pin),
          ledState_(false) {
    }
    void run() override {
        ledState_ = !ledState_;
        std::cout << "[GPIO] Pin " << pin_ << " toggled -> "
                   << (ledState_ ? "HIGH" : "LOW") << std::endl;
    }
private:
    int pin_;
    bool ledState_;
};

#endif