// =============================================================================
//  User_Setup.h — TFT_eSPI para ESP32-2432S028R ("Cheap Yellow Display", CYD)
// =============================================================================
//
//  INSTRUCCIONES:
//  Copiar este archivo a la carpeta de la librería TFT_eSPI y reemplazar el
//  User_Setup.h que ya existe ahí:
//
//    Windows : Documents\Arduino\libraries\TFT_eSPI\User_Setup.h
//    macOS   : ~/Documents/Arduino/libraries/TFT_eSPI/User_Setup.h
//    Linux   : ~/Arduino/libraries/TFT_eSPI/User_Setup.h
//    PlatformIO: .pio/libdeps/<env>/TFT_eSPI/User_Setup.h
//
//  Si la pantalla queda en BLANCO, casi siempre es un error aquí.
//  Verifica que estés editando el User_Setup.h DENTRO de la librería,
//  no el que está en la carpeta del sketch.
// =============================================================================

// ─── DRIVER ──────────────────────────────────────────────────────────────────
#define ILI9341_DRIVER

#define TFT_WIDTH  240
#define TFT_HEIGHT 320

// ─── PINES SPI DEL DISPLAY (bus SPI2 / VSPI del ESP32) ──────────────────────
//
//  CYD wiring:
//    GPIO 13 → TFT MOSI (data)
//    GPIO 12 → TFT MISO (lectura de display, opcional)
//    GPIO 14 → TFT CLK  (reloj SPI)
//    GPIO 15 → TFT CS   (chip select)
//    GPIO  2 → TFT DC   (data/command)
//    RST     → no conectado (el ILI9341 se resetea por software)
//
#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS   15
#define TFT_DC    2
#define TFT_RST  -1   // Sin pin de reset físico

// ─── RETROILUMINACIÓN ────────────────────────────────────────────────────────
#define TFT_BL           21   // GPIO 21 controla el backlight
#define TFT_BACKLIGHT_ON HIGH  // HIGH = encendido

// ─── TOUCH XPT2046 (bus SPI3 / HSPI del ESP32) ───────────────────────────────
//
//  El touch usa un bus SPI separado:
//    GPIO 32 → Touch MOSI
//    GPIO 39 → Touch MISO
//    GPIO 25 → Touch CLK
//    GPIO 33 → Touch CS   ← este es el único que necesita TFT_eSPI
//    GPIO 36 → Touch IRQ  (no usado en este firmware)
//
#define TOUCH_CS 33

// ─── FRECUENCIAS SPI ─────────────────────────────────────────────────────────
#define SPI_FREQUENCY       40000000   // ILI9341 soporta hasta 40 MHz
#define SPI_READ_FREQUENCY  20000000   // Lecturas más lentas
#define SPI_TOUCH_FREQUENCY  2500000   // XPT2046 máximo 2.5 MHz

// ─── FUENTES CARGADAS ────────────────────────────────────────────────────────
#define LOAD_GLCD     // Font 1 — 8px (GLCD, mínima)
#define LOAD_FONT2    // Font 2 — 16px (pequeña, usada para texto de estado)
#define LOAD_FONT4    // Font 4 — 26px (mediana, usada para splash/título)
#define LOAD_FONT6    // Font 6 — 48px (grande, dígitos)
#define LOAD_FONT7    // Font 7 — 7 segmentos
#define LOAD_FONT8    // Font 8 — 75px (muy grande)
#define LOAD_GFXFF    // FreeFonts (compatibles con Adafruit GFX)
#define SMOOTH_FONT   // Fuentes antialiased desde SPIFFS/LittleFS
