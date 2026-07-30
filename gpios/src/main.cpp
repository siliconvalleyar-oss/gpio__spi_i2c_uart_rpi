#include <memory>
#include <iostream>
#include <csignal>
#include <cstdint>
#include <cstdlib>
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
#include <set>
#include <vector>

#include <bcm2835.h>
#include <port16/port16.h>
#include <spi_master/spi_master.h>
#include <i2c_master/i2c_master.h>
#include <uart_master/uart_master.h>
#include <onewire_master/onewire_master.h>
#include <config/config.h>

static const char* CONFIG_PATH = "config.cfg";

static volatile std::sig_atomic_t running = 1;

static void onSignal(int) {
    running = 0;
}

#include <termios.h>

enum { KEY_NONE = -1, KEY_UP = 1000, KEY_DOWN, KEY_ENTER = '\n', KEY_SPACE = ' ',
       KEY_CTRLC = 0x03 };

static struct termios orig_tio;
static bool terminal_raw = false;

static void restore_terminal() {
    if (terminal_raw) {
        terminal_raw = false;
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_tio);
        std::cout << "\033[?25h" << std::flush;
    }
}

static void set_raw_mode() {
    tcgetattr(STDIN_FILENO, &orig_tio);
    struct termios raw = orig_tio;
    raw.c_lflag &= ~(ECHO | ICANON | ISIG);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    terminal_raw = true;
    std::cout << "\033[?25l" << std::flush;
}

static int read_key() {
    char c;
    while (running) {
        ssize_t n = read(STDIN_FILENO, &c, 1);
        if (n == 1) {
            if (c == '\033') {
                char seq[2];
                if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\033';
                if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\033';
                if (seq[0] == '[') {
                    switch (seq[1]) {
                        case 'A': return KEY_UP;
                        case 'B': return KEY_DOWN;
                    }
                }
                return '\033';
            }
            return static_cast<unsigned char>(c);
        }
        if (n < 0 && errno != EINTR) break;
    }
    return KEY_NONE;
}

static bool is_interactive() {
    return isatty(STDIN_FILENO) == 1;
}

static bool timed_read(char* buf, size_t bufsz, size_t& out_len, int timeout_ms) {
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(STDIN_FILENO, &rfds);
    timeval tv{};
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    int ret = select(STDIN_FILENO + 1, &rfds, nullptr, nullptr, &tv);
    if (ret <= 0) return false;
    if (!FD_ISSET(STDIN_FILENO, &rfds)) return false;

    out_len = 0;
    while (out_len < bufsz - 1 && running) {
        char c;
        ssize_t r = read(STDIN_FILENO, &c, 1);
        if (r != 1) break;
        if (c == '\n' || c == '\r') break;
        buf[out_len++] = c;
    }
    buf[out_len] = '\0';
    return out_len > 0;
}

static std::set<int> ask_protocols() {
    std::set<int> selected;
    if (!is_interactive()) {
        selected.insert({1, 2, 3, 4, 5});
        return selected;
    }

    const char* names[] = {"SPI", "I2C", "UART", "1-Wire", "PWM"};
    const int N = 5;
    int cur = 0;
    bool first_draw = true;

    auto draw = [&]() {
        if (!first_draw) {
            std::cout << "\033[" << (N + 1) << "A";
        }
        first_draw = false;
        std::cout << "\r\033[J";
        std::cout << "Arrow keys to move, Space=toggle, Enter=confirm\n";
        for (int i = 0; i < N; ++i) {
            std::cout << (i == cur ? " \033[7m" : "  ")
                      << (selected.count(i + 1) ? "[X]" : "[ ]")
                      << " " << names[i]
                      << (i == cur ? "\033[0m" : "")
                      << "\n";
        }
        std::cout << std::flush;
    };

    set_raw_mode();
    draw();

    while (running) {
        int key = read_key();
        if (key == KEY_UP && cur > 0) { --cur; draw(); }
        else if (key == KEY_DOWN && cur < N - 1) { ++cur; draw(); }
        else if (key == KEY_SPACE) {
            int idx = cur + 1;
            if (selected.count(idx)) selected.erase(idx);
            else selected.insert(idx);
            draw();
        } else if (key == KEY_ENTER || key == '\r') {
            break;
        } else if (key == KEY_CTRLC) {
            selected.clear();
            break;
        } else if (key == KEY_NONE) {
            break;
        }
    }

    std::cout << "\033[" << (N + 1) << "B";
    restore_terminal();
    return selected;
}

