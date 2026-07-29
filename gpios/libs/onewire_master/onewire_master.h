#pragma once

#include <cstdint>
#include <memory>

namespace ONEWIRE_MASTER {

struct Config {
    uint16_t pin = 4;
};

struct OneWire {
    Config cfg;
    bool init_ok = false;
};

struct OneWireDeleter {
    void operator()(OneWire*) const noexcept;
};

using OneWireHandle = std::unique_ptr<OneWire, OneWireDeleter>;

OneWireHandle make(const Config& cfg);
void destroy(OneWire* ow);
bool reset(OneWire& ow);
bool write_byte(OneWire& ow, uint8_t value);
uint8_t read_byte(OneWire& ow);

}
