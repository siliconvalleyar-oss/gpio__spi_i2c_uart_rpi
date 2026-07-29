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
    if (!bcm2835_init()) {
        throw std::runtime_error("bcm2835 init failed in Port16::make");
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
    bcm2835_close();
    delete p;
}

void write_random(Port16& p) {
    if (!p.init_ok) return;

    static thread_local std::mt19937 rng(
        static_cast<unsigned>(std::chrono::high_resolution_clock::now().time_since_epoch().count())
    );
    std::uniform_int_distribution<uint16_t> dist(0, 0xFFFF);
    write(p, dist(rng));
}

void write(Port16& p, uint16_t value) {
    if (!p.init_ok) return;

    if (!p.cfg.as_input) {
        for (int i = 0; i < 16; ++i) {
            bcm2835_gpio_write(p.cfg.pins[i], (value & (1u << i)) ? HIGH : LOW);
        }
    }
}

uint16_t read(const Port16& p) {
    if (!p.init_ok) return 0;

    uint16_t value = 0;
    if (p.cfg.as_input) {
        for (int i = 0; i < 16; ++i) {
            if (bcm2835_gpio_lev(p.cfg.pins[i]) == HIGH) {
                value |= (1u << i);
            }
        }
    }
    return value;
}

}
