#ifndef MOCK_SPI_H
#define MOCK_SPI_H
#include <iostream>
#include <iomanip>
#include "SPIDriver.h"

/*simulates a full-duplex SPI transfer since SPI writes and reads,simultaneously, this just echoes back a simple 
transformation of the outgoing bytes as if a peripheral responded*/
class MockSPI : public SPIDriver {
public:
    //a single trasfer function because both read and write happen at the same time in SPI
    std::vector<uint8_t> transfer(uint8_t chipSelect,
                                   const std::vector<uint8_t>& out) override {
        std::cout << "[SPI] CS" << (int)chipSelect << " TX -> [";
        for (size_t i = 0; i < out.size(); ++i) {
            std::cout << "0x" << std::hex << std::setw(2) << std::setfill('0')
                       << (int)out[i];
            if (i != out.size() - 1) std::cout << ", ";
        }
        std::cout << "]" << std::dec << std::endl;
        std::vector<uint8_t> in;
        for (uint8_t b : out) {
            in.push_back(static_cast<uint8_t>(b + 1));
        }
        return in;
    }
};
#endif 