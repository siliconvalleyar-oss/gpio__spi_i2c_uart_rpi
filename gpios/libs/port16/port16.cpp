#include "port16.h"
#include <bcm2835.h>
#include <stdexcept>
#include <random>
#include <chrono>

namespace PORT16 {

void Port16Deleter::operator()(Port16* p) const noexcept {
    destroy(p);
}

Port16Handle make(const Config& cfg) {
    if (cfg.pins.empty()) {
        throw std::runtime_error("Port16: no pins configured");
    }

    auto p = std::make_unique<Port16>();
    p->cfg = cfg;
    for (uint16_t pin : p->cfg.pins) {
        bcm2835_gpio_fsel(pin, BCM2835_GPIO_FSEL_OUTP);
        bcm2835_gpio_write(pin, LOW);
    }
    p->init_ok = true;
    return Port16Handle(p.release());
}

void destroy(Port16* p) {
    if (!p) return;
    for (uint16_t pin : p->cfg.pins) {
        bcm2835_gpio_fsel(pin, BCM2835_GPIO_FSEL_INPT);
    }
    delete p;
}

void write_random(Port16& p) {
    if (!p.init_ok || p.cfg.as_input) return;

    std::uniform_int_distribution<uint32_t> dist(0, (1u << p.cfg.pins.size()) - 1);
    std::mt19937 rng(static_cast<unsigned>(
        std::chrono::high_resolution_clock::now().time_since_epoch().count()));
    write(p, dist(rng));
}

void write(Port16& p, uint32_t value) {
    if (!p.init_ok || p.cfg.as_input) return;

    for (size_t i = 0; i < p.cfg.pins.size(); ++i) {
        bcm2835_gpio_write(p.cfg.pins[i], (value & (1u << i)) ? HIGH : LOW);
    }
}

uint32_t read(const Port16& p) {
    if (!p.init_ok || !p.cfg.as_input) return 0;

    uint32_t value = 0;
    for (size_t i = 0; i < p.cfg.pins.size(); ++i) {
        if (bcm2835_gpio_lev(p.cfg.pins[i]) == HIGH) {
            value |= (1u << i);
        }
    }
    return value;
}

}
