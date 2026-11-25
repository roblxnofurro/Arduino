#pragma once
#include "Arduino.h"

class LiquidCrystal {
 public:
  char pantalla[2][17];
  int fila = 0, col = 0;
  LiquidCrystal(int, int, int, int, int, int) { clear(); }
  void begin(int, int) {}
  void clear() {
    for (auto &f : pantalla) { memset(f, ' ', 16); f[16] = 0; }
    fila = col = 0;
  }
  void setCursor(int c, int f) { col = c; fila = f; }
  void escribir(const std::string &s) {
    for (char ch : s) {
      if (col < 16) pantalla[fila][col] = ch;   // lo que pasa de 16 no se ve
      col++;
      simAvanzar(45);
    }
  }
  void print(const char *s) { escribir(s); }
  void print(double v, int d = 2) {
    char b[48]; snprintf(b, sizeof b, "%.*f", d, v); escribir(b);
  }
};
