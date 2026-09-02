#ifndef MOCK_I2C_H
#define MOCK_I2C_H
#include <iostream>
#include <iomanip>
#include "I2CDriver.h"

/*simulates an I2C transaction by printing the address and byte payload in hex, the way a logic analyzer trace or datasheet 
example would show it*/
class MockI2C : public I2CDriver {
public:
    //overriding the write function in parent class 
    void write(uint8_t deviceAddr, const std::vector<uint8_t>& data) override {
        std::cout << "[I2C] Write to addr 0x" << std::hex << (int)deviceAddr
                   << " -> [";
        printBytes(data);
        std::cout << "]" << std::dec << std::endl;
    }
    //we don't have any hardware to read from so invent fake data to return 
    std::vector<uint8_t> read(uint8_t deviceAddr, size_t numBytes) override {
        std::vector<uint8_t> data;
        for (size_t i = 0; i < numBytes; ++i) {
            data.push_back(static_cast<uint8_t>(0x1A + i));
        }
        std::cout << "[I2C] Read " << numBytes << " bytes from addr 0x"
                   << std::hex << (int)deviceAddr << " -> [";
        printBytes(data);
        std::cout << "]" << std::dec << std::endl;
        return data;
    }
private:
    void printBytes(const std::vector<uint8_t>& data) {
        for (size_t i = 0; i < data.size(); ++i) {
            std::cout << "0x" << std::hex << std::setw(2) << std::setfill('0')
                       << (int)data[i];
            if (i != data.size() - 1) std::cout << ", ";
        }
    }
};
#endif 