# Build & Installation

## Dependencies

- `g++` with C++20 support
- `make`
- `bcm2835` library v1.75+
- `pthread` (included in libc)

### Auto-install

```bash
./script_tools/install_deps.sh
```

Installs: `build-essential`, `git`, `rsync`, `sshpass`, `libbcm2835-dev`.

### Manual (Raspberry Pi OS)

```bash
sudo apt update
sudo apt install -y build-essential git rsync sshpass

# bcm2835 library (if not in apt)
wget http://www.airspayce.com/mikem/bcm2835/bcm2835-1.77.tar.gz
tar xzf bcm2835-1.77.tar.gz
cd bcm2835-1.77
./configure
make -j4
sudo make install
sudo ldconfig
cd ..
rm -rf bcm2835-1.77 bcm2835-1.77.tar.gz
```

## Compile

```bash
make -C gpios clean && make -C gpios -j4
```

## Run

```bash
sudo ./gpios/bin/gpio_generator
```

## Remote (via SSH)

```bash
ssh pi@<ip> "cd /home/pi/src/gpio__spi_i2c_uart_rpi && git pull && make clean && make -j4"
```

## Arguments

```bash
# Duration in seconds (overrides menu prompt)
sudo ./gpios/bin/gpio_generator 30

# Skip interactive menu, enable all protocols
sudo ./gpios/bin/gpio_generator 30 --all
```
