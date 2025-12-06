// LilyGo T-Display-S3 Configuration
#define USER_SETUP_LOADED

#define ST7789_DRIVER

#define TFT_WIDTH  170
#define TFT_HEIGHT 320

#define TFT_RGB_ORDER TFT_BGR

// Pin definitions from PinConfig.h
#define TFT_MOSI -1
#define TFT_SCLK -1
#define TFT_CS    6
#define TFT_DC    7
#define TFT_RST   5
#define TFT_BL   38

#define TFT_D0   39
#define TFT_D1   40
#define TFT_D2   41
#define TFT_D3   42
#define TFT_D4   45
#define TFT_D5   46
#define TFT_D6   47
#define TFT_D7   48

#define TFT_WR    8
#define TFT_RD    9

// Interface mode
#define TFT_PARALLEL_8_BIT

// Backlight control
#define TFT_BACKLIGHT_ON HIGH

// Frequencies
#define SPI_FREQUENCY  40000000
#define SPI_READ_FREQUENCY  16000000
