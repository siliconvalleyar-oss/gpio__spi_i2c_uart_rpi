# SKILL: Weather Display — E-Paper

## 1. Descripción General

Aplicación que muestra el clima de Buenos Aires en tiempo real en una pantalla
e-paper de 2.66" (296×152), usando la API gratuita de wttr.in.

**Branch**: `Whater`
**Carpeta**: `RASPI_EPD_SRC_Whater/`
**Binario**: `bin/weather_app`

### Funcionalidad

- Clima de Buenos Aires desde wttr.in (cada 30 min)
- Temperatura grande (FONT_16x32_BIGNUM)
- Condición, humedad, viento, sensación térmica, presión, visibilidad
- Banner scroll horizontal con info detallada (FONT_3x8_TINY)
- Scroll avanza 4px cada 8 segundos
- Refresh diferencial (`fastUpdate`) sin flash innecesario

---

## 2. Estructura del Proyecto

```
RASPI_EPD_SRC_Whater/
├── Makefile
├── src/main.cpp              # Entry point, fetch + layout + scroll
├── libs/
│   ├── epaper/               # Driver EPD (EPD_Driver + EpaperDisplay)
│   ├── fonts/                # Fonts bitmap (FontManager)
│   ├── gpio/                 # GPIO legacy (no usado)
│   ├── tyme/                 # delay_ms, delay_us
│   └── app/config.h          # CPU detection
└── docs/SKILL_WHATER.md
```

---

## 3. Arquitectura

### Flujo principal

```
main()
├── init display (globalUpdate)
├── loop:
│   ├── cada 30 min: fetch wttr.in → WeatherData
│   ├── cada 8 seg: advance scrollPx
│   ├── clearScreen(true)
│   ├── render "BUENOS AIRES" title
│   ├── render condition text
│   ├── render big temperature "25C"
│   ├── render details (humidity, wind, feels like, pressure, visibility)
│   ├── render update timestamp
│   ├── render scrolling marquee at new offset
│   └── update() → fastUpdate()
└── shutdown
```

### fetchWeather() — wttr.in API

```bash
curl -s "wttr.in/Buenos+Aires?format=j1"
```

Parseo manual con strstr para extraer:
- `temp_C`, `FeelsLikeC`, `humidity`, `windspeedKmph`
- `pressure`, `visibility`, `weatherDesc[0].value`

### Bucle de tiempo

| Evento | Intervalo |
|:-------|:---------:|
| Fetch clima | 1800s (30 min) |
| Avance scroll | 8s |
| Reintento fallo | próximo ciclo |

---

## 4. Layout (296×152)

```
Y=0:   "BUENOS AIRES"           FONT_7x8_THICK
Y=12:  ──────── separator ─────  drawLine
Y=16:  "Soleado"                FONT_5x8 (condition)
Y=27:  "25" (big) + "C" label   FONT_16x32_BIGNUM + FONT_7x8_THICK
Y=63:  "Humedad: 65%  Viento: 12 km/h"  FONT_5x8
Y=74:  "Sensacion: 23C  Presion: 1013 hPa"  FONT_5x8
Y=85:  "Visibilidad: 10 km"     FONT_5x8
Y=96:  ──────── separator ─────  drawLine
Y=100: "actualizado HH:MM"      FONT_3x8_TINY
Y=110: [scrolling marquee]      FONT_3x8_TINY
```

---

## 5. Scroll Marquee

### Mecanismo

```
marqueeText = "Buenos Aires | Soleado | Temp: 25C | ... | "
textW = getTextWidth(text, FONT_3x8_TINY)
cycle = textW + 30 (gap)
offset = scrollPx % cycle

x1 = 296 - offset    ← first copy
x2 = x1 + textW + 30 ← second copy (wrap)

drawString(x1, y, text, FONT_3x8_TINY)
if x2 + textW > 0 && x2 < 296:
    drawString(x2, y, text, FONT_3x8_TINY)
```

- `scrollPx` avanza 4px cada 8 segundos
- Cuando `scrollPx >= cycle`, el texto reinicia (wrap natural)
- En cada refresh, todo el contenido se redibuja sobre buffer limpio
- `fastUpdate()` con `prevBuffer` solo envía los píxeles cambiados

---

## 6. API

### wttr.in

- URL: `https://wttr.in/Buenos+Aires?format=j1`
- Sin API key
- Timeout: 10s conexión, 15s total
- Retorna JSON con `current_condition[0]`

### Formato de respuesta

```json
{"current_condition": [{
    "temp_C": "25",
    "FeelsLikeC": "23",
    "humidity": "65",
    "windspeedKmph": "12",
    "pressure": "1013",
    "visibility": "10",
    "weatherDesc": [{"value": "Sunny"}]
}]}
```

---

## 7. Dependencias

```bash
sudo apt-get install libbcm2835-dev
```

**No requiere** `libqrencode-dev`.

---

## 8. Build & Run

```bash
cd RASPI_EPD_SRC_Whater
make
sudo TZ=America/Argentina/Buenos_Aires ./bin/weather_app
# o:
make run
```

---

## 9. Deploy Remoto

```bash
rsync -avz --exclude='.git/' --exclude='obj/' --exclude='bin/' \
  RASPI_EPD_SRC_Whater/ pi@raspi.local:/home/pi/src/epaper_rpi/RASPI_EPD_SRC_Whater/

ssh pi@raspi.local "cd /home/pi/src/epaper_rpi/RASPI_EPD_SRC_Whater && \
  make clean && make && sudo TZ=America/Argentina/Buenos_Aires ./bin/weather_app"
```
