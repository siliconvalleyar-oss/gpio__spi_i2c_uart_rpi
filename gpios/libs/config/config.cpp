#include "config.h"
#include <json/json.hpp>
#include <fstream>
#include <iostream>
#include <cstdlib>
#include <cerrno>
#include <unistd.h>
#include <sys/select.h>
#include <termios.h>

using json = nlohmann::json;

static struct termios cfg_orig;
static bool cfg_raw = false;

static void cfg_restore() {
    if (cfg_raw) {
        cfg_raw = false;
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &cfg_orig);
        std::cout << "\033[?25h" << std::flush;
    }
}

static void cfg_raw_mode() {
    tcgetattr(STDIN_FILENO, &cfg_orig);
    struct termios raw = cfg_orig;
    raw.c_lflag &= ~(ECHO | ICANON | ISIG);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    cfg_raw = true;
    std::cout << "\033[?25l" << std::flush;
}

static int cfg_read_key() {
    char c;
    while (true) {
        ssize_t n = read(STDIN_FILENO, &c, 1);
        if (n == 1) {
            if (c == '\033') {
                char seq[2];
                if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\033';
                if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\033';
                if (seq[0] == '[') {
                    switch (seq[1]) {
                        case 'A': return 1000;
                        case 'B': return 1001;
                    }
                }
                return '\033';
            }
            return static_cast<unsigned char>(c);
        }
        if (n < 0 && errno != EINTR) break;
    }
    return -1;
}

enum { CMD_UP = 1000, CMD_DOWN = 1001 };

static long prompt_value(const char* label, long current, long minv, long maxv) {
    std::cout << "\r\033[J" << label << " [" << current << "]: " << std::flush;
    char buf[32] = {0};
    size_t pos = 0;
    while (true) {
        int c = cfg_read_key();
        if (c == '\n' || c == '\r') {
            break;
        }
        if (c == 0x03) {
            std::cout << "\n";
            return current;
        }
        if (c == 127 || c == 8) {
            if (pos > 0) {
                --pos;
                std::cout << "\b \b" << std::flush;
            }
            continue;
        }
        if (c >= '0' && c <= '9' && pos < sizeof(buf) - 1) {
            buf[pos++] = static_cast<char>(c);
            std::cout << static_cast<char>(c) << std::flush;
        }
    }
    buf[pos] = '\0';
    if (pos == 0) return current;
    char* end = nullptr;
    long v = std::strtol(buf, &end, 10);
    if (end && *end == '\0' && v >= minv && v <= maxv) {
        std::cout << "\n";
        return v;
    }
    std::cout << " (invalid, keeping " << current << ")\n";
    return current;
}

AppConfig load_config(const char* path) {
    AppConfig cfg;
    std::ifstream f(path);
    if (!f.is_open()) return cfg;
    try {
        json j;
        f >> j;
        cfg.tick_ms           = j.value("tick_ms", cfg.tick_ms);
        cfg.spi_speed_hz      = j.value("spi_speed_hz", cfg.spi_speed_hz);
        cfg.i2c_clock_divider = j.value("i2c_clock_divider", cfg.i2c_clock_divider);
        cfg.uart_baud         = j.value("uart_baud", cfg.uart_baud);
        cfg.uart_device       = j.value("uart_device", cfg.uart_device);
        cfg.pwm_clock_divider = j.value("pwm_clock_divider", cfg.pwm_clock_divider);
        cfg.pwm_range         = j.value("pwm_range", cfg.pwm_range);
        cfg.onewire_pin       = j.value("onewire_pin", cfg.onewire_pin);
    } catch (...) {}
    return cfg;
}

void save_config(const char* path, const AppConfig& cfg) {
    json j;
    j["tick_ms"]           = cfg.tick_ms;
    j["spi_speed_hz"]      = cfg.spi_speed_hz;
    j["i2c_clock_divider"] = cfg.i2c_clock_divider;
    j["uart_baud"]         = cfg.uart_baud;
    j["uart_device"]       = cfg.uart_device;
    j["pwm_clock_divider"] = cfg.pwm_clock_divider;
    j["pwm_range"]         = cfg.pwm_range;
    j["onewire_pin"]       = cfg.onewire_pin;
    std::ofstream f(path);
    if (f.is_open()) f << j.dump(2) << "\n";
}

