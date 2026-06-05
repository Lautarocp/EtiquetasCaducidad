#pragma once
// display_ui.h — Pantalla de estado TFT para ESP32-2432S028R (CYD)
// Requiere TFT_eSPI configurado con User_Setup.h de este proyecto.

#include <TFT_eSPI.h>

// Objeto TFT global (definido aquí, incluir solo desde el .ino principal)
TFT_eSPI tft = TFT_eSPI();

// ─── displayInit ─────────────────────────────────────────────────────────────
// Inicializa el display: enciende backlight, configura orientación landscape.
void displayInit() {
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);   // encender retroiluminación
  tft.init();
  tft.setRotation(1);            // landscape: 320 ancho × 240 alto
  tft.fillScreen(TFT_BLACK);
}

// ─── drawSplash ──────────────────────────────────────────────────────────────
// Pantalla de carga con título y mensaje de estado.
void drawSplash(const char* msg) {
  tft.fillScreen(TFT_NAVY);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_NAVY);
  tft.drawString("RELAY ETIQUETAS", 160, 85, 4);
  tft.setTextColor(TFT_CYAN, TFT_NAVY);
  tft.drawString(msg, 160, 125, 2);
}

// ─── drawWifiProgress ────────────────────────────────────────────────────────
// Barra de progreso durante la conexión WiFi (pct: 0-100).
// Llamar después de drawSplash(); sobrepone solo la barra sin limpiar pantalla.
void drawWifiProgress(int pct) {
  if (pct < 0) pct = 0;
  if (pct > 100) pct = 100;
  tft.fillRect(10, 152, 300, 16, TFT_DARKGREY);
  if (pct > 0) tft.fillRect(10, 152, 3 * pct, 16, 0x035F);  // azul medio
}

// ─── drawStatusScreen ────────────────────────────────────────────────────────
// Redibuja la pantalla de estado completa.
//
//  ip          — IP local del ESP32 (o cadena vacía si sin WiFi)
//  wifiOk      — true si el WiFi está conectado
//  ssid        — nombre de la red WiFi
//  printerIp   — IP de la impresora (literal, para mostrar)
//  printerPort — puerto de la impresora
//  lastStatus  — texto del resultado de la última impresión ("---" = ninguna)
//  lastOk      — true = última impresión OK, false = error
//
void drawStatusScreen(const String& ip, bool wifiOk, const String& ssid,
                      const String& printerIp, uint16_t printerPort,
                      const String& lastStatus, bool lastOk) {
  tft.fillScreen(TFT_BLACK);

  // ── Cabecera ──────────────────────────────────────────────────────────────
  tft.fillRect(0, 0, 320, 30, TFT_NAVY);
  tft.setTextColor(TFT_WHITE, TFT_NAVY);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("RELAY ETIQUETAS", 160, 15, 2);

  tft.setTextDatum(TL_DATUM);

  // ── WiFi ──────────────────────────────────────────────────────────────────
  tft.setTextColor(TFT_SILVER, TFT_BLACK);
  tft.drawString("WiFi:", 5, 38, 2);
  if (wifiOk) {
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    String s = " OK  ";
    s += ssid.substring(0, 16);   // truncar SSID largo
    tft.drawString(s, 48, 38, 2);
  } else {
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.drawString(" RECONECTANDO...", 48, 38, 2);
  }

  // ── URL de acceso ─────────────────────────────────────────────────────────
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  String url = "http://";
  url += wifiOk ? ip : String("---");
  tft.drawString("URL:", 5, 60, 2);
  tft.drawString(url, 42, 60, 2);

  // ── Impresora ─────────────────────────────────────────────────────────────
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  String pr = "Imp: ";
  pr += printerIp;
  pr += ":";
  pr += String(printerPort);
  tft.drawString(pr, 5, 82, 2);

  // ── Separador ─────────────────────────────────────────────────────────────
  tft.drawFastHLine(0, 104, 320, TFT_DARKGREY);

  // ── Última impresión ──────────────────────────────────────────────────────
  tft.setTextColor(TFT_SILVER, TFT_BLACK);
  tft.drawString("Ultima impresion:", 5, 113, 2);

  uint16_t stColor = TFT_DARKGREY;
  if (lastStatus != "---") {
    stColor = lastOk ? TFT_GREEN : TFT_RED;
  }
  tft.setTextColor(stColor, TFT_BLACK);
  String st = lastStatus;
  if (st.length() > 28) st = st.substring(0, 28);
  tft.drawString(st, 5, 135, 2);
}
