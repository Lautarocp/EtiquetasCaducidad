/*
 * =========================================================================
 *  Relay de impresión de etiquetas de caducidad
 *  Hardware: ESP32-2432S028R ("Cheap Yellow Display", CYD)
 *            Display ILI9341 2.8" 320×240 + touch XPT2046
 * =========================================================================
 *
 *  LIBRERÍAS REQUERIDAS (instalar desde Library Manager o platformio.ini):
 *
 *    TFT_eSPI       >= 2.5.43   (Bodmer)
 *    Plataforma ESP32 Arduino >= 2.0.14  (Espressif en Boards Manager)
 *    WiFi.h, WebServer.h       -> incluidas en la plataforma ESP32
 *
 *  ANTES DE COMPILAR:
 *    Copiar User_Setup.h de esta carpeta dentro de la librería TFT_eSPI.
 *    Ver instrucciones detalladas en README.md y en User_Setup.h.
 *
 * =========================================================================
 */

// =========================================================================
//  CONFIGURACIÓN — editar estas líneas antes de flashear
// =========================================================================

#define WIFI_SSID    "TuRedWiFi"       // Nombre de la red WiFi
#define WIFI_PASS    "TuContrasena"    // Contraseña WiFi

#define PRINTER_IP   "192.168.1.100"  // IP de la impresora térmica de red
#define PRINTER_PORT 9100             // Puerto ESC/POS (casi siempre 9100)

// ── IP estática para el ESP32 ─────────────────────────────────────────────
// Recomendado: evita que el TPV pierda la URL si el router cambia la IP por DHCP.
// Comentar la línea #define USE_STATIC_IP para usar DHCP.
#define USE_STATIC_IP
IPAddress staticIP(192, 168, 1, 200); // IP que tendrá el ESP32
IPAddress gateway (192, 168, 1,   1); // IP del router (puerta de enlace)
IPAddress subnet  (255, 255, 255,  0);
IPAddress dns1    (  8,   8,   8,  8); // DNS (Google, o la IP del router)

// ── Opciones de impresión (descomentar si es necesario) ───────────────────
// #define ESCPOS_NO_CUT         // Impresora sin cortador de papel
// #define ESCPOS_STRIP_ACCENTS  // Quitar tildes/ñ si salen símbolos raros

// =========================================================================

#include <WiFi.h>
#include <WebServer.h>

#include "display_ui.h"   // TFT_eSPI, tft, drawSplash, drawStatusScreen…
#include "escpos.h"       // PrintJob, printLabel()
#include "html_content.h" // HTML_TEMPLATE (PROGMEM)

// ─── Presets de productos ────────────────────────────────────────────────────
// Añadir/quitar líneas según los productos del negocio.
// shelfDays: vida útil en días desde la fecha de realización.
struct Product {
  const char* name;
  uint8_t     shelfDays;
};

const Product PRODUCTS[] = {
  {"Tortilla",          3},
  {"Ensalada",          2},
  {"Carne cocinada",    3},
  {"Pollo cocinado",    3},
  {"Pescado cocinado",  2},
  {"Sopa / Caldo",      3},
  {"Arroz cocinado",    3},
  {"Pasta cocinada",    3},
  {"Patatas cocidas",   3},
  {"Verduras cocidas",  3},
};
static const int NUM_PRODUCTS = sizeof(PRODUCTS) / sizeof(PRODUCTS[0]);

// ─── Estado global ───────────────────────────────────────────────────────────
WebServer server(80);

static bool    wifiConnected      = false;
static bool    lastPrintOk        = false;
static String  lastPrintStatus    = "---";
static bool    displayNeedsUpdate = false;
static unsigned long lastWifiCheck = 0;

static const unsigned long WIFI_CHECK_INTERVAL = 30000UL; // 30 s
static const int           WIFI_CONNECT_TIMEOUT = 20000;  // 20 s
static const int           PRINTER_CONNECT_TIMEOUT = 3000;// 3 s

// ─── Helper: construir JSON de productos ─────────────────────────────────────
// Genera [{«name»:«...",«days»:N},…] para inyectar en el HTML o servir en /products.
static String buildProductsJson() {
  String j;
  j.reserve(NUM_PRODUCTS * 40);
  j = "[";
  for (int i = 0; i < NUM_PRODUCTS; i++) {
    if (i > 0) j += ",";
    j += F("{\"name\":\"");
    j += PRODUCTS[i].name;
    j += F("\",\"days\":");
    j += PRODUCTS[i].shelfDays;
    j += "}";
  }
  j += "]";
  return j;
}

// ─── Helper: extraer campo de JSON plano ─────────────────────────────────────
// Encuentra la primera ocurrencia de "key": valor_o_"cadena" en body.
// Sin dependencia de ArduinoJson para conservar heap.
static String jsonField(const String& body, const String& key) {
  String needle = "\"" + key + "\"";
  int idx = body.indexOf(needle);
  if (idx < 0) return "";
  idx += needle.length();

  // Saltar espacios y ':'
  while (idx < (int)body.length() && (body[idx] == ' ' || body[idx] == ':')) idx++;
  if (idx >= (int)body.length()) return "";

  if (body[idx] == '"') {
    // Valor string
    idx++;
    String result;
    result.reserve(32);
    while (idx < (int)body.length() && body[idx] != '"') {
      if (body[idx] == '\\') idx++;  // saltar escape
      result += body[idx++];
    }
    return result;
  }

  // Valor numérico / booleano
  int start = idx;
  while (idx < (int)body.length() && body[idx] != ',' && body[idx] != '}' &&
         body[idx] != ' ' && body[idx] != '\n') idx++;
  return body.substring(start, idx);
}

// ─── Helper: actualizar display ──────────────────────────────────────────────
static void refreshDisplay() {
  drawStatusScreen(
    WiFi.localIP().toString(),
    wifiConnected,
    String(WIFI_SSID),
    String(PRINTER_IP),
    (uint16_t)PRINTER_PORT,
    lastPrintStatus,
    lastPrintOk
  );
}

// ─── HTTP: GET / ─────────────────────────────────────────────────────────────
static void handleRoot() {
  // Copiar la plantilla de PROGMEM y sustituir el placeholder de productos.
  String html = FPSTR(HTML_TEMPLATE);
  html.replace(F("%PRODUCTS_JSON%"), buildProductsJson());
  server.send(200, F("text/html; charset=utf-8"), html);
}

// ─── HTTP: GET /products ──────────────────────────────────────────────────────
static void handleProducts() {
  server.send(200, F("application/json"), buildProductsJson());
}

// ─── HTTP: POST /print ────────────────────────────────────────────────────────
static void handlePrint() {
  // Leer body (JSON enviado con Content-Type: application/json)
  String body = server.arg("plain");
  if (body.length() == 0) {
    server.send(400, F("application/json"), F("{\"ok\":false,\"msg\":\"Body vacio\"}"));
    return;
  }

  // Construir PrintJob desde el JSON
  PrintJob job;
  job.producto    = jsonField(body, "producto");
  job.realizacion = jsonField(body, "realizacion");
  job.envasado    = jsonField(body, "envasado");
  job.caducidad   = jsonField(body, "caducidad");
  job.congelado   = jsonField(body, "congelado");
  job.testMode    = (jsonField(body, "test") == "true");

  String copiasStr = jsonField(body, "copias");
  job.copies = (uint8_t)constrain(copiasStr.toInt(), 1, 10);
  if (copiasStr.length() == 0) job.copies = 1;  // defecto si falta el campo

  // Validación mínima server-side
  if (!job.testMode && job.producto.length() == 0) {
    server.send(400, F("application/json"), F("{\"ok\":false,\"msg\":\"Producto vacio\"}"));
    return;
  }

  // Verificar que el WiFi sigue activo
  if (WiFi.status() != WL_CONNECTED) {
    server.send(503, F("application/json"), F("{\"ok\":false,\"msg\":\"ESP32 sin WiFi\"}"));
    lastPrintStatus = "ERROR: sin WiFi";
    lastPrintOk = false;
    displayNeedsUpdate = true;
    return;
  }

  // Conectar a la impresora (timeout 3 s, no bloquea más que eso)
  WiFiClient printer;
  if (!printer.connect(PRINTER_IP, PRINTER_PORT, PRINTER_CONNECT_TIMEOUT)) {
    String err = F("{\"ok\":false,\"msg\":\"Impresora no responde (");
    err += PRINTER_IP;
    err += ":";
    err += PRINTER_PORT;
    err += ")\"}";
    server.send(503, F("application/json"), err);
    lastPrintStatus = "ERROR: sin impresora";
    lastPrintOk = false;
    displayNeedsUpdate = true;
    return;
  }

  // Enviar ESC/POS y cerrar conexión
  printLabel(printer, job);
  printer.stop();

  server.send(200, F("application/json"), F("{\"ok\":true}"));
  lastPrintStatus = job.testMode ? "OK (prueba)" : "OK";
  lastPrintOk = true;
  displayNeedsUpdate = true;

  Serial.println(job.testMode ? F("Print: prueba OK") : F("Print: OK"));
}

// ─── HTTP: 404 ────────────────────────────────────────────────────────────────
static void handleNotFound() {
  server.send(404, F("text/plain"), F("Not found"));
}

// ─── WiFi: conectar con progreso en TFT ──────────────────────────────────────
static void connectWiFi() {
#ifdef USE_STATIC_IP
  if (!WiFi.config(staticIP, gateway, subnet, dns1)) {
    Serial.println(F("WiFi.config: fallo al configurar IP estática"));
  }
#endif

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(false); // el watchdog del loop() gestiona la reconexión
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  drawSplash("Conectando WiFi...");

  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - t0 >= (unsigned long)WIFI_CONNECT_TIMEOUT) {
      Serial.println(F("WiFi: timeout, continuando offline"));
      break;
    }
    delay(200);
    int pct = (int)((millis() - t0) * 100UL / WIFI_CONNECT_TIMEOUT);
    drawWifiProgress(pct);
    Serial.print('.');
  }
  Serial.println();

  wifiConnected = (WiFi.status() == WL_CONNECTED);
  if (wifiConnected) {
    Serial.print(F("WiFi OK — IP: "));
    Serial.println(WiFi.localIP());
  } else {
    Serial.println(F("Sin WiFi — el servidor sigue activo para cuando conecte"));
  }
}

// ─── setup ───────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Serial.println(F("\n=== Relay Etiquetas Caducidad ==="));

  // Display
  displayInit();
  drawSplash("Iniciando...");
  delay(400);

  // WiFi
  connectWiFi();

  // Rutas HTTP
  server.on("/",         HTTP_GET,  handleRoot);
  server.on("/products", HTTP_GET,  handleProducts);
  server.on("/print",    HTTP_POST, handlePrint);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println(F("Servidor HTTP iniciado (puerto 80)"));

  // Mostrar pantalla de estado
  displayNeedsUpdate = true;
}

// ─── loop ────────────────────────────────────────────────────────────────────
void loop() {
  // Atender peticiones HTTP
  server.handleClient();

  // Watchdog WiFi (cada 30 s)
  unsigned long now = millis();
  if (now - lastWifiCheck >= WIFI_CHECK_INTERVAL) {
    lastWifiCheck = now;
    bool connected = (WiFi.status() == WL_CONNECTED);
    if (connected != wifiConnected) {
      wifiConnected = connected;
      displayNeedsUpdate = true;
      if (!connected) {
        Serial.println(F("WiFi perdido — intentando reconectar..."));
        WiFi.reconnect();
      } else {
        Serial.print(F("WiFi reconectado — IP: "));
        Serial.println(WiFi.localIP());
      }
    }
  }

  // Actualizar pantalla si hay cambios de estado (no bloquea el loop)
  if (displayNeedsUpdate) {
    displayNeedsUpdate = false;
    refreshDisplay();
  }
}
