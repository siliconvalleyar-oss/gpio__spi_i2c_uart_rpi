#include "i2c_master.h"
#include <bcm2835.h>
#include <cstring>

namespace I2C_MASTER {

void I2cDeleter::operator()(I2c* p) const noexcept {
    destroy(p);
}

I2cHandle make(const Config& cfg) {
    auto i2c = std::make_unique<I2c>();
    i2c->cfg = cfg;

    bcm2835_i2c_begin();
    bcm2835_i2c_setClockDivider(cfg.clock_divider);
    bcm2835_i2c_setSlaveAddress(cfg.slave_addr);
    i2c->init_ok = true;
    return I2cHandle(i2c.release());
}

void destroy(I2c* i) {
    if (!i) return;
    bcm2835_i2c_end();
    delete i;
}

bool write(const I2c& i2c, const uint8_t* data, uint32_t len) {
    if (!i2c.init_ok || !data || len == 0) {
        return false;
    }
    return bcm2835_i2c_write(reinterpret_cast<const char*>(data), len) == BCM2835_I2C_REASON_OK;
}

bool read(const I2c& i2c, uint8_t* data, uint32_t len) {
    if (!i2c.init_ok || !data || len == 0) {
        return false;
    }
    return bcm2835_i2c_read(reinterpret_cast<char*>(data), len) == BCM2835_I2C_REASON_OK;
}

}
