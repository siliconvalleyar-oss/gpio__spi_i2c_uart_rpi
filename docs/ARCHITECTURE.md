# Architecture

## File Structure

```
gpio_generator_16bits/
├── gpios/
│   ├── src/
│   │   └── main.cpp             ← entry point, menu, loop, cleanup
│   ├── libs/
│   │   ├── port16/              ← 16-bit GPIO port abstraction
│   │   │   ├── port16.h
│   │   │   └── port16.cpp
│   │   ├── spi_master/          ← SPI master via bcm2835
│   │   │   ├── spi_master.h
│   │   │   └── spi_master.cpp
│   │   ├── i2c_master/          ← I2C master via bcm2835
│   │   │   ├── i2c_master.h
│   │   │   └── i2c_master.cpp
│   │   ├── uart_master/         ← UART via /dev/serial0
│   │   │   ├── uart_master.h
│   │   │   └── uart_master.cpp
│   │   └── onewire_master/      ← 1-Wire bit-bang via bcm2835
│   │       ├── onewire_master.h
│   │       └── onewire_master.cpp
│   └── Makefile
├── docs/
│   ├── ARCHITECTURE.md
│   ├── BUILD.md
│   ├── EXAMPLES.md
│   └── PINOUT.md
├── script_tools/
│   ├── install_deps.sh          ← automatic dependency installer
│   ├── rsync.sh                 ← transfer to Pi
│   └── .sshpass.env             ← SSH password (gitignored)
├── SKILL.md
├── Makefile                     ← top-level (delegates to gpios/)
├── VERSION
└── LICENSE
```

## Main Loop

```
main()
├── signal(SIGINT/SIGTERM/SIGHUP) → onSignal()
├── bcm2835_init()
├── ask_protocols()               ← menú interactivo
├── ask_duration()
├── PORT16::make(pines_libres)
├── SPI/I2C/UART/1W/PWM::make()  ← solo si habilitados
│
├── loop 100Hz (10ms tick)
│   ├── PORT16::write_random()    ← siempre
│   ├── PWM set_data()            ← siempre (si activo)
│   └── peripheral rotation       ← 1 de 4 por tick
│
└── cleanup
    ├── PWM apagado y pines INPUT
    ├── PORT16 destroy (pines INPUT)
    ├── SPI/I2C/UART/1W destroy
    └── bcm2835_close()
```

## Signal Handling

- `SIGINT` (Ctrl+C) → `running = 0` → loop termina
- `SIGTERM` → ídem
- `SIGHUP` (SSH drop) → ídem
- Variable `volatile std::sig_atomic_t` garantiza visibilidad en el handler

## Timing

- `std::chrono::steady_clock` para el timeout (10s default)
- Acumulador de ticks: `target = tick * 10ms`, duerme la diferencia
- No depende del reloj de pared (NTP-safe)

## Cleanup

Cada módulo restaura sus pines a estado seguro:
- **Port16**: todos los pines → `FSEL_INPT`
- **OneWire**: pin → `FSEL_INPT` + `PUD_UP`
- **PWM**: `set_data(0)` → `set_mode(enabled=0)` → pines → `FSEL_INPT`
- **SPI**: `bcm2835_spi_end()`
- **I2C**: `bcm2835_i2c_end()`
- **UART**: `close(fd)`
- **Global**: `bcm2835_close()` (una sola vez al final)