static const char* FIELD_NAMES[] = {
    "Tick speed (ms)",
    "SPI speed (Hz)",
    "I2C clock divider",
    "UART baud rate",
    "PWM clock divider",
    "PWM range",
    nullptr
};

enum FID {
    F_TICK   = 0,
    F_SPI    = 1,
    F_I2C    = 2,
    F_UART   = 3,
    F_PWM_DIV= 4,
    F_PWM_RNG= 5,
    F_COUNT  = 6
};

struct Field {
    const char* label;
    long* value;
    long minv;
    long maxv;
};

bool config_menu(AppConfig& cfg) {
    long vals[F_COUNT] = {
        static_cast<long>(cfg.tick_ms),
        static_cast<long>(cfg.spi_speed_hz),
        static_cast<long>(cfg.i2c_clock_divider),
        static_cast<long>(cfg.uart_baud),
        static_cast<long>(cfg.pwm_clock_divider),
        static_cast<long>(cfg.pwm_range)
    };

    Field fields[F_COUNT] = {
        {"Tick speed (ms)",       &vals[F_TICK],   1, 1000},
        {"SPI speed (Hz)",        &vals[F_SPI],    0, 25000000},
        {"I2C clock divider",     &vals[F_I2C],    2, 65535},
        {"UART baud rate",        &vals[F_UART],   300, 115200},
        {"PWM clock divider",    &vals[F_PWM_DIV], 2, 65535},
        {"PWM range",             &vals[F_PWM_RNG], 2, 65535},
    };

    int cur = 0;
    bool first = true;
    const int LINES = F_COUNT + 3;

    auto draw = [&]() {
        if (!first) std::cout << "\033[" << LINES << "A";
        first = false;
        std::cout << "\r\033[J";
        std::cout << "Configuration (arrows, Enter=edit, s=save, q=quit)\n\n";
        for (int i = 0; i < F_COUNT; ++i) {
            std::cout << (i == cur ? " \033[7m" : "  ")
                      << fields[i].label << ": " << vals[i]
                      << (i == cur ? "\033[0m" : "") << "\n";
        }
        std::cout << "\n  s=save & exit   q=discard\n" << std::flush;
    };

    cfg_raw_mode();
    draw();

    while (true) {
        int key = cfg_read_key();
        if (key == CMD_UP && cur > 0) { --cur; draw(); }
        else if (key == CMD_DOWN && cur < F_COUNT - 1) { ++cur; draw(); }
        else if (key == '\n' || key == '\r') {
            long v = prompt_value(fields[cur].label, vals[cur], fields[cur].minv, fields[cur].maxv);
            vals[cur] = v;
            draw();
        }
        else if (key == 's' || key == 'S') {
            cfg.tick_ms           = static_cast<int>(vals[F_TICK]);
            cfg.spi_speed_hz      = static_cast<int>(vals[F_SPI]);
            cfg.i2c_clock_divider = static_cast<int>(vals[F_I2C]);
            cfg.uart_baud         = static_cast<int>(vals[F_UART]);
            cfg.pwm_clock_divider = static_cast<int>(vals[F_PWM_DIV]);
            cfg.pwm_range         = static_cast<int>(vals[F_PWM_RNG]);
            cfg_restore();
            std::cout << "\r\033[JConfig saved.\n";
            return true;
        }
        else if (key == 'q' || key == 'Q') {
            cfg_restore();
            std::cout << "\r\033[JConfig discarded.\n";
            return false;
        }
        else if (key == 0x03) {
            cfg_restore();
            std::cout << "\r\033[J\n";
            return false;
        }
        else if (key == -1) {
            break;
        }
    }
    cfg_restore();
    return false;
}
