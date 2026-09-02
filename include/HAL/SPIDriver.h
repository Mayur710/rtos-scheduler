#ifndef SPI_DRIVER_H
#define SPI_DRIVER_H

#include <cstdint>
#include <vector>

/*SPI is full-duplex: every transfer simultaneously writes and reads data so unlike UART/I2C one function does both operations
real driver typically also needs a chip-select (CS) line here we pass it in as an explicit parameter to keep the 
interface hardware-agnostic */
class SPIDriver {
public:
    virtual ~SPIDriver() = default;
    virtual std::vector<uint8_t> transfer(uint8_t chipSelect,const std::vector<uint8_t>& out) = 0;
};

#endif 