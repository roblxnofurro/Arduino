// Arduino simulado en PC para validar la logica de los sketches.
// NO es un emulador del AVR: modela el circuito de entrada, el ADC de 10 bits,
// micros() con resolucion de 4 us y la interrupcion INT0.
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <cstdlib>
#include <string>

#define A0 14
#define INPUT 0
#define INPUT_PULLUP 2
#define FALLING 2
#define F(x) (x)

unsigned long millis();
unsigned long micros();
void delay(unsigned long ms);
int analogRead(uint8_t pin);
inline void pinMode(uint8_t, uint8_t) {}
void attachInterrupt(uint8_t, void (*isr)(), int);
inline uint8_t digitalPinToInterrupt(uint8_t p) { return p == 2 ? 0 : 1; }
void noInterrupts();
void interrupts();
char *dtostrf(double val, signed char width, unsigned char prec, char *buf);
template <class A, class B> auto max(A a, B b) -> decltype(a + b) { return a > b ? a : b; }

void simAvanzar(double us);   // avanza el tiempo simulado (carga de CPU)

class SerialSim {
 public:
  std::string linea, ultimaLinea;
  bool eco = false;
  void begin(long) {}
  void escribir(const std::string &s) {
    linea += s;
    simAvanzar(s.size() * 1041.7);   // 9600 baudios ~ 1.04 ms por caracter
  }
  void print(const char *s) { escribir(s); }
  void print(int v) { escribir(std::to_string(v)); }
  void print(unsigned int v) { escribir(std::to_string(v)); }
  void print(long v) { escribir(std::to_string(v)); }
  void print(unsigned long v) { escribir(std::to_string(v)); }
  void print(double v, int d = 2) {
    char b[48]; snprintf(b, sizeof b, "%.*f", d, v); escribir(b);
  }
  void println() {
    escribir("\r\n");
    ultimaLinea = linea;
    if (eco) fputs(linea.c_str(), stdout);
    linea.clear();
  }
  void println(const char *s) { print(s); println(); }
};
extern SerialSim Serial;
