#ifndef LOG_TASK_H
#define LOG_TASK_H

#include "Task.h"
#include "HAL/UARTDriver.h"

/*simulates a task that periodically sends a status/log message out over UART using
 * the same dependency-injection pattern as SensorReadTask*/
class LogTask : public Task {
public:
    LogTask(int id, int priority, UARTDriver& uart)
        : Task(id, priority, "LogTask"),
          uart_(uart),
          messageCount_(0) {
    }

    void run() override {
        messageCount_++;
        std::string msg = "Log entry #" + std::to_string(messageCount_);
        uart_.write(msg);
    }

private:
    UARTDriver& uart_;
    int messageCount_;
};

#endif