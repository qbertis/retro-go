# ESP32-S3 NES Console

Портативна ігрова консоль на базі ESP32-S3 N16R8 з використанням Retro-Go.

## Hardware

| Компонент | Модель |
|---|---|
| MCU | ESP32-S3 N16R8 (16 MB Flash, 8 MB PSRAM) |
| Display | ST7789V 320×240 SPI |
| Storage | microSD (SPI3) |
| Audio | PCM5102A (I²S) |
| Input | 10 GPIO buttons + USB HID keyboard |

## Pinout

### Display (SPI2)
| Signal | GPIO |
|---|---|
| SCLK | 12 |
| MOSI | 11 |
| CS | 10 |
| DC | 9 |
| RST | 14 |
| BL | 39 |

### microSD (SPI3)
| Signal | GPIO |
|---|---|
| SCK | 15 |
| MOSI | 16 |
| MISO | 17 |
| CS | 18 |

### Audio (I²S)
| Signal | GPIO |
|---|---|
| BCK | 41 |
| WS | 42 |
| DIN | 40 |

### Buttons
| Button | GPIO |
|---|---|
| UP | 4 |
| DOWN | 5 |
| LEFT | 6 |
| RIGHT | 7 |
| A | 13 |
| B | 1 |
| START | 2 |
| SELECT | 21 |
| MENU | 47 |
| OPTION | 48 |

## Build

```bash
# ESP-IDF v5.3.5
cd retro-go
python rg_tool.py --target esp32s3-nes build all
python rg_tool.py --target esp32s3-nes --port COMx flash