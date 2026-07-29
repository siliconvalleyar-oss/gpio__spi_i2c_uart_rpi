#include "uart_master.h"
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <cstring>

namespace UART_MASTER {

void UartDeleter::operator()(Uart* u) const noexcept {
    destroy(u);
}

UartHandle make(const Config& cfg) {
    auto u = std::make_unique<Uart>();
    u->cfg = cfg;

    u->fd = open(cfg.device.c_str(), O_RDWR | O_NOCTTY | O_SYNC);
    if (u->fd < 0) {
        return UartHandle(nullptr);
    }

    struct termios tio{};
    if (tcgetattr(u->fd, &tio) != 0) {
        close(u->fd);
        return UartHandle(nullptr);
    }

    speed_t speed = static_cast<speed_t>(cfg.baud);
    cfsetispeed(&tio, speed);
    cfsetospeed(&tio, speed);

    tio.c_cflag |= (CLOCAL | CREAD);
    tio.c_cflag &= ~PARENB;
    tio.c_cflag &= ~CSTOPB;
    tio.c_cflag &= ~CSIZE;
    tio.c_cflag |= CS8;

    tio.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tio.c_oflag &= ~OPOST;

    tio.c_cc[VMIN] = 0;
    tio.c_cc[VTIME] = 1;

    tcflush(u->fd, TCIFLUSH);
    if (tcsetattr(u->fd, TCSANOW, &tio) != 0) {
        close(u->fd);
        return UartHandle(nullptr);
    }

    u->init_ok = true;
    return UartHandle(u.release());
}

void destroy(Uart* u) {
    if (!u) return;
    if (u->fd >= 0) {
        close(u->fd);
    }
    delete u;
}

bool write(const Uart& u, const uint8_t* data, uint32_t len) {
    if (u.fd < 0 || !data || len == 0) {
        return false;
    }
    ssize_t n = ::write(u.fd, data, len);
    tcdrain(u.fd);
    return n == static_cast<ssize_t>(len);
}

}
