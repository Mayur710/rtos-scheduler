#ifndef I2C_DRIVER_H
#define I2C_DRIVER_H

#include <cstdint>
#include <vector>
#include <cstddef>
/*I2C is addressed and byte-oriented (unlike UART's plain byte stream) so the interface takes a 7-bit device address plus a 
byte buffer.
 */
class I2CDriver {
public:
    virtual ~I2CDriver() = default;

    // Write a buffer of bytes to the device at the given I2C address.
    virtual void write(uint8_t deviceAddr, const std::vector<uint8_t>& data) = 0;

    // Read numBytes from the device at the given I2C address.
    virtual std::vector<uint8_t> read(uint8_t deviceAddr, size_t numBytes) = 0;
};

#endif 