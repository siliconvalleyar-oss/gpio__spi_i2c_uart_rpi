# GPIO Pin Assignments

## Pinout RPi 40-pin Header (GPIO 0-27)

| GPIO | Physical Pin | Default Function | Reserved By |
|------|-------------|-----------------|-------------|
| 0 | 27 | ID_SD (HAT) | — |
| 1 | 28 | ID_SC (HAT) | — |
| 2 | 3 | I2C1 SDA | I2C |
| 3 | 5 | I2C1 SCL | I2C |
| 4 | 7 | 1-Wire / GPCLK0 | 1-Wire |
| 5 | 29 | — | — |
| 6 | 31 | — | — |
| 7 | 26 | SPI0 CE1 | SPI |
| 8 | 24 | SPI0 CE0 | SPI |
| 9 | 21 | SPI0 MISO | SPI |
| 10 | 19 | SPI0 MOSI | SPI |
| 11 | 23 | SPI0 SCLK | SPI |
| 12 | 32 | PWM0 | PWM |
| 13 | 33 | PWM1 | PWM |
| 14 | 8 | UART TXD | UART |
| 15 | 10 | UART RXD | UART |
| 16 | 36 | — | — |
| 17 | 11 | — | — |
| 18 | 12 | PCM CLK / I2S BCLK | I2S (siempre) |
| 19 | 35 | PCM FS / I2S LRCLK | I2S (siempre) |
| 20 | 38 | PCM DIN / I2S DIN | I2S (siempre) |
| 21 | 40 | PCM DOUT / I2S DOUT | I2S (siempre) |
| 22 | 15 | — | — |
| 23 | 16 | — | — |
| 24 | 18 | — | — |
| 25 | 22 | — | — |
| 26 | 37 | — | — |
| 27 | 13 | — | — |

## Reservation Logic

En `main.cpp`, `pins_for_protocol()` define qué pines reserva cada periférico:

```cpp
SPI:   {7, 8, 9, 10, 11}
I2C:   {2, 3}
UART:  {14, 15}
1-W:   {4}
PWM:   {12, 13}
I2S:   {18, 19, 20, 21}   // siempre reservados
```

Los pines libres se asignan al generador GPIO (máx 16).

## Ejemplo: todos los periféricos activos

Reservados: 2, 3, 4, 7, 8, 9, 10, 11, 12, 13, 14, 15, 18, 19, 20, 21  
Disponibles para GPIO: **0, 1, 5, 6, 16, 17, 22, 23, 24, 25, 26, 27** (12 pines)

## Ejemplo: solo SPI + PWM

Reservados: 7, 8, 9, 10, 11, 12, 13 + 18, 19, 20, 21 (I2S)  
Disponibles para GPIO: **0, 1, 2, 3, 4, 5, 6, 14, 15, 16, 17, 22, 23, 24, 25, 26, 27** (17 → capped at 16)
