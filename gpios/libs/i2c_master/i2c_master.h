#pragma once

#include <cstdint>
#include <memory>
#include <vector>

namespace I2C_MASTER {

struct Config {
    uint8_t slave_addr = 0x20;
    uint32_t speed_hz = 2500;
};

struct I2c {
    Config cfg;
    bool init_ok = false;
};

struct I2cDeleter {
    void operator()(I2c*) const noexcept;
};

using I2cHandle = std::unique_ptr<I2c, I2cDeleter>;

I2cHandle make(const Config& cfg);
void destroy(I2c* i2c);
bool write(const I2c& i2c, const uint8_t* data, uint32_t len);
bool read(const I2c& i2c, uint8_t* data, uint32_t len);

}
