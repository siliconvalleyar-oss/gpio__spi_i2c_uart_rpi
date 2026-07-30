#pragma once

#include <cstdint>
#include <string>

struct AppConfig {
    int tick_ms = 10;
    int spi_speed_hz = 500000;
    int i2c_clock_divider = 2500;
    int uart_baud = 9600;
    std::string uart_device = "/dev/serial0";
    int pwm_clock_divider = 192;
    int pwm_range = 256;
    int onewire_pin = 4;
};

AppConfig load_config(const char* path);
void save_config(const char* path, const AppConfig& cfg);
bool config_menu(AppConfig& cfg);
