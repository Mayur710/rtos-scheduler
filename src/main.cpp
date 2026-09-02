#include <iostream>
#include "Scheduler.h"
#include "HAL/MockUART.h"
#include "HAL/MockI2C.h"
#include "tasks/SensorReadTask.h"
#include "tasks/LogTask.h"
#include "tasks/BlinkTask.h"

int main() {
    std::cout << "=== RTOS-style Cooperative Scheduler Simulation ===\n" << std::endl;

    /*reating  mock hardware drivers these stand in for real UART/I2C peripherals. Tasks only know about the abstract 
    UARTDriver/I2CDriver interfaces, not these concrete Mock classes that's the HAL pattern in action.*/
    MockUART uart;
    MockI2C i2c;

    
    // priority: 0 = highest. SensorReadTask runs most often/urgently,
    // LogTask is medium priority, BlinkTask is lowest (cosmetic).
    SensorReadTask sensorTask(1, 0, i2c, 0x48);
    LogTask logTask(2, 1, uart);
    BlinkTask blinkTask(3, 2, 13); // pin 13, like the classic Arduino LED

    //register tasks with the scheduler
    Scheduler scheduler;
    scheduler.addTask(&sensorTask);
    scheduler.addTask(&logTask);
    scheduler.addTask(&blinkTask);

    //run the simulation for a fixed number of ticks 
    const int NUM_TICKS = 9;
    scheduler.run(NUM_TICKS);

    std::cout << "\n Simulation complete after " << scheduler.getTickCount()
               << " ticks" << std::endl;

    return 0;
}