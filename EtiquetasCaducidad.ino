/*
 * =========================================================================
 *  Relay de impresión de etiquetas de caducidad
 *  Hardware: ESP32-2432S028R ("Cheap Yellow Display", CYD)
 * =========================================================================
 *
 *  LIBRERÍAS REQUERIDAS:
 *    TFT_eSPI       >= 2.5.43   (Bodmer)
 *    WiFiManager    >= 2.0.17   (tzapu/tablatronix) ← NUEVA
 *    Plataforma ESP32 Arduino   >= 2.0.14 (Espressif en Boards Manager)
 *
 *  CONFIGURACIÓN WIFI:
 *    Las credenciales WiFi NO están hardcodeadas. En el primer arranque
 *    (o al pulsar BOOT 3 s), el ESP32 crea un AP llamado "RelayEtiquetas".
 *    Conéctate a ese AP y abre http://192.168.4.1 para configurar la red
 *    y la IP de la impresora. Los datos se guardan en flash (NVS).
 *
 *  ANTES DE COMPILAR:
 *    Copiar User_Setup.h de esta carpeta dentro de la librería TFT_eSPI.
 * =========================================================================
 */

// =========================================================================
//  CONFIGURACIÓN — valores por defecto (se pueden sobreescribir vía portal)
// =========================================================================

// Nombre y contraseña del AP de configuración que levanta el ESP32
#define CFG_AP_NAME  "RelayEtiquetas"
#define CFG_AP_PASS  ""              // dejar vacío = AP abierto sin contraseña

// IP de impresora por defecto (editable en el portal sin reflashear)
#define PRINTER_IP_DEFAULT   "192.168.1.100"
#define PRINTER_PORT_DEFAULT "9100"

// IP estática para el ESP32 (opcional).
// Comentar #define USE_STATIC_IP para usar DHCP.
// AVISO: si cambias de red, actualiza también la IP estática aquí o usa DHCP.
// #define USE_STATIC_IP
// IPAddress staticIP(192, 168, 1, 200);
// IPAddress gateway (192, 168, 1,   1);
// IPAddress subnet  (255, 255, 255,  0);
// IPAddress dns1    (  8,   8,   8,  8);

// Opciones de impresión
// #define ESCPOS_NO_CUT         // Impresora sin cortador
// #define ESCPOS_STRIP_ACCENTS  // Quitar tildes si salen símbolos raros

// =========================================================================

#include <WiFi.h>
#include <WiFiManager.h>    // tzapu/WiFiManager
#include <Preferences.h>    // NVS flash (incluida en ESP32 Arduino)
#include <WebServer.h>

#include "display_ui.h"
#include "escpos.h"
#include "html_content.h"

// ─── Presets de productos ────────────────────────────────────────────────────
struct Product { const char* name; uint8_t shelfDays; };

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

// ─── Configuración en tiempo de ejecución (cargada desde NVS) ────────────────
static String  runtimePrinterIP   = PRINTER_IP_DEFAULT;
static uint16_t runtimePrinterPort = 9100;

// ─── Estado global ───────────────────────────────────────────────────────────
static WebServer  server(80);
static Preferences prefs;

static bool    wifiConnected      = false;
static bool    lastPrintOk        = false;
static String  lastPrintStatus    = "---";
static bool    displayNeedsUpdate = false;
static unsigned long lastWifiCheck = 0;

static const unsigned long WIFI_CHECK_INTERVAL    = 30000UL;
static const int           PRINTER_CONNECT_TIMEOUT = 3000;

// ─── NVS: cargar / guardar config de impresora ───────────────────────────────
static void loadPrinterConfig() {
  prefs.begin("relay", true);  // read-only
  runtimePrinterIP   = prefs.getString("printer_ip",   PRINTER_IP_DEFAULT);
  runtimePrinterPort = (uint16_t)prefs.getUInt("printer_port", 9100);
  prefs.end();
}

static void savePrinterConfig(const String& ip, uint16_t port) {
  prefs.begin("relay", false);
  prefs.putString("printer_ip",   ip);
  prefs.putUInt("printer_port",   port);
  prefs.end();
  runtimePrinterIP   = ip;
  runtimePrinterPort = port;
}

// ─── Helpers JSON / display (sin cambios respecto a versión anterior) ─────────

static String buildProductsJson() {
  String j;
  j.reserve(NUM_PRODUCTS * 40);
  j = "[";
  for (int i = 0; i < NUM_PRODUCTS; i++) {
    if (i > 0) j += ",";
    j += F("{\"name\":\""); j += PRODUCTS[i].name;
    j += F("\",\"days\":"); j += PRODUCTS[i].shelfDays;
    j += "}";
  }
  j += "]";
  return j;
}

static String jsonField(const String& body, const String& key) {
  String needle = "\"" + key + "\"";
  int idx = body.indexOf(needle);
  if (idx < 0) return "";
  idx += needle.length();
  while (idx < (int)body.length() && (body[idx] == ' ' || body[idx] == ':')) idx++;
  if (idx >= (int)body.length()) return "";
  if (body[idx] == '"') {
    idx++;
    String result; result.reserve(32);
    while (idx < (int)body.length() && body[idx] != '"') {
      if (body[idx] == '\\') idx++;
      result += body[idx++];
    }
    return result;
  }
  int start = idx;
  while (idx < (int)body.length() && body[idx] != ',' && body[idx] != '}' &&
         body[idx] != ' ' && body[idx] != '\n') idx++;
  return body.substring(start, idx);
}

static void refreshDisplay() {
  String ssid = WiFi.SSID();
  drawStatusScreen(WiFi.localIP().toString(), wifiConnected, ssid,
                   runtimePrinterIP, runtimePrinterPort,
                   lastPrintStatus, lastPrintOk);
}

// ─── HTTP handlers ───────────────────────────────────────────────────────────

static void handleRoot() {
  String html = FPSTR(HTML_TEMPLATE);
  html.replace(F("%PRODUCTS_JSON%"), buildProductsJson());
  server.send(200, F("text/html; charset=utf-8"), html);
}

static void handleProducts() {
  server.send(200, F("application/json"), buildProductsJson());
}

static void handlePrint() {
  String body = server.arg("plain");
  if (body.length() == 0) {
    server.send(400, F("application/json"), F("{\"ok\":false,\"msg\":\"Body vacio\"}"));
    return;
  }

  PrintJob job;
  job.producto    = jsonField(body, "producto");
  job.realizacion = jsonField(body, "realizacion");
  job.envasado    = jsonField(body, "envasado");
  job.caducidad   = jsonField(body, "caducidad");
  job.congelado   = jsonField(body, "congelado");
  job.testMode    = (jsonField(body, "test") == "true");

  String copiasStr = jsonField(body, "copias");
  job.copies = (uint8_t)constrain(copiasStr.toInt(), 1, 10);
  if (copiasStr.length() == 0) job.copies = 1;

  if (!job.testMode && job.producto.length() == 0) {
    server.send(400, F("application/json"), F("{\"ok\":false,\"msg\":\"Producto vacio\"}"));
    return;
  }
  if (WiFi.status() != WL_CONNECTED) {
    server.send(503, F("application/json"), F("{\"ok\":false,\"msg\":\"ESP32 sin WiFi\"}"));
    return;
  }

  WiFiClient printer;
  if (!printer.connect(runtimePrinterIP.c_str(), runtimePrinterPort, PRINTER_CONNECT_TIMEOUT)) {
    String err = F("{\"ok\":false,\"msg\":\"Impresora no responde (");
    err += runtimePrinterIP; err += ":"; err += runtimePrinterPort; err += ")\"}";
    server.send(503, F("application/json"), err);
    lastPrintStatus = "ERROR: sin impresora";
    lastPrintOk = false;
    displayNeedsUpdate = true;
    return;
  }

  printLabel(printer, job);
  printer.stop();

  server.send(200, F("application/json"), F("{\"ok\":true}"));
  lastPrintStatus = job.testMode ? "OK (prueba)" : "OK";
  lastPrintOk = true;
  displayNeedsUpdate = true;
  Serial.println(job.testMode ? F("Print: prueba OK") : F("Print: OK"));
}

static void handleNotFound() {
  server.send(404, F("text/plain"), F("Not found"));
}

// ─── WiFiManager setup ───────────────────────────────────────────────────────
static void setupWiFiManager() {
  // Parámetros personalizados que aparecen en el portal de configuración
  char ipBuf[16];   runtimePrinterIP.toCharArray(ipBuf, sizeof(ipBuf));
  char portBuf[6];  snprintf(portBuf, sizeof(portBuf), "%u", runtimePrinterPort);

  WiFiManagerParameter param_ip(
    "pip", "IP de la impresora termica", ipBuf, 15);
  WiFiManagerParameter param_port(
    "pport", "Puerto impresora (normalmente 9100)", portBuf, 5);

  WiFiManager wm;
  wm.addParameter(&param_ip);
  wm.addParameter(&param_port);

  // Callback: se muestra la pantalla de configuración cuando el AP sube
  wm.setAPCallback([](WiFiManager*) {
    drawAPConfigScreen(CFG_AP_NAME, CFG_AP_PASS);
    Serial.println(F("Modo AP config: RelayEtiquetas — abrir 192.168.4.1"));
  });

  // Callback: guardar parámetros cuando el usuario guarda en el portal
  wm.setSaveParamsCallback([&]() {
    uint16_t port = (uint16_t)String(param_port.getValue()).toInt();
    if (port == 0) port = 9100;
    savePrinterConfig(String(param_ip.getValue()), port);
    Serial.printf("Config guardada: impresora %s:%u\n",
                  runtimePrinterIP.c_str(), runtimePrinterPort);
  });

  // Tiempo máximo esperando en el portal antes de reintentar (0 = infinito)
  wm.setConfigPortalTimeout(0);

  // Título del portal web
  wm.setTitle("Relay Etiquetas Caducidad");

#ifdef USE_STATIC_IP
  wm.setSTAStaticIPConfig(staticIP, gateway, subnet, dns1);
#endif

  drawSplash("Conectando WiFi...");

  // autoConnect: si hay credenciales guardadas las usa directamente;
  // si no (o fallan), levanta el AP de configuración.
  bool connected;
  if (strlen(CFG_AP_PASS) > 0) {
    connected = wm.autoConnect(CFG_AP_NAME, CFG_AP_PASS);
  } else {
    connected = wm.autoConnect(CFG_AP_NAME);
  }

  // Actualizar runtime vars con los valores del portal (por si cambiaron)
  {
    uint16_t port = (uint16_t)String(param_port.getValue()).toInt();
    if (port == 0) port = 9100;
    // Solo guardar si son distintos a lo ya almacenado (evitar escrituras NVS innecesarias)
    String newIp = String(param_ip.getValue());
    if (newIp != runtimePrinterIP || port != runtimePrinterPort) {
      savePrinterConfig(newIp, port);
    }
  }

  wifiConnected = connected;
  if (connected) {
    Serial.print(F("WiFi OK — IP: ")); Serial.println(WiFi.localIP());
  } else {
    Serial.println(F("Sin WiFi — continuando offline"));
  }
}

// ─── Reset por botón BOOT ────────────────────────────────────────────────────
// Mantener pulsado el botón BOOT (GPIO 0) durante 3 s en el arranque para
// borrar las credenciales WiFi guardadas y volver al portal de configuración.
static void checkResetButton() {
  pinMode(0, INPUT_PULLUP);
  if (digitalRead(0) == LOW) {
    drawSplash("Mantenga para resetear...");
    int held = 0;
    while (digitalRead(0) == LOW && held < 30) {
      delay(100);
      held++;
      drawWifiProgress(held * 100 / 30);
    }
    if (held >= 30) {
      Serial.println(F("Reset WiFi solicitado — borrando credenciales"));
      WiFiManager wm;
      wm.resetSettings();
      prefs.begin("relay", false);
      prefs.clear();
      prefs.end();
      drawSplash("Reiniciando...");
      delay(1000);
      ESP.restart();
    }
  }
}

// ─── setup ───────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Serial.println(F("\n=== Relay Etiquetas Caducidad ==="));

  displayInit();
  drawSplash("Iniciando...");
  delay(400);

  // Cargar config de impresora desde NVS
  loadPrinterConfig();
  Serial.printf("Impresora: %s:%u\n", runtimePrinterIP.c_str(), runtimePrinterPort);

  // Verificar si el usuario quiere resetear la config WiFi
  checkResetButton();

  // Conectar vía WiFiManager (portal automático si no hay red guardada)
  setupWiFiManager();

  // Rutas HTTP
  server.on("/",         HTTP_GET,  handleRoot);
  server.on("/products", HTTP_GET,  handleProducts);
  server.on("/print",    HTTP_POST, handlePrint);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println(F("Servidor HTTP iniciado (puerto 80)"));

  displayNeedsUpdate = true;
}

// ─── loop ────────────────────────────────────────────────────────────────────
void loop() {
  server.handleClient();

  // Watchdog WiFi
  unsigned long now = millis();
  if (now - lastWifiCheck >= WIFI_CHECK_INTERVAL) {
    lastWifiCheck = now;
    bool connected = (WiFi.status() == WL_CONNECTED);
    if (connected != wifiConnected) {
      wifiConnected = connected;
      displayNeedsUpdate = true;
      if (!connected) {
        Serial.println(F("WiFi perdido — reconectando..."));
        WiFi.reconnect();
      } else {
        Serial.print(F("WiFi reconectado — IP: "));
        Serial.println(WiFi.localIP());
      }
    }
  }

  if (displayNeedsUpdate) {
    displayNeedsUpdate = false;
    refreshDisplay();
  }
}
