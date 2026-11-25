// Banco de pruebas en PC: ejecuta un sketch (original o corregido) contra un
// modelo del circuito de entrada y reporta lo que mostraria el Arduino.
//
// Compilar (ver ejecutar_pruebas.sh):
//   g++ -O2 -std=c++17 -Imock -DSKETCH='"ruta.ino"' simulador.cpp -o sim
//
// Uso: ./sim hw=0 forma=sen vp=10 f=50 vf=0.75 t=6 [vp2=5 t2=3] [ruido=0]
//            [traza=1] [von=1.5] [seed=1]
//   hw=0  circuito actual: puente (2 diodos de vf cada uno) + C1 470uF +
//         divisor 10k/10k -> A0
//   hw=1  acondicionador de la Opcion B: A0 = 2.5 V + 0.2326 * v(t)
//   forma sen | cuad | tri ; vp = amplitud pico ; f = Hz
//   von   tension de entrada a partir de la cual el 4N25 lleva D2 a LOW
//   ruido ruido gaussiano del ADC en LSB rms (0 = Proteus ideal)
//   rs    resistencia de salida del generador en ohm (0 = fuente ideal, como
//         VSINE de Proteus; 50 = generador de laboratorio). Con rs > 0 se
//         modela la carga del circuito: rama R3 + LED/D1 y recarga de C1.
//   vo    offset DC de la fuente (V)
//   ppc   flancos FALLING por ciclo en D2: 2 (verificado en Proteus) o 1
#include "Arduino.h"
#include <random>
#include <map>

// ---------------- estado de la simulacion ----------------
static double t_us = 0;
static std::map<std::string, std::string> P;
static double par(const char *k, double def) {
  auto it = P.find(k);
  return it == P.end() ? def : atof(it->second.c_str());
}
static std::string pars(const char *k, const char *def) {
  auto it = P.find(k);
  return it == P.end() ? def : it->second;
}

static int HW; static std::string FORMA;
static double VP, VP2, T2, FREQ, VF, VON, RUIDO, RS, VO;
static int PPC = 2;
static double vBornesUlt = 0, vBornesMax = -1e9, sumaV2 = 0, tV2 = 0;
static double vC1 = 0;                       // tension en C1 (hw=0)
static const double TAU = 20000.0 * 470e-6;  // descarga por R1 + R2
static bool ledPrev = false;
static void (*isrFn)() = nullptr;
static bool intOn = true, enIsr = false, pendiente = false;
static std::mt19937 rng;
static std::normal_distribution<double> gauss(0.0, 1.0);

static double vin(double tu) {
  double amp = (VP2 >= 0 && tu >= T2 * 1e6) ? VP2 : VP;
  double fase = 2 * M_PI * FREQ * tu * 1e-6;
  double s = sin(fase);
  if (FORMA == "cuad") return VO + amp * (s >= 0 ? 1 : -1);
  if (FORMA == "tri") return VO + amp * (2 / M_PI) * asin(s);
  return VO + amp * s;
}

static void dispararIsr() {
  if (!isrFn) return;
  if (!intOn || enIsr) { pendiente = true; return; }
  enIsr = true; isrFn(); enIsr = false;
}

