# RSYNC & Ejemplos de Uso

## Requisitos

- `rsync` instalado en local y remoto.
- `sshpass` instalado en local.
- Acceso SSH a `pi@rpi2w.local`.
- Contraseña configurada en `script_tools/.sshpass.env` o variable de entorno `SSHPASS`.

## Configuración inicial

```bash
# Opción 1: variable de entorno (solo sesión actual)
export SSHPASS='tu-contraseña'

# Opción 2: archivo local secreto (recomendado)
cat > script_tools/.sshpass.env <<'EOF'
SSHPASS='tu-contraseña'
EOF
chmod 600 script_tools/.sshpass.env
```

> `SSHPASS` y `.sshpass.env` están en `.gitignore`. No los subas al repositorio.

## Uso del script interactivo

```bash
./script_tools/rsync.sh
```

### Menú principal

1. Transferir proyecto completo
2. Transferir carpeta específica
3. Transferir archivo(s) específico(s)
4. Configurar contraseña SSH (SSHPASS)
5. Ver ejemplos de uso
0. Salir

## Ejemplos de transferencia

### Proyecto completo

```bash
export SSHPASS='tu-contraseña'
rsync -avzP --rsh="sshpass -e ssh" gpio_generator_16bits/ pi@rpi2w.local:/home/pi/src/
```

### Solo carpeta `gpios/`

```bash
export SSHPASS='tu-contraseña'
rsync -avzP --rsh="sshpass -e ssh" gpio_generator_16bits/gpios/ pi@rpi2w.local:/home/pi/src/gpio_generator_16bits/gpios/
```

### Archivo específico

```bash
export SSHPASS='tu-contraseña'
rsync -avzP --rsh="sshpass -e ssh" gpio_generator_16bits/gpios/Makefile pi@rpi2w.local:/home/pi/src/gpio_generator_16bits/gpios/Makefile
```

## Compilar en la Raspberry Pi

```bash
ssh pi@rpi2w.local
cd /home/pi/src/gpio_generator_16bits/gpios
make clean && make
```

## Ejecutar en la Raspberry Pi

```bash
# Duración por defecto: 120 segundos
cd /home/pi/src/gpio_generator_16bits/gpios
sudo ./bin/gpio_generator

# Duración personalizada: 60 segundos
sudo ./bin/gpio_generator 60

# 5 minutos
sudo ./bin/gpio_generator 300
```

## Parámetros de salida

El programa imprime en consola líneas como:

```
[SPI] TX=0x3A RX=0x7F
[I2C] Wrote 0xA1
[UART] Sent 6 bytes
[1W] Presence, wrote 0x55
[GPIO] Port16=0b1100110010101101 0xCAD5
```

## Notas

- SPI rota con una fase cada 4 ticks.
- I2C, UART y 1-Wire se ejecutan de a uno por tick.
- Los 16 GPIO reflejan un valor aleatorio cada tick.
- Cada tick dura ~10 ms.
- El programa termina automáticamente al cumplirse el tiempo configurado.
