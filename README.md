# Relay de impresión de etiquetas de caducidad — ESP32-2432S028R (CYD)

El ESP32 se conecta a la red WiFi, sirve una webapp para diseñar etiquetas de caducidad de
alimentos y reenvía cada impresión a una impresora térmica de red por TCP/ESC/POS.

---

## Contenido del proyecto

```
EtiquetasCaducidad/
├── EtiquetasCaducidad.ino  ← sketch principal, configuración, rutas HTTP
├── display_ui.h            ← pantalla de estado TFT (TFT_eSPI)
├── escpos.h                ← generación manual de comandos ESC/POS
├── html_content.h          ← frontend HTML/CSS/JS embebido en PROGMEM
├── User_Setup.h            ← configuración de TFT_eSPI para la CYD
└── README.md               ← este archivo
```

---

## 1. Librerías requeridas

| Librería | Versión mínima | Dónde instalar |
|----------|---------------|----------------|
| **TFT_eSPI** (Bodmer) | 2.5.43 | Arduino Library Manager / `lib_deps` PlatformIO |
| **WiFiManager** (tzapu/tablatronix) | 2.0.17 | Arduino Library Manager → buscar "WiFiManager by tzapu" |
| **ESP32 Arduino board package** (Espressif) | 2.0.14 | Boards Manager → "esp32 by Espressif" |
| WiFi.h, WebServer.h, Preferences.h | — | Incluidas en el paquete ESP32 anterior |

> No se requiere ArduinoJson.

---

## 2. Configuración WiFi (portal automático)

Las credenciales WiFi **no están hardcodeadas**. El ESP32 usa WiFiManager:

### Primera vez (o tras reset)

1. Al arrancar sin red configurada, el ESP32 levanta un AP propio:
   - **Nombre:** `RelayEtiquetas`
   - **Contraseña:** ninguna (AP abierto por defecto)
2. La pantalla del CYD muestra la instrucción de conexión.
3. Desde el móvil o portátil, conéctate a esa red WiFi.
4. Abre **http://192.168.4.1** en el navegador.
5. Pulsa **Configure WiFi**, elige tu red, introduce la contraseña.
6. En la misma pantalla aparecen dos campos extra:
   - **IP de la impresora térmica** (por defecto `192.168.1.100`)
   - **Puerto** (por defecto `9100`)
7. Pulsa **Save**. El ESP32 reinicia y conecta a tu red.

### Cambiar de red más adelante

Mantén pulsado el botón **BOOT** (GPIO 0) del ESP32 durante **3 segundos**
en el arranque. El ESP32 borra las credenciales y vuelve al portal.

### Cambiar solo la IP de la impresora

Sin reflashear ni tocar la red WiFi: pulsa BOOT 3 s → portal → cambia solo
el campo de IP de la impresora → Save.

---

## 2. Configuración de TFT_eSPI — `User_Setup.h` (CRÍTICO)

**Este es el paso donde más falla la gente.** El `User_Setup.h` que está en
la carpeta del sketch es solo de referencia; hay que copiarlo dentro de la librería.

### Dónde está la librería

| Entorno | Ruta |
|---------|------|
| Arduino IDE (Windows) | `C:\Users\<tu_usuario>\Documents\Arduino\libraries\TFT_eSPI\` |
| Arduino IDE (macOS) | `~/Documents/Arduino/libraries/TFT_eSPI/` |
| Arduino IDE (Linux) | `~/Arduino/libraries/TFT_eSPI/` |
| PlatformIO | `.pio/libdeps/<env>/TFT_eSPI/` |

### Pasos

1. Localiza la carpeta `TFT_eSPI` de la librería.
2. Haz una copia de seguridad del `User_Setup.h` original.
3. Copia el `User_Setup.h` de este proyecto reemplazando el de la librería.
4. Si usas `User_Setup_Select.h`, asegúrate de que **ninguna otra línea
   `#include` esté activa** (todas comentadas excepto `#include <User_Setup.h>`).

### Pines del CYD (ESP32-2432S028R)

```
Display ILI9341 (bus SPI2 / VSPI):
  GPIO 13 → MOSI      GPIO 14 → CLK
  GPIO 12 → MISO      GPIO 15 → CS
  GPIO  2 → DC        GPIO 21 → Backlight (HIGH=encendido)
  RST     → no conectado

Touch XPT2046 (bus SPI3 / HSPI — no usado en este firmware):
  GPIO 32 → MOSI      GPIO 25 → CLK
  GPIO 39 → MISO      GPIO 33 → CS
  GPIO 36 → IRQ
```

---

## 3. IP de la impresora — valores por defecto en el código

Si quieres cambiar los valores que aparecen pre-rellenados en el portal,
edita en `EtiquetasCaducidad.ino`:

```cpp
#define PRINTER_IP_DEFAULT   "192.168.1.100"
#define PRINTER_PORT_DEFAULT "9100"
```

