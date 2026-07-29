#pragma once

#include <cstdint>
#include <array>
#include <memory>

namespace PORT16 {

struct Config {
    std::array<uint16_t, 16> pins{};
    bool as_input = false;
};

struct Port16 {
    Config cfg;
    bool init_ok = false;
};

struct Port16Deleter {
    void operator()(Port16*) const noexcept;
};

using Port16Handle = std::unique_ptr<Port16, Port16Deleter>;

Port16Handle make(const Config& cfg);
void destroy(Port16* p);
void write_random(Port16& p);
void write(Port16& p, uint16_t value);
uint16_t read(const Port16& p);

}
