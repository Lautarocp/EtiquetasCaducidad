# CLAUDE.md — Relay Etiquetas Caducidad (ESP32-2432S028R)

Instrucciones para compilar y flashear este proyecto desde **Debian Linux** en terminal,
usando `arduino-cli`. No se requiere Arduino IDE ni entorno gráfico.

---

## Dependencias del sistema (Debian/Ubuntu)

```bash
# Herramientas básicas
sudo apt update
sudo apt install -y curl python3 python3-pip git

# Acceso al puerto serie (ESP32 aparece como /dev/ttyUSB0 o /dev/ttyACM0)
# IMPORTANTE: cerrar sesión y volver a entrar para que surta efecto
sudo usermod -aG dialout $USER
```

---

## Instalar arduino-cli

```bash
# Descargar e instalar la versión más reciente
curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh

# El binario queda en ~/bin/arduino-cli — añadir al PATH si no está
echo 'export PATH="$HOME/bin:$PATH"' >> ~/.bashrc
source ~/.bashrc

# Verificar
arduino-cli version
```

---

## Instalar la plataforma ESP32 y las librerías

Ejecutar **una sola vez** tras instalar arduino-cli:

```bash
# 1. Inicializar config
arduino-cli config init

# 2. Añadir repositorio de placas ESP32 (Espressif)
arduino-cli config add board_manager.additional_urls \
  https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json

# 3. Actualizar índice y descargar la plataforma ESP32
arduino-cli core update-index
arduino-cli core install esp32:esp32

# 4. Instalar librerías necesarias
arduino-cli lib install "TFT_eSPI"      # >= 2.5.43
arduino-cli lib install "WiFiManager"   # >= 2.0.17  (tzapu/tablatronix)
```

---

## Configurar TFT_eSPI (paso crítico)

Después de instalar la librería, copiar el `User_Setup.h` de este proyecto
al directorio de la librería TFT_eSPI:

```bash
# Localizar dónde arduino-cli instaló TFT_eSPI
TFTLIB=$(arduino-cli lib list | grep TFT_eSPI | awk '{print $NF}')
# Normalmente está en: ~/Arduino/libraries/TFT_eSPI/

# Hacer copia de seguridad del User_Setup.h original
cp ~/Arduino/libraries/TFT_eSPI/User_Setup.h \
   ~/Arduino/libraries/TFT_eSPI/User_Setup.h.orig

# Copiar el User_Setup.h de este proyecto
cp User_Setup.h ~/Arduino/libraries/TFT_eSPI/User_Setup.h
```

Si el comando `TFTLIB` no funciona, buscar manualmente:

```bash
find ~ -path "*/TFT_eSPI/User_Setup.h" 2>/dev/null
```

---

## Detectar el puerto del ESP32

Conectar el ESP32 por USB y ejecutar:

```bash
ls /dev/ttyUSB* /dev/ttyACM* 2>/dev/null
```

Normalmente aparece como `/dev/ttyUSB0`. Si no aparece nada:

```bash
# Ver los últimos mensajes del kernel al conectar el USB
dmesg | tail -20
```

Buscar una línea como: `usb ... cp210x converter now attached to ttyUSB0`

---

## Compilar

Desde la raíz del proyecto:

```bash
arduino-cli compile \
  --fqbn esp32:esp32:esp32 \
  --build-property "build.partitions=default" \
  EtiquetasCaducidad/
```

Para ver todos los warnings:

```bash
arduino-cli compile --fqbn esp32:esp32:esp32 --warnings all EtiquetasCaducidad/
```

---

## Flashear (compilar + subir en un paso)

```bash
# Reemplazar /dev/ttyUSB0 por el puerto detectado en el paso anterior
arduino-cli compile --upload \
  --fqbn esp32:esp32:esp32 \
  --port /dev/ttyUSB0 \
  EtiquetasCaducidad/
```

Si falla el upload con "permission denied":

```bash
# Verificar que el usuario está en el grupo dialout
groups $USER | grep dialout

# Si no está, añadirlo y reiniciar sesión
sudo usermod -aG dialout $USER
# Cerrar sesión y volver a entrar, luego reintentar
```

Si falla con "Failed to connect":

```bash
# Mantener pulsado el botón BOOT del ESP32 mientras se ejecuta el upload
arduino-cli compile --upload --fqbn esp32:esp32:esp32 --port /dev/ttyUSB0 EtiquetasCaducidad/
# Soltar BOOT cuando aparezca "Connecting..."
```

---

## Monitor serie (debug)

```bash
arduino-cli monitor --port /dev/ttyUSB0 --config baudrate=115200
# Salir con Ctrl+C
```

Alternativa con `screen`:

```bash
sudo apt install screen
screen /dev/ttyUSB0 115200
# Salir: Ctrl+A luego K
```

---

## Flujo completo de primera vez

```bash
# 1. Instalar dependencias del sistema
sudo apt install -y curl python3 git
sudo usermod -aG dialout $USER
# (cerrar sesión y volver a entrar)

# 2. Instalar arduino-cli
curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh
echo 'export PATH="$HOME/bin:$PATH"' >> ~/.bashrc && source ~/.bashrc

# 3. Plataforma y librerías
arduino-cli config init
arduino-cli config add board_manager.additional_urls \
  https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
arduino-cli core update-index && arduino-cli core install esp32:esp32
arduino-cli lib install "TFT_eSPI" "WiFiManager"

# 4. Copiar User_Setup.h
cp User_Setup.h ~/Arduino/libraries/TFT_eSPI/User_Setup.h

# 5. Conectar ESP32 por USB y detectar puerto
ls /dev/ttyUSB*

# 6. Compilar y flashear
arduino-cli compile --upload --fqbn esp32:esp32:esp32 --port /dev/ttyUSB0 EtiquetasCaducidad/

# 7. Ver logs
arduino-cli monitor --port /dev/ttyUSB0 --config baudrate=115200
```

---

## Notas sobre este proyecto

- **WiFi:** no hay credenciales hardcodeadas. Al primer arranque el ESP32 crea
  el AP `RelayEtiquetas`; conectarse y abrir `http://192.168.4.1`.
- **Reset WiFi:** mantener pulsado BOOT (GPIO 0) 3 s al encender.
- **IP impresora:** configurable desde el mismo portal, sin reflashear.
- **Pantalla en blanco:** casi siempre es el `User_Setup.h` incorrecto (paso 4).