Estos son solo los valores iniciales del formulario del portal.
Una vez guardados desde el portal, los valores en NVS tienen prioridad.

---

## 4. IP estática (opcional)

Por defecto se usa DHCP (más flexible al cambiar de red). Si el TPV accede
siempre a la misma URL, conviene fijar una IP estática para el ESP32.

Descomenta y ajusta estas líneas en `EtiquetasCaducidad.ino`:

```cpp
#define USE_STATIC_IP
IPAddress staticIP(192, 168, 1, 200); // ← IP deseada para el ESP32
IPAddress gateway (192, 168, 1,   1); // ← IP del router
IPAddress subnet  (255, 255, 255,  0);
IPAddress dns1    (  8,   8,   8,  8);
```

> Si cambias de red WiFi (con el portal), recuerda actualizar también la IP
> estática para que corresponda a la nueva subred.

---

## 5. Compilar y flashear

### Arduino IDE

1. Abre `EtiquetasCaducidad.ino`.
2. En **Herramientas → Placa** selecciona `ESP32 Dev Module`
   (o `ESP32-WROOM-32`, el CYD es compatible con ambas).
3. Configura:
   - **Flash Size**: 4MB (32Mb)
   - **Partition Scheme**: Default 4MB with spiffs  
   - **Upload Speed**: 921600
4. Selecciona el puerto COM/tty correcto.
5. Pulsa **Subir** (Ctrl+U).

### PlatformIO

Crea `platformio.ini` en la raíz del proyecto:

```ini
[env:cyd]
platform  = espressif32
board     = esp32dev
framework = arduino
monitor_speed = 115200
lib_deps  =
    bodmer/TFT_eSPI @ ^2.5.43
    tzapu/WiFiManager @ ^2.0.17
```

Luego ejecuta `pio run --target upload`.

> **PlatformIO**: copia `User_Setup.h` a `.pio/libdeps/cyd/TFT_eSPI/User_Setup.h`
> después del primer `pio run` (que descarga la librería).

---

## 6. Abrir la webapp desde el TPV

1. Una vez flasheado, la pantalla del CYD muestra la IP del dispositivo:
   ```
   URL: http://192.168.1.200
   ```
2. Abre esa URL en el navegador del TPV (o cualquier dispositivo en la misma red).
3. Rellena el formulario y pulsa **Imprimir**.

---

## 7. Personalizar productos

Los presets de productos están en el array `PRODUCTS[]` dentro de `EtiquetasCaducidad.ino`.
Cada entrada tiene el nombre y la vida útil en días:

```cpp
const Product PRODUCTS[] = {
  {"Tortilla",         3},
  {"Ensalada",         2},
  // … añadir/quitar entradas aquí
};
```

---

## 8. Troubleshooting

### Pantalla en blanco o con colores incorrectos

- El `User_Setup.h` dentro de la librería TFT_eSPI **no es el correcto**.
  Repite el paso 2 de este README.
- Comprueba que en `User_Setup_Select.h` solo esté activa la línea
  `#include <User_Setup.h>` (sin `//` delante).
- Asegúrate de que el pin `TFT_BL = 21` está en HIGH (backlight encendido).

### La impresora no imprime

1. Confirma la IP: haz `ping 192.168.1.100` desde el ordenador.
2. Verifica el puerto: `telnet 192.168.1.100 9100` debe conectar.
3. Comprueba que la impresora está encendida, tiene papel y está en modo red.
4. Mira el monitor serie (115200 baud): el sketch imprime errores TCP.
5. El ESP32 y la impresora deben estar en la **misma subred**.

### Los acentos y la ñ salen como símbolos raros

La impresora puede usar un code page diferente a WPC1252 (0x10).
Prueba cambiando `0x10` por otro valor en `escpos.h`:

```cpp
const uint8_t codepage[] = {0x1B, 0x74, 0x02};  // CP850
const uint8_t codepage[] = {0x1B, 0x74, 0x00};  // CP437 (solo inglés)
```

O activa el modo de compatibilidad sin tildes en `EtiquetasCaducidad.ino`
(antes de los `#include`):

```cpp
#define ESCPOS_STRIP_ACCENTS
```

### La impresora no corta el papel

Desactiva el comando de corte en `EtiquetasCaducidad.ino`:

```cpp
#define ESCPOS_NO_CUT
```

### El ESP32 no conecta al WiFi

- Verifica SSID y contraseña (son case-sensitive).
- El SSID de 5 GHz no es compatible con el ESP32; usa la banda de **2.4 GHz**.
- Si el router tiene MAC filtering, añade la MAC del ESP32.

### El servidor web deja de responder tras varios minutos

- Comprueba que `server.handleClient()` se llama en cada iteración de `loop()`.
- Si usas `delay()` en el `loop()`, elimínalos.

---

## Licencia

Libre para uso personal y comercial. Sin garantías.
