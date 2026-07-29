#pragma once

#include <cstdint>
#include <memory>
#include <string>

namespace UART_MASTER {

struct Config {
    std::string device;
    uint32_t baud = 115200;
    uint8_t bits = 8;
    char parity = 'N';
    uint8_t stop = 1;
};

struct Uart {
    Config cfg;
    int fd = -1;
    bool init_ok = false;
};

struct UartDeleter {
    void operator()(Uart*) const noexcept;
};

using UartHandle = std::unique_ptr<Uart, UartDeleter>;

UartHandle make(const Config& cfg);
void destroy(Uart* uart);
bool write(const Uart& uart, const uint8_t* data, uint32_t len);

}
