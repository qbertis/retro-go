#pragma once

// ============================================================
// Target definition
// ============================================================
#define RG_TARGET_NAME             "esp32s3-nes"

// ============================================================
// Storage — microSD on SPI3_HOST
// ============================================================
#define RG_STORAGE_ROOT             "/sd"
#define RG_STORAGE_SDSPI_HOST       SPI3_HOST
#define RG_STORAGE_SDSPI_SPEED      SDMMC_FREQ_DEFAULT

// ============================================================
// Audio — external I2S DAC (PCM5102A)
// ============================================================
#define RG_AUDIO_USE_INT_DAC        0
#define RG_AUDIO_USE_EXT_DAC        1
#define RG_AUDIO_SAMPLE_RATE        48000

// ============================================================
// Video — ST7789V 320x240 on SPI2_HOST
// ============================================================
#define RG_SCREEN_DRIVER            0
#define RG_SCREEN_HOST              SPI2_HOST
#define RG_SCREEN_SPEED             SPI_MASTER_FREQ_40M
#define RG_SCREEN_BACKLIGHT         1
#define RG_SCREEN_WIDTH             320
#define RG_SCREEN_HEIGHT            240
#define RG_SCREEN_ROTATE            0
#define RG_SCREEN_VISIBLE_AREA      {0, 0, 0, 0}
#define RG_SCREEN_SAFE_AREA         {0, 0, 0, 0}

#define RG_SCREEN_INIT()                                                       \
    ILI9341_CMD(0x01);                              /* SWRESET */            \
    ILI9341_CMD(0x11);                              /* SLPOUT */             \
    ILI9341_CMD(0x3A, 0x55);                        /* COLMOD = 16 bit */    \
    ILI9341_CMD(0x36, 0x60);                        /* MADCTL: MV|MY (180°) */\
    ILI9341_CMD(0xB2, 0x0C, 0x0C, 0x00, 0x33, 0x33);/* PORCTRL */            \
    ILI9341_CMD(0xB7, 0x35);                        /* GCTRL */              \
    ILI9341_CMD(0xBB, 0x19);                        /* VCOMS */              \
    ILI9341_CMD(0xC0, 0x2C);                        /* LCMCTRL */            \
    ILI9341_CMD(0xC2, 0x01);                        /* VDVVRHEN */           \
    ILI9341_CMD(0xC3, 0x12);                        /* VRHS */               \
    ILI9341_CMD(0xC4, 0x20);                        /* VDVS */               \
    ILI9341_CMD(0xC6, 0x0F);                        /* FRCTRL2 */            \
    ILI9341_CMD(0xD0, 0xA4, 0xA1);                  /* PWCTRL1 */            \
    ILI9341_CMD(0xE0, 0xD0, 0x04, 0x0D, 0x11, 0x13, 0x2B, 0x3F, 0x54,       \
                      0x4C, 0x18, 0x0D, 0x0B, 0x1F, 0x23); /* PVGAMCTRL */   \
    ILI9341_CMD(0xE1, 0xD0, 0x04, 0x0C, 0x11, 0x13, 0x2C, 0x3F, 0x44,       \
                      0x51, 0x2F, 0x1F, 0x1F, 0x20, 0x23); /* NVGAMCTRL */   \
    ILI9341_CMD(0x21);                              /* INVON */              \
    ILI9341_CMD(0x13);                              /* NORON */              \
    ILI9341_CMD(0x29)                               /* DISPON */

// ============================================================
// Input — GPIO buttons
// ============================================================
#define RG_GAMEPAD_GPIO_MAP {\
    {RG_KEY_UP,     .num = GPIO_NUM_4,  .pullup = 1, .level = 0},\
    {RG_KEY_DOWN,   .num = GPIO_NUM_5,  .pullup = 1, .level = 0},\
    {RG_KEY_LEFT,   .num = GPIO_NUM_6,  .pullup = 1, .level = 0},\
    {RG_KEY_RIGHT,  .num = GPIO_NUM_7,  .pullup = 1, .level = 0},\
    {RG_KEY_A,      .num = GPIO_NUM_13, .pullup = 1, .level = 0},\
    {RG_KEY_B,      .num = GPIO_NUM_1,  .pullup = 1, .level = 0},\
    {RG_KEY_START,  .num = GPIO_NUM_2,  .pullup = 1, .level = 0},\
    {RG_KEY_SELECT, .num = GPIO_NUM_21, .pullup = 1, .level = 0},\
    {RG_KEY_MENU,   .num = GPIO_NUM_47, .pullup = 1, .level = 0},\
    {RG_KEY_OPTION, .num = GPIO_NUM_48, .pullup = 1, .level = 0},\
}

// ============================================================
// Battery — ADC1_CHANNEL_7 (GPIO8) via 2x100k divider
// ============================================================
#define RG_BATTERY_DRIVER           1
#define RG_BATTERY_ADC_UNIT         ADC_UNIT_1
#define RG_BATTERY_ADC_CHANNEL      ADC_CHANNEL_7   // GPIO8
#define RG_BATTERY_CALC_PERCENT(raw) (((raw) * 2.f - 3200.f) / (4200.f - 3200.f) * 100.f)
#define RG_BATTERY_CALC_VOLTAGE(raw) ((raw) * 2.f * 0.001f)

// ============================================================
// SPI Display pins
// ============================================================
#define RG_GPIO_LCD_MISO            GPIO_NUM_NC
#define RG_GPIO_LCD_MOSI            GPIO_NUM_11
#define RG_GPIO_LCD_CLK             GPIO_NUM_12
#define RG_GPIO_LCD_CS              GPIO_NUM_10
#define RG_GPIO_LCD_DC              GPIO_NUM_9
#define RG_GPIO_LCD_RST             GPIO_NUM_14
#define RG_GPIO_LCD_BCKL            GPIO_NUM_39

// ============================================================
// microSD pins — SPI3_HOST
// ============================================================
#define RG_GPIO_SDSPI_MISO          GPIO_NUM_17
#define RG_GPIO_SDSPI_MOSI          GPIO_NUM_16
#define RG_GPIO_SDSPI_CLK           GPIO_NUM_15
#define RG_GPIO_SDSPI_CS            GPIO_NUM_18

// ============================================================
// I2S DAC (PCM5102A)
// ============================================================
#define RG_GPIO_SND_I2S_BCK         41
#define RG_GPIO_SND_I2S_WS          42
#define RG_GPIO_SND_I2S_DATA        40