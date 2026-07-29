#include "spi_master.h"
#include <bcm2835.h>

namespace SPI_MASTER {

void SpiDeleter::operator()(Spi* s) const noexcept {
    destroy(s);
}

SpiHandle make(const Config& cfg) {
    auto s = std::make_unique<Spi>();
    s->cfg = cfg;

    bcm2835_spi_begin();
    bcm2835_spi_setBitOrder(BCM2835_SPI_BIT_ORDER_MSBFIRST);
    bcm2835_spi_setDataMode(BCM2835_SPI_MODE0);
    s->init_ok = true;
    return SpiHandle(s.release());
}

void destroy(Spi*) {
    bcm2835_spi_end();
}

int transfer(Spi& s, const uint8_t* tx, uint8_t* rx, uint32_t len) {
    if (!s.init_ok || !tx || !rx || len == 0) {
        return -1;
    }

    for (uint32_t i = 0; i < len; ++i) {
        rx[i] = static_cast<uint8_t>(bcm2835_spi_transfer(static_cast<unsigned char>(tx[i])));
    }
    return static_cast<int>(len);
}

}
