#pragma once

#include <cstdint>
#include <memory>

namespace SPI_MASTER {

struct Config {
    uint32_t speed_hz = 500000;
    uint8_t bits = 8;
};

struct Spi {
    Config cfg;
    bool init_ok = false;
};

struct SpiDeleter {
    void operator()(Spi*) const noexcept;
};

using SpiHandle = std::unique_ptr<Spi, SpiDeleter>;

SpiHandle make(const Config& cfg);
void destroy(Spi* s);
int transfer(Spi& s, const uint8_t* tx, uint8_t* rx, uint32_t len);

}
