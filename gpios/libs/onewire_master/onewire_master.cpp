#include "onewire_master.h"
#include <bcm2835.h>
#include <cstdint>
#include <iostream>

namespace ONEWIRE_MASTER {

void OneWireDeleter::operator()(OneWire*) const noexcept {
    destroy(nullptr);
}

OneWireHandle make(const Config& cfg) {
    auto ow = std::make_unique<OneWire>();
    ow->cfg = cfg;

    if (!bcm2835_init()) {
        std::cerr << "bcm2835 init failed for OneWire\n";
        return OneWireHandle(nullptr);
    }

    bcm2835_gpio_fsel(ow->cfg.pin, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_write(ow->cfg.pin, HIGH);
    ow->init_ok = true;
    return OneWireHandle(ow.release());
}

void destroy(OneWire*) {
    bcm2835_close();
}

bool reset(OneWire& ow) {
    if (!ow.init_ok) return false;

    bcm2835_gpio_write(ow.cfg.pin, LOW);
    delayMicroseconds(480);

    bcm2835_gpio_write(ow.cfg.pin, HIGH);
    bcm2835_gpio_fsel(ow.cfg.pin, BCM2835_GPIO_FSEL_INPT);
    delayMicroseconds(70);

    bool presence = !bcm2835_gpio_lev(ow.cfg.pin);

    delayMicroseconds(410);
    bcm2835_gpio_fsel(ow.cfg.pin, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_write(ow.cfg.pin, HIGH);
    return presence;
}

bool write_byte(OneWire& ow, uint8_t value) {
    if (!ow.init_ok) return false;

    for (int i = 0; i < 8; ++i) {
        bcm2835_gpio_write(ow.cfg.pin, LOW);
        delayMicroseconds(value & 1 ? 6 : 60);
        bcm2835_gpio_write(ow.cfg.pin, HIGH);
        delayMicroseconds(value & 1 ? 64 : 10);
        value >>= 1;
    }
    return true;
}

uint8_t read_byte(OneWire& ow) {
    if (!ow.init_ok) return 0;

    uint8_t value = 0;
    for (int i = 0; i < 8; ++i) {
        bcm2835_gpio_write(ow.cfg.pin, LOW);
        delayMicroseconds(6);
        bcm2835_gpio_write(ow.cfg.pin, HIGH);

        bcm2835_gpio_fsel(ow.cfg.pin, BCM2835_GPIO_FSEL_INPT);
        delayMicroseconds(9);
        if (bcm2835_gpio_lev(ow.cfg.pin)) {
            value |= (1u << i);
        }
        bcm2835_gpio_fsel(ow.cfg.pin, BCM2835_GPIO_FSEL_OUTP);
        delayMicroseconds(60);
    }
    return value;
}

}
