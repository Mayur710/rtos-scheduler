/*this is the hardware abstraction layer */
#ifndef UART_DRIVER_H
#define UART_DRIVER_H
#include <string>
#include <cstdint>

/*defines the contract any UART implementation must follow whether it talks to hardware or prints to console */
class UARTDriver {
public:
    virtual ~UARTDriver() = default;
    //sends a string of data over the UART interface
    virtual void write(const std::string& data) = 0;
    //receive upto maxLen bytes from UART and returns the received string 
    virtual std::string read(size_t maxLen) = 0;
};
#endif 