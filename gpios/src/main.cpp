#include <memory>
#include <iostream>
#include <csignal>
#include <cstdint>
#include <cstring>
#include <chrono>
#include <random>
#include <unistd.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <iomanip>
#include <ctime>
#include <sched.h>
#include <cerrno>

#include <bcm2835.h>
#include <port16/port16.h>
#include <spi_master/spi_master.h>
#include <i2c_master/i2c_master.h>
#include <uart_master/uart_master.h>
#include <onewire_master/onewire_master.h>

static volatile bool running = true;

static void onSignal(int) {
    running = false;
}

static bool is_interactive() {
    return isatty(STDIN_FILENO) == 1;
}

static bool ask_continue() {
    if (!is_interactive()) {
        return false;
    }

    std::cout << "Desea continuar? (s/n): " << std::flush;
    fflush(stdout);

    for (int i = 0; i < 50 && running; ++i) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(STDIN_FILENO, &rfds);
        timeval tv{};
        tv.tv_sec = 0;
        tv.tv_usec = 200000;

        int ret = select(STDIN_FILENO + 1, &rfds, nullptr, nullptr, &tv);
        if (ret <= 0) {
            continue;
        }

        if (FD_ISSET(STDIN_FILENO, &rfds)) {
            char buf[2] = {0};
            if (read(STDIN_FILENO, buf, 1) != 1) {
                std::cout << "\nFinalizando.\n";
                return false;
            }
            return buf[0] == 's' || buf[0] == 'S';
        }
    }

    std::cout << "\nSin respuesta. Finalizando.\n";
    return false;
}

static int ask_duration() {
    if (!is_interactive()) {
        return -1;
    }

    std::cout << "Cuanto tiempo desea continuar (segundos)? " << std::flush;
    fflush(stdout);

    for (int i = 0; i < 50 && running; ++i) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(STDIN_FILENO, &rfds);
        timeval tv{};
        tv.tv_sec = 0;
        tv.tv_usec = 200000;

        int ret = select(STDIN_FILENO + 1, &rfds, nullptr, nullptr, &tv);
        if (ret <= 0) {
            continue;
        }

        if (FD_ISSET(STDIN_FILENO, &rfds)) {
            char buf[32] = {0};
            int n = 0;
            while (n < 31 && running) {
                ssize_t r = read(STDIN_FILENO, buf + n, 1);
                if (r != 1) {
                    break;
                }
                if (buf[n] == '\n' || buf[n] == '\r') {
                    break;
                }
                ++n;
            }
            buf[n] = '\0';

            char* end = nullptr;
            long v = std::strtol(buf, &end, 10);
            if (end && *end == '\0' && v > 0) {
                return static_cast<int>(v);
            }

            std::cout << "Valor invalido. Finalizando.\n";
            return -1;
        }
    }

    std::cout << "\nSin respuesta. Finalizando.\n";
    return -1;
}

static void set_low_priority() {
    if (setpriority(PRIO_PROCESS, 0, 10) == 0) {
    }
    if (sched_yield() == 0) {
    }
}

int main(int argc, char** argv) {
    int duration_s = 30;
    if (argc > 1) {
        char* end = nullptr;
        long v = std::strtol(argv[1], &end, 10);
        if (end && *end == '\0' && v > 0) {
            duration_s = static_cast<int>(v);
        }
    }

    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);

    std::time_t now_c = std::time(nullptr);
    std::cout << "=== GPIO 16-bit Generator ===\n";
    std::cout << "Inicio: " << std::put_time(std::localtime(&now_c), "%Y-%m-%d %H:%M:%S") << "\n";
    std::cout << "Duracion configurada: " << duration_s << "s\n";
    std::cout << "Presione Ctrl+C para detener.\n";

    if (!bcm2835_init()) {
        std::cerr << "ERROR: bcm2835_init() failed\n";
        return 1;
    }

    set_low_priority();

    PORT16::Config p16cfg{};
    for (int i = 0; i < 16; ++i) {
        p16cfg.pins[i] = static_cast<uint16_t>(20 + i);
    }
    p16cfg.as_input = false;
    auto port16 = PORT16::make(p16cfg);

    SPI_MASTER::Config spicfg{};
    spicfg.speed_hz = 500000;
    auto spi = SPI_MASTER::make(spicfg);

    I2C_MASTER::Config i2ccfg{};
    i2ccfg.slave_addr = 0x20;
    i2ccfg.speed_hz = 2500;
    auto i2c = I2C_MASTER::make(i2ccfg);

    UART_MASTER::Config ucfg{};
    ucfg.device = "/dev/serial0";
    ucfg.baud = 115200;
    auto uart = UART_MASTER::make(ucfg);

    ONEWIRE_MASTER::Config owcfg{};
    owcfg.pin = 4;
    auto ow = ONEWIRE_MASTER::make(owcfg);

    int total_ticks = 0;

    while (running) {
        std::time_t start = std::time(nullptr);
        std::time_t end = start + duration_s;
        int tick = 0;
        std::mt19937 rng(static_cast<unsigned>(
            std::chrono::high_resolution_clock::now().time_since_epoch().count()));
        std::uniform_int_distribution<int> dist8(0, 255);

        while (running && std::time(nullptr) < end) {
            PORT16::write_random(*port16);

            int phase = tick % 4;
            if (phase == 0) {
                uint8_t tx = static_cast<uint8_t>(dist8(rng));
                uint8_t rx = 0;
                SPI_MASTER::transfer(*spi, &tx, &rx, 1);
            } else if (phase == 1) {
                uint8_t data = static_cast<uint8_t>(dist8(rng));
                I2C_MASTER::write(*i2c, &data, 1);
            } else if (phase == 2) {
                std::string msg = "T:" + std::to_string(tick) + "\n";
                UART_MASTER::write(*uart,
                                    reinterpret_cast<const uint8_t*>(msg.data()),
                                    msg.size());
            } else {
                if (ONEWIRE_MASTER::reset(*ow)) {
                    uint8_t cmd = static_cast<uint8_t>(dist8(rng));
                    ONEWIRE_MASTER::write_byte(*ow, cmd);
                }
            }

            ++tick;
            ++total_ticks;

            timespec ts{};
            ts.tv_sec = 0;
            ts.tv_nsec = 10 * 1000 * 1000;
            while (nanosleep(&ts, &ts) == -1 && errno == EINTR && running) {
            }
        }

        if (!running) {
            break;
        }

        std::cout << "Ciclo finalizado. ticks=" << tick
                  << " total_ticks=" << total_ticks << "\n";

        if (!ask_continue()) {
            break;
        }

        int next = ask_duration();
        if (next < 0) {
            break;
        }
        duration_s = next;
    }

    port16.reset();
    spi.reset();
    i2c.reset();
    uart.reset();
    ow.reset();
    bcm2835_close();

    std::cout << "Done. Total ticks: " << total_ticks << "\n";
    return 0;
}