static int ask_duration() {
    if (!is_interactive()) return -1;

    std::cout << "Duration in seconds (default 10): " << std::flush;
    char buf[32] = {0};
    size_t len = 0;
    if (!timed_read(buf, sizeof(buf), len, 10000)) {
        return -1;
    }

    char* end = nullptr;
    long v = std::strtol(buf, &end, 10);
    if (end && *end == '\0' && v > 0) {
        return static_cast<int>(v);
    }
    return -1;
}

static void set_low_priority() {
    setpriority(PRIO_PROCESS, 0, 10);
    sched_yield();
}

static const std::vector<int>& pins_for_protocol(int proto) {
    static const std::vector<int> spi_pins   = {7, 8, 9, 10, 11};
    static const std::vector<int> i2c_pins   = {2, 3};
    static const std::vector<int> uart_pins  = {14, 15};
    static const std::vector<int> onewire_pins = {4};
    static const std::vector<int> pwm_pins   = {12, 13};
    static const std::vector<int> empty;
    switch (proto) {
        case 1: return spi_pins;
        case 2: return i2c_pins;
        case 3: return uart_pins;
        case 4: return onewire_pins;
        case 5: return pwm_pins;
        default: return empty;
    }
}

static std::vector<uint16_t> available_gpio_pins(const std::set<int>& active_protos) {
    std::set<uint16_t> reserved;
    for (int p : active_protos) {
        for (int pin : pins_for_protocol(p)) {
            reserved.insert(static_cast<uint16_t>(pin));
        }
    }
    static const std::vector<int> i2s = {18, 19, 20, 21};
    for (int pin : i2s) reserved.insert(static_cast<uint16_t>(pin));

    std::vector<uint16_t> avail;
    for (uint16_t gpio = 0; gpio <= 27; ++gpio) {
        if (reserved.find(gpio) == reserved.end()) {
            avail.push_back(gpio);
        }
    }
    return avail;
}

static int main_menu(AppConfig& cfg) {
    const char* items[] = {"Select protocols and run", "Configure ports", "Quit"};
    const int N = 3;
    int cur = 0;
    bool first = true;
    int result = -1;

    auto draw = [&]() {
        if (!first) std::cout << "\033[" << (N + 2) << "A";
        first = false;
        std::cout << "\r\033[J";
        std::cout << "GPIO Generator — Main Menu\n\n";
        for (int i = 0; i < N; ++i) {
            std::cout << (i == cur ? " \033[7m" : "  ")
                      << items[i]
                      << (i == cur ? "\033[0m" : "") << "\n";
        }
        std::cout << std::flush;
    };

    set_raw_mode();
    draw();

    while (result < 0 && running) {
        int key = read_key();
        if (key == KEY_UP && cur > 0) --cur;
        else if (key == KEY_DOWN && cur < N - 1) ++cur;
        else if (key == KEY_ENTER || key == '\r') result = cur;
        else if (key == KEY_CTRLC) result = 2;
        else if (key == 'q' || key == 'Q') result = 2;
        else continue;
        if (result < 0) draw();
    }

    std::cout << "\033[" << (N + 2) << "B";
    restore_terminal();
    return result;
}