void simAvanzar(double us) {
  double objetivo = t_us + us;
  while (t_us < objetivo) {
    double h = std::min(2.0, objetivo - t_us);
    t_us += h;
    double vs = vin(t_us);                   // FEM del generador
    double v = vs;                           // tension en bornes del circuito
    if (RS > 0) {
      // Rama R3 (1.8k) + LED (~1.2 V) / D1 en antiparalelo (~0.7 V)
      double vt = vs;
      if (vs > 1.2) vt = (vs * 1800 + RS * 1.2) / (1800 + RS);
      else if (vs < -0.7) vt = (vs * 1800 - RS * 0.7) / (1800 + RS);
      v = vt;
      if (HW == 0 && fabs(vt) - 2 * VF > vC1) {   // el puente recarga C1
        double iOpto = fabs(vt) > 1.2 ? (fabs(vt) - (vt > 0 ? 1.2 : 0.7)) / 1800 : 0;
        double iPuente = (fabs(vs) - vC1 - 2 * VF) / RS - iOpto;
        if (iPuente > 0) {
          vC1 += iPuente * h * 1e-6 / 470e-6;
          v = (vs > 0 ? 1 : -1) * (vC1 + 2 * VF);
        }
      }
      if (HW == 0) vC1 -= vC1 / 20000.0 * h * 1e-6 / 470e-6;   // divisor
    } else if (HW == 0) {
      double vrect = fabs(v) - 2 * VF;       // puente: conducen 2 diodos
      if (vrect > vC1) vC1 = vrect;          // carga (fuente ideal, Proteus)
      else vC1 *= exp(-h * 1e-6 / TAU);      // descarga por el divisor
    }
    vBornesUlt = v;
    if (t_us > (par("t", 6) - 1) * 1e6) {   // ultimo segundo: pico y RMS en bornes
      if (v > vBornesMax) vBornesMax = v;
      sumaV2 += v * v * h; tV2 += h;
    }
    // ppc=2 (por defecto, comportamiento verificado en Proteus): un pulso por
    // semiciclo. ppc=1: el LED solo conduce en el semiciclo positivo.
    bool led = (PPC == 2) ? fabs(v) > VON : v > VON;
    bool flanco = led && !ledPrev;           // D2 pasa a LOW: flanco FALLING
    ledPrev = led;                           // antes de la ISR: micros() dentro
    if (flanco) dispararIsr();               // de la ISR re-entra aqui
  }
}

unsigned long micros() { simAvanzar(4); return ((unsigned long)(t_us / 4)) * 4; }
unsigned long millis() { simAvanzar(4); return (unsigned long)(t_us / 1000); }
void delay(unsigned long ms) { simAvanzar(ms * 1000.0); }
void attachInterrupt(uint8_t, void (*isr)(), int) { isrFn = isr; }
void noInterrupts() { intOn = false; }
void interrupts() {
  intOn = true;
  if (pendiente && !enIsr) { pendiente = false; dispararIsr(); }
}
int analogRead(uint8_t) {
  double va0 = (HW == 0) ? vC1 * 10000.0 / 20000.0 : 2.5 + 0.2326 * (RS > 0 ? vBornesUlt : vin(t_us));
  double cuentas = va0 * 1024.0 / 5.0 + RUIDO * gauss(rng);
  int x = (int)floor(cuentas);
  if (x < 0) x = 0;
  if (x > 1023) x = 1023;
  simAvanzar(112);                           // 13 ciclos ADC a 125 kHz + bucle
  return x;
}
char *dtostrf(double val, signed char w, unsigned char p, char *buf) {
  sprintf(buf, "%*.*f", w, p, val);
  return buf;
}
SerialSim Serial;

// ---------------- sketch bajo prueba ----------------
#include SKETCH

int main(int argc, char **argv) {
  for (int i = 1; i < argc; i++) {
    std::string a = argv[i];
    auto p = a.find('=');
    if (p != std::string::npos) P[a.substr(0, p)] = a.substr(p + 1);
  }
  HW = (int)par("hw", 0); FORMA = pars("forma", "sen");
  VP = par("vp", 10); VP2 = par("vp2", -1); T2 = par("t2", 1e9);
  FREQ = par("f", 50); VF = par("vf", 0.75); VON = par("von", 1.5);
  RUIDO = par("ruido", 0); RS = par("rs", 0); PPC = (int)par("ppc", 2); VO = par("vo", 0);
  rng.seed((unsigned)par("seed", 1));
  double duracion = par("t", 6);
  bool traza = par("traza", 0) != 0;

  setup();
  std::string ultima;
  while (t_us < duracion * 1e6) {
    loop();
    if (Serial.ultimaLinea != ultima) {
      ultima = Serial.ultimaLinea;
      if (traza) printf("t=%7.3f s | %s", t_us / 1e6, ultima.c_str());
    }
  }
  printf("RESULTADO vrms=%.4f f=%.4f vpico=%.4f lcd0=[%s] lcd1=[%s] "
         "vc1_real=%.4f vp_bornes=%.4f vrms_bornes=%.4f\n",
         voltajeRMS, frecuencia, voltajePico, lcd.pantalla[0], lcd.pantalla[1],
         vC1, vBornesMax, tV2 > 0 ? sqrt(sumaV2 / tV2) : 0.0);
  return 0;
}
