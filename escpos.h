#pragma once
// escpos.h — Generación de comandos ESC/POS para impresora térmica de red
//
// Escribe el ticket directamente al WiFiClient ya conectado; sin buffers
// intermedios grandes para conservar heap.
//
// Ajustes opcionales (descomentar en el .ino ANTES de incluir este header):
//
//   #define ESCPOS_NO_CUT        — Desactiva el corte automático de papel
//   #define ESCPOS_STRIP_ACCENTS — Reemplaza tildes/ñ por ASCII plano en lugar
//                                  de usar code page (útil si salen símbolos raros)

#include <WiFiClient.h>

// ─── PrintJob ─────────────────────────────────────────────────────────────────
struct PrintJob {
  String producto;
  String realizacion;   // formato dd/mm/yyyy
  String envasado;      // formato dd/mm/yyyy
  String caducidad;     // formato dd/mm/yyyy
  String congelado;     // formato dd/mm/yyyy — vacío = línea no impresa
  uint8_t copies = 1;
  bool testMode  = false;
};

// ─── Helpers internos ─────────────────────────────────────────────────────────

#ifdef ESCPOS_STRIP_ACCENTS
// Elimina tildes y caracteres especiales del español (fallback sin code page).
static void _stripAccents(String& s) {
  // Los literales están en UTF-8 (dos bytes por carácter acentuado)
  const char* src[] = {
    "\xC3\xA1","\xC3\xA9","\xC3\xAD","\xC3\xB3","\xC3\xBA",  // á é í ó ú
    "\xC3\x81","\xC3\x89","\xC3\x8D","\xC3\x93","\xC3\x9A",  // Á É Í Ó Ú
    "\xC3\xB1","\xC3\x91","\xC3\xBC","\xC3\x9C"               // ñ Ñ ü Ü
  };
  const char* dst[] = {
    "a","e","i","o","u","A","E","I","O","U","n","N","u","U"
  };
  for (int i = 0; i < 14; i++) s.replace(src[i], dst[i]);
}
#endif

// Envía un array de bytes al cliente (helper para comandos ESC/POS).
static inline void _cmd(WiFiClient& c, const uint8_t* buf, size_t len) {
  c.write(buf, len);
}

// Convierte una cadena UTF-8 a WPC1252 y la envía al cliente, añadiendo CR+LF.
//
// Tabla de conversión para caracteres españoles (UTF-8 → WPC1252):
//   U+00C0-U+00FF (prefijo 0xC3 en UTF-8): valor WPC1252 = segundo_byte + 0x40
//   U+0080-U+00BF (prefijo 0xC2 en UTF-8): valor WPC1252 = segundo_byte (igual)
//
// Si los acentos siguen mal: cambia 0x10 en ESCPOS_CODEPAGE[2] por el código
// de página de tu impresora (0x02=CP850, 0x00=CP437) o activa ESCPOS_STRIP_ACCENTS.
static void _writeLine(WiFiClient& c, const String& str) {
#ifdef ESCPOS_STRIP_ACCENTS
  String s = str;
  _stripAccents(s);
  c.write(reinterpret_cast<const uint8_t*>(s.c_str()), s.length());
#else
  const uint8_t* p = reinterpret_cast<const uint8_t*>(str.c_str());
  const size_t   n = str.length();
  for (size_t i = 0; i < n; ) {
    uint8_t b = p[i];
    if (b < 0x80) {
      c.write(b);
      i++;
    } else if (b == 0xC2 && i + 1 < n) {
      c.write(p[i + 1]);                              // U+0080-U+00BF → byte bajo
      i += 2;
    } else if (b == 0xC3 && i + 1 < n) {
      c.write(static_cast<uint8_t>(p[i + 1] + 0x40)); // U+00C0-U+00FF
      i += 2;
    } else {
      c.write(static_cast<uint8_t>('?'));              // multibyte fuera de rango
      i++;
    }
  }
#endif
  c.write(static_cast<uint8_t>('\r'));
  c.write(static_cast<uint8_t>('\n'));
}

