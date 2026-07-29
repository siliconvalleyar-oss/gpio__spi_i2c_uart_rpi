# GPIO 16-bit Signal Generator — SKILL

## Descripción

Generador de señales para Raspberry Pi que maneja pines GPIO, SPI, I2C, UART, 1-Wire y PWM.  
Cada tick (~10ms) escribe valores aleatorios en los GPIOs libres y rota por los protocolos habilitados.

---

## Bugs críticos corregidos

| Bug | Archivo | Fix |
|-----|---------|-----|
| Deleters I2C/OneWire pasaban `nullptr` a `destroy()` | `i2c_master.cpp:8`, `onewire_master.cpp:9` | Pasan el puntero real |
| `bcm2835_close()` llamado desde 3 lugares distintos | `port16.cpp`, `onewire_master.cpp`, `main.cpp` | Solo `main.cpp` llama `bcm2835_init/close` |
| `bcm2835_init()` llamado 3 veces al inicio | `port16.cpp:14`, `onewire_master.cpp:16` | Solo `main.cpp` lo llama |
| `volatile bool running` → no atómico para signal handler | `main.cpp:27` | Cambiado a `volatile std::sig_atomic_t` |
| Sin handler SIGHUP → no terminaba al caer SSH | `main.cpp` | Agregado `std::signal(SIGHUP, onSignal)` |
| `std::time(nullptr)` en loop → sensible a NTP | `main.cpp:188` | Cambiado a `std::chrono::steady_clock` |
| Conflicto de pines GPIO vs periféricos | `main.cpp:155` | Reseva dinámica de pines por protocolo |
| I2C `speed_hz` mal nombrado (era divider, no Hz) | `i2c_master.h` | Renombrado a `clock_divider` |
| `speed_t` casteado directamente en UART | `uart_master.cpp:28` | Usa `cfsetispeed`/`cfsetospeed` |
| Pines no restaurados al salir | `port16.cpp`, `onewire_master.cpp` | Se ponen en INPUT al hacer destroy |
| I2S pins (18-21) no estaban reservados | `main.cpp` | Siempre se reservan para evitar conflicto de audio |

---

## Arquitectura

```
main.cpp
 ├── bcm2835_init()           ← única llamada global
 ├── ask_protocols()          ← menú interactivo con flechas ↑↓ y Espacio
 ├── ask_duration()           ← duración en segundos
 ├── PORT16::make(pines)      ← pines libres → señales aleatorias cada tick
 ├── SPI / I2C / UART / 1-Wire / PWM   ← solo si el usuario los habilitó
 ├── loop principal (100 Hz, 10ms tick)
 │   ├── PORT16::write_random()   ← todos los ticks
 │   ├── PWM (si activo)          ← todos los ticks
 │   └── periférico rotado (1 de 4 por tick)
 └── cleanup
     ├── port16.reset()   ← pone pines en INPUT
     ├── spi/i2c/uart/ow.reset()
     └── bcm2835_close()
```

---

## Menú interactivo (cursor)

El menú usa modo RAW del terminal + códigos ANSI:

```
Arrow keys to move, Space=toggle, Enter=confirm
 [X] SPI
  [ ] I2C
  [ ] UART
  [ ] 1-Wire
  [ ] PWM
```

- `↑/↓` : mover cursor (resaltado con `\033[7m`)
- `Espacio` : marcar/desmarcar
- `Enter` : confirmar
- `Ctrl+C` : cancelar

El menú se redibuja en el mismo lugar sin scroll gracias a `\033[<N>A` + `\033[J`.

---

## Reserva de pines

| Periférico | Pines reservados |
|------------|-----------------|
| SPI0 | 7, 8, 9, 10, 11 |
| I2C1 | 2, 3 |
| UART | 14, 15 |
| 1-Wire | 4 |
| PWM | 12, 13 |
| I2S (siempre) | 18, 19, 20, 21 |

Los pines NO reservados se asignan al generador GPIO (máx 16).

---

## Cleanup

Al salir (temporizador, Ctrl+C, SIGHUP, SIGTERM):

1. PWM → `set_data(0)` → `set_mode(enabled=0)` → pines a INPUT
2. Port16 → todos los pines a `FSEL_INPT`
3. OneWire → pin a `FSEL_INPT` + `PUD_UP`
4. SPI/I2C → `bcm2835_spi_end()` / `bcm2835_i2c_end()`
5. `bcm2835_close()` → cierra `/dev/mem`

---

## Compilación

```bash
make -C gpios clean && make -C gpios -j4
sudo ./gpios/bin/gpio_generator
```

Dependencias: `build-essential`, `libbcm2835-dev` o descarga desde airspayce.com.

Instalación automática:

```bash
./script_tools/install_deps.sh
```

---

## Scripts auxiliares

| Script | Función |
|--------|---------|
| `script_tools/install_deps.sh` | Instala todo lo necesario |
| `script_tools/rsync.sh` | Transferencia a la Pi por rsync |

---

## Versiones

| Tag | Cambios |
|-----|---------|
| v1.5.0 | Commit inicial |
| v1.6.0 | Bug fixes críticos + menú + PWM |
| v1.6.1 | Menú checklist |
| v1.7.0 | Menú con cursor (flechas + espacio) |
