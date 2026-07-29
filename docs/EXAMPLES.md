# Usage Examples

## Run interactively

```bash
cd gpios && sudo ./bin/gpio_generator
```

Aparece el menú interactivo:

```
Arrow keys to move, Space=toggle, Enter=confirm
 [X] SPI
  [ ] I2C
  [ ] UART
  [ ] 1-Wire
  [ ] PWM
```

- `↑/↓` mover cursor
- `Espacio` marcar/desmarcar
- `Enter` confirmar
- `Ctrl+C` cancelar

Luego pide duración:

```
Duration in seconds (default 10):
```

## Sin menú interactivo (todos los protocolos)

```bash
sudo ./gpios/bin/gpio_generator 60 --all
```

## Solo duración (con menú)

```bash
sudo ./gpios/bin/gpio_generator 30
```

## Compilar y ejecutar en una línea

```bash
make -C gpios clean && make -C gpios -j4 && sudo ./gpios/bin/gpio_generator
```

## Transferir a Raspberry Pi con rsync

```bash
# Configurar contraseña (una vez)
export SSHPASS='tu-contraseña'

# Transferir proyecto completo
rsync -avzP --rsh="sshpass -e ssh" gpio_generator_16bits/ pi@rpi2w.local:/home/pi/src/

# O usar el script interactivo
./script_tools/rsync.sh
```

## Compilación remota

```bash
sshpass -e ssh pi@rpi2w.local \
  "cd /home/pi/src/gpio__spi_i2c_uart_rpi && git pull && make clean && make -j4"
```

## Instalar dependencias en la Pi

```bash
./script_tools/install_deps.sh
```

## Ver pines GPIO activos

Al iniciar, la app muestra qué pines está usando:

```
GPIO pins: 12 (0,1,5,6,16,17,22,23,24,25,26,27)
```

## Output durante ejecución

```
=== GPIO Generator ===
Start:  2026-07-29 18:22:50
GPIO pins: 16 (0,1,2,3,5,6,12,13,14,15,16,17,22,23,24,25)
SPI enabled
1-Wire enabled
Duration: 10s
Press Ctrl+C to stop.
Done. Time: 2026-07-29 18:23:00  Total ticks: 1000
```