int main(int argc, char** argv) {
    int duration_s = 10;
    if (argc > 1) {
        char* end = nullptr;
        long v = std::strtol(argv[1], &end, 10);
        if (end && *end == '\0' && v > 0) {
            duration_s = static_cast<int>(v);
        }
    }

    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);
    std::signal(SIGHUP, onSignal);

    AppConfig cfg = load_config(CONFIG_PATH);

    if (!bcm2835_init()) {
        std::cerr << "ERROR: bcm2835_init() failed (run as root?)\n";
        return 1;
    }

    set_low_priority();

    std::set<int> protocols;

    if (argc > 2 && strcmp(argv[2], "--all") == 0) {
        protocols.insert({1, 2, 3, 4, 5});
    } else if (argc > 2 && strcmp(argv[2], "--quick") == 0) {
        // skip menus
    } else if (is_interactive()) {
        while (running) {
            int choice = main_menu(cfg);
            if (choice == 0) {
                protocols = ask_protocols();
                if (protocols.empty()) continue;
                break;
            } else if (choice == 1) {
                bool changed = config_menu(cfg);
                if (changed) save_config(CONFIG_PATH, cfg);
                continue;
            } else {
                bcm2835_close();
                return 0;
            }
        }
    }

    if (!running) { bcm2835_close(); return 0; }

    int dur = ask_duration();
    if (dur > 0) duration_s = dur;
    if (!running) { bcm2835_close(); return 0; }

    bool use_spi    = protocols.count(1);
    bool use_i2c    = protocols.count(2);
    bool use_uart   = protocols.count(3);
    bool use_onewire = protocols.count(4);
    bool use_pwm    = protocols.count(5);

    std::vector<uint16_t> gpio_pins = available_gpio_pins(protocols);
    if (gpio_pins.size() > 16) gpio_pins.resize(16);
    if (gpio_pins.empty()) {
        std::cerr << "ERROR: no GPIO pins available\n";
        bcm2835_close();
        return 1;
    }

    PORT16::Config p16cfg{};
    p16cfg.pins = gpio_pins;
    p16cfg.as_input = false;
    auto port16 = PORT16::make(p16cfg);

    std::unique_ptr<SPI_MASTER::Spi, SPI_MASTER::SpiDeleter> spi;
    if (use_spi) {
        SPI_MASTER::Config spicfg{};
        spicfg.speed_hz = cfg.spi_speed_hz;
        spi = SPI_MASTER::make(spicfg);
    }

    std::unique_ptr<I2C_MASTER::I2c, I2C_MASTER::I2cDeleter> i2c;
    if (use_i2c) {
        I2C_MASTER::Config i2ccfg{};
        i2ccfg.slave_addr = 0x20;
        i2ccfg.clock_divider = cfg.i2c_clock_divider;
        i2c = I2C_MASTER::make(i2ccfg);
    }

    std::unique_ptr<UART_MASTER::Uart, UART_MASTER::UartDeleter> uart;
    if (use_uart) {
        UART_MASTER::Config ucfg{};
        ucfg.device = cfg.uart_device;
        ucfg.baud = cfg.uart_baud;
        uart = UART_MASTER::make(ucfg);
    }

    std::unique_ptr<ONEWIRE_MASTER::OneWire, ONEWIRE_MASTER::OneWireDeleter> ow;
    if (use_onewire) {
        ONEWIRE_MASTER::Config owcfg{};
        owcfg.pin = cfg.onewire_pin;
        ow = ONEWIRE_MASTER::make(owcfg);
    }

    if (use_pwm) {
        for (int pin : {12, 13}) {
            bcm2835_gpio_fsel(pin, BCM2835_GPIO_FSEL_ALT0);
        }
        bcm2835_pwm_set_clock(cfg.pwm_clock_divider);
        bcm2835_pwm_set_mode(0, 1, 1);
        bcm2835_pwm_set_range(0, cfg.pwm_range);
        bcm2835_pwm_set_mode(1, 1, 1);
        bcm2835_pwm_set_range(1, cfg.pwm_range);
    }

    std::time_t now_c = std::time(nullptr);
    std::cout << "\n=== GPIO Generator ===\n";
    std::cout << "Start:  " << std::put_time(std::localtime(&now_c), "%Y-%m-%d %H:%M:%S") << "\n";
    std::cout << "GPIO pins: " << gpio_pins.size() << " (";
    for (size_t i = 0; i < gpio_pins.size(); ++i) {
        if (i) std::cout << ",";
        std::cout << static_cast<int>(gpio_pins[i]);
    }
    std::cout << ")\n";
    if (use_spi)    std::cout << "SPI enabled  (" << cfg.spi_speed_hz << " Hz)\n";
    if (use_i2c)    std::cout << "I2C enabled  (divider " << cfg.i2c_clock_divider << ")\n";
    if (use_uart)   std::cout << "UART enabled (" << cfg.uart_baud << " baud)\n";
    if (use_onewire) std::cout << "1-Wire enabled\n";
    if (use_pwm)    std::cout << "PWM enabled  (divider " << cfg.pwm_clock_divider
                              << ", range " << cfg.pwm_range << ")\n";
    std::cout << "Tick: " << cfg.tick_ms << "ms\n";
    std::cout << "Duration: " << duration_s << "s\n";
    std::cout << "Press Ctrl+C to stop.\n";

    int total_ticks = 0;
    std::mt19937 rng(static_cast<unsigned>(
        std::chrono::high_resolution_clock::now().time_since_epoch().count()));
    std::uniform_int_distribution<uint32_t> dist_val(0, 0xFFFF);

    int phase = 0;
    auto start_time = std::chrono::steady_clock::now();
    auto end_time = start_time + std::chrono::seconds(duration_s);

    while (running && std::chrono::steady_clock::now() < end_time) {
        PORT16::write_random(*port16);

        if (use_pwm) {
            bcm2835_pwm_set_data(0, dist_val(rng) % cfg.pwm_range);
            bcm2835_pwm_set_data(1, dist_val(rng) % cfg.pwm_range);
        }

        if (spi && phase == 0) {
            uint8_t tx = static_cast<uint8_t>(dist_val(rng));
            uint8_t rx = 0;
            SPI_MASTER::transfer(*spi, &tx, &rx, 1);
        } else if (i2c && phase == 1) {
            uint8_t data = static_cast<uint8_t>(dist_val(rng));
            I2C_MASTER::write(*i2c, &data, 1);
        } else if (uart && phase == 2) {
            std::string msg = "T:" + std::to_string(total_ticks) + "\n";
            UART_MASTER::write(*uart,
                               reinterpret_cast<const uint8_t*>(msg.data()),
                               msg.size());
        } else if (ow && phase == 3) {
            if (ONEWIRE_MASTER::reset(*ow)) {
                uint8_t cmd = static_cast<uint8_t>(dist_val(rng));
                ONEWIRE_MASTER::write_byte(*ow, cmd);
            }
        }

        ++total_ticks;
        phase = (phase + 1) % 4;

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time).count();
        auto target = static_cast<long long>(total_ticks) * cfg.tick_ms;
        if (target > elapsed) {
            timespec ts{};
            long diff = static_cast<long>(target - elapsed);
            ts.tv_sec = diff / 1000;
            ts.tv_nsec = (diff % 1000) * 1000000L;
            nanosleep(&ts, nullptr);
        }
    }

    if (use_pwm) {
        bcm2835_pwm_set_data(0, 0);
        bcm2835_pwm_set_data(1, 0);
        bcm2835_pwm_set_mode(0, 1, 0);
        bcm2835_pwm_set_mode(1, 1, 0);
        for (int pin : {12, 13}) {
            bcm2835_gpio_fsel(pin, BCM2835_GPIO_FSEL_INPT);
        }
    }

    port16.reset();
    spi.reset();
    i2c.reset();
    uart.reset();
    ow.reset();
    bcm2835_close();

    std::time_t end_c = std::time(nullptr);
    std::cout << "Done. Time: "
              << std::put_time(std::localtime(&end_c), "%Y-%m-%d %H:%M:%S")
              << "  Total ticks: " << total_ticks << "\n";
    return 0;
}