// ─── printLabel ──────────────────────────────────────────────────────────────
// Genera y envía el ticket ESC/POS completo al WiFiClient abierto.
// Repite el ticket job.copies veces.
bool printLabel(WiFiClient& client, const PrintJob& job) {

  // Comandos ESC/POS (arrays locales pequeños, sin necesidad de PROGMEM en ESP32)
  const uint8_t init[]       = {0x1B, 0x40};             // ESC @  — inicializar
  // Code page: 0x10=WPC1252. Cambiar aquí si los acentos salen mal.
  const uint8_t codepage[]   = {0x1B, 0x74, 0x10};       // ESC t  — WPC1252
  const uint8_t alignLeft[]  = {0x1B, 0x61, 0x00};       // ESC a 0 — izquierda
  const uint8_t alignCenter[]= {0x1B, 0x61, 0x01};       // ESC a 1 — centro
  const uint8_t sizeNormal[] = {0x1B, 0x21, 0x00};       // ESC !  — tamaño normal
  const uint8_t sizeDouble[] = {0x1B, 0x21, 0x30};       // ESC !  — doble alto+ancho
  const uint8_t boldOn[]     = {0x1B, 0x45, 0x01};       // ESC E  — negrita on
  const uint8_t boldOff[]    = {0x1B, 0x45, 0x00};       // ESC E  — negrita off
  const uint8_t feed[]       = {0x1B, 0x64, 0x04};       // ESC d  — avanzar 4 líneas
#ifndef ESCPOS_NO_CUT
  const uint8_t cut[]        = {0x1D, 0x56, 0x01};       // GS V 1 — corte parcial
#endif

  for (uint8_t copy = 0; copy < job.copies; copy++) {

    // Inicializar + código de página
    _cmd(client, init,     sizeof(init));
    _cmd(client, codepage, sizeof(codepage));

    if (job.testMode) {
      // ── Ticket de prueba ─────────────────────────────────────────────────
      _cmd(client, alignCenter, sizeof(alignCenter));
      _cmd(client, boldOn,      sizeof(boldOn));
      _writeLine(client, "-- PRUEBA DE IMPRESION --");
      _cmd(client, boldOff, sizeof(boldOff));
      _writeLine(client, "ESP32 Relay Etiquetas v1.0");
      _writeLine(client, "Impresora OK");
      _writeLine(client, "");

    } else {
      // ── Nombre del producto — centrado, doble alto+ancho ──────────────────
      _cmd(client, alignCenter, sizeof(alignCenter));
      _cmd(client, sizeDouble,  sizeof(sizeDouble));
      _writeLine(client, job.producto);

      // ── Fechas de realización y envasado — izquierda, tamaño normal ───────
      _cmd(client, sizeNormal, sizeof(sizeNormal));
      _cmd(client, alignLeft,  sizeof(alignLeft));
      _writeLine(client, "Realizacion: " + job.realizacion);
      _writeLine(client, "Envasado:    " + job.envasado);

      // ── Fecha de caducidad — centrado, doble alto+ancho ───────────────────
      _cmd(client, alignCenter, sizeof(alignCenter));
      _cmd(client, sizeDouble,  sizeof(sizeDouble));
      _writeLine(client, "CAD: " + job.caducidad);

      // ── Fecha de congelado (opcional) — izquierda, tamaño normal ──────────
      if (job.congelado.length() > 0) {
        _cmd(client, sizeNormal, sizeof(sizeNormal));
        _cmd(client, alignLeft,  sizeof(alignLeft));
        _writeLine(client, "Congelado:   " + job.congelado);
      }

      _cmd(client, sizeNormal, sizeof(sizeNormal));
      _writeLine(client, "");
    }

    // Avance + corte
    _cmd(client, feed, sizeof(feed));
#ifndef ESCPOS_NO_CUT
    _cmd(client, cut, sizeof(cut));
#endif

  }  // fin bucle copias

  client.flush();
  return true;
}
