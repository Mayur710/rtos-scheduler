#ifndef MOCK_UART_H
#define MOCK_UART_H
#include <iostream>
#include "UARTDriver.h"

/*concrete UARTDriver implementation that simulates hardware by printing to console instead of touching real UART registers
this let us test task/scheduler logic without any hardware attached*/
class MockUART : public UARTDriver {
public:
    void write(const std::string& data) override {
        std::cout << "[UART] TX: \"" << data << "\"" << std::endl;
    }
    std::string read(size_t maxLen) override {
        // simulated incoming data
        std::string simulated = "ACK";
        if (simulated.size() > maxLen) {
            simulated = simulated.substr(0, maxLen);
        }
        std::cout << "[UART] RX: \"" << simulated << "\"" << std::endl;
        return simulated;
    }
};
#endif 