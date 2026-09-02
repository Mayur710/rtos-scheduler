/*purpose is to periodically wake up, ask a sensor for data over I2C, stitch that data together into a readable number, and 
print it*/
#ifndef SENSOR_READ_TASK_H
#define SENSOR_READ_TASK_H
#include <iostream>
#include "Task.h"
#include "HAL/I2CDriver.h"

/*simulates a periodic sensor poll over I2C holds a reference to an I2CDriver rather than a concrete MockI2C class so that 
it can be used with a real I2C driver in production code*/
class SensorReadTask : public Task {
public:
    SensorReadTask(int id, int priority, I2CDriver& i2c, uint8_t deviceAddr)
        : Task(id, priority, "SensorReadTask"),
          i2c_(i2c),
          deviceAddr_(deviceAddr) {
    }
    void run() override {

        std::vector<uint8_t> raw = i2c_.read(deviceAddr_, 2);

        int value = (raw[0] << 8) | raw[1];
        std::cout << "[SensorReadTask] Parsed sensor value: " << value
                   << std::endl;
    }

private:
    I2CDriver& i2c_;
    uint8_t deviceAddr_;
};

#endif