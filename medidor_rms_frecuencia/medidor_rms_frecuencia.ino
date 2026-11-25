/*
  ============================================================================
  MEDIDOR DE VOLTAJE RMS Y FRECUENCIA - Arduino Uno (ATmega328P)
  Version corregida tras auditoria. Ver AUDITORIA.md en la raiz del proyecto.
  ============================================================================

  HARDWARE ACTUAL (segun el esquematico de Proteus):
    - V1 (AC) -> puente BR1 -> C1 = 470 uF en paralelo con el divisor
      R1 = 10k (arriba) / R2 = 10k (abajo) -> A0.
      El negativo del puente debe ser el GND del Arduino.
    - V1 -> R3 = 1.8k -> LED del 4N25 (pines 1-2), con D1 (1N4007) en
      antiparalelo. Colector (pin 5) -> D2 con R4 = 10k a 5V. Emisor (pin 4) a GND.
    - LCD 16x2 en modo 4 bits: RS=7, E=6, D4=5, D5=4, D6=3, D7=8.

  QUE MIDE REALMENTE ESTE HARDWARE (importante):
    C1 convierte el puente en un DETECTOR DE PICO. En A0 hay una tension
    CONTINUA (casi sin rizado) proporcional al pico de la entrada, no la forma
    de onda. Por eso, con este hardware:
      * Se mide el VOLTAJE PICO de la entrada.
      * El Vrms se obtiene como Vpico / FACTOR_CRESTA. Esto SOLO es exacto si
        se conoce la forma de onda (senoidal: sqrt(2)). No es RMS verdadero.
      * Calcular sqrt(sum(Vi^2)/N) sobre A0 NO da el RMS de la entrada: daria
        el valor de la tension continua del condensador.
    Para medir RMS verdadero hay que modificar el hardware (ver
    MODO_RMS_VERDADERO y AUDITORIA.md). NO se hace ningun cambio de
    hardware sin autorizacion: el modo por defecto es el del circuito actual.

  SENAL DE REFERENCIA: generador de funciones, senoidal, 60 Hz, 20 Vpp,
    offset 0 V -> Vp = 10 V, Vrms = 7.071 V, T = 16.667 ms (EN VACIO).
    El generador tiene 50 ohm de salida y este circuito lo carga: el valor de
    referencia para comparar es la tension MEDIDA EN LOS BORNES del circuito
    (osciloscopio), no la que indica la pantalla del generador.

  ESTADO: CONSTANTES FISICAS PENDIENTES DE MEDIR.
    VREF_ADC, R1_OHM, R2_OHM y CAIDA_PUENTE_V se obtienen MIDIENDO el circuito
    (procedimiento en AUDITORIA.md, seccion 10.4). No se ajustan para forzar
    7.07 V. CAIDA_PUENTE_V = 1.50 V es el valor heredado del original y NO
    esta verificado.
  ============================================================================
*/

#include <LiquidCrystal.h>
#include <math.h>
#include <string.h>

// ----------------------------------------------------------------------------
// MODO DE MEDICION DE VOLTAJE
//   0 = hardware actual (puente + C1 + divisor). Detector de pico.
//   1 = RMS verdadero. REQUIERE MODIFICAR EL HARDWARE: A0 debe recibir la
//       forma de onda AC atenuada y desplazada a ~2.5 V (sin puente ni C1 en
//       ese camino). Ver AUDITORIA.md, seccion "Opcion B".
// ----------------------------------------------------------------------------
#ifndef MODO_RMS_VERDADERO
#define MODO_RMS_VERDADERO 0
#endif

// ----------------------------------------------------------------------------
// CONEXIONES (sin cambios respecto al original)
// ----------------------------------------------------------------------------
LiquidCrystal lcd(7, 6, 5, 4, 3, 8);   // RS, E, D4, D5, D6, D7
const uint8_t PIN_SENSOR = A0;          // salida del divisor de voltaje
const uint8_t PIN_OPTO   = 2;           // colector del 4N25 (INT0)

// ----------------------------------------------------------------------------
// ADC
// ----------------------------------------------------------------------------
// Tension REAL del pin 5V del Arduino. Por USB suele estar entre 4.6 y 5.2 V.
// Medirla con multimetro y escribirla aqui. Es la principal fuente de error
// de ganancia en el montaje fisico. En Proteus es exactamente 5.00 V.
const float VREF_ADC = 5.00;
// ATmega328P (datasheet, seccion ADC): ADC = Vin * 1024 / Vref, por lo que
// Vin = ADC * Vref / 1024. (El original usaba 1023: error de 0.1 %.)
const float CUENTAS_ADC = 1024.0;
const int   ADC_SATURACION = 1020;      // >= este valor se considera saturado
const float ADC_UMBRAL_SIN_SENAL = 3.0; // promedio < 3 cuentas: no hay senal

// ----------------------------------------------------------------------------
// DIVISOR Y CONDENSADOR (MODO 0). Escribir los valores REALES medidos.
// ----------------------------------------------------------------------------
const float R1_OHM = 10000.0;           // del + del puente a A0
const float R2_OHM = 10000.0;           // de A0 a GND
const float C1_FARADIOS = 470e-6;
// Ganancia del divisor: V_C1 = V_A0 * (R1 + R2) / R2
const float GANANCIA_DIVISOR = (R1_OHM + R2_OHM) / R2_OHM;
// Constante de tiempo de DESCARGA de C1 a traves del divisor.
// 20k * 470uF = 9.4 s: la lectura SUBE rapido pero BAJA muy lento.
const float TAU_DESCARGA_S = (R1_OHM + R2_OHM) * C1_FARADIOS;

// ----------------------------------------------------------------------------
// CAIDA DEL PUENTE (MODO 0)
//   Vpico_entrada = CAL_GANANCIA * Vpico_C1 + CAIDA_PUENTE_V
//   Es la caida de los 2 diodos que conducen EN EL INSTANTE DEL PICO, cuando
//   el puente recarga C1. Depende del modelo de puente, de la corriente de
//   recarga y de la temperatura. NO se supone: se MIDE asi:
//     CAIDA_PUENTE_V = Vp en los bornes de entrada (osciloscopio)
//                    - V_C1 (voltimetro DC sobre C1)
//   El Serial imprime "VC1" para comprobar tambien VREF_ADC y el divisor.
// ----------------------------------------------------------------------------
const float CAIDA_PUENTE_V = 1.50;      // PENDIENTE DE MEDIR (valor del original)

// Correccion de ganancia residual. Dejar en 1.000. Solo se cambia si, con
// VREF_ADC y R1/R2 ya medidos, la verificacion en varios puntos demuestra un
// error de ganancia sistematico (procedimiento en AUDITORIA.md).
const float CAL_GANANCIA = 1.000;

// Factor de cresta = Vpico / Vrms de la forma de onda de ENTRADA.
//   Senoidal: 1.41421356   Cuadrada simetrica: 1.0   Triangular: 1.7320508
// Solo se usa en MODO 0, donde no se puede medir la forma de onda.
const float FACTOR_CRESTA = 1.41421356;

// ----------------------------------------------------------------------------
// MODO 1 (RMS VERDADERO) - solo aplica con el hardware modificado
// ----------------------------------------------------------------------------
// Atenuacion del circuito de acondicionamiento: V_A0(AC) / V_entrada(AC).
// Ejemplo de la Opcion B en AUDITORIA.md: Ra = 33k, Rb = Rc = 20k
//   -> (Rb||Rc) / (Ra + Rb||Rc) = 10k / 43k = 0.2326
const float ATENUACION_AC = 0.2326;
const float CAL_GANANCIA_RMS = 1.000;   // PROVISIONAL
const unsigned int MAX_MUESTRAS_RMS = 16000; // evita desbordar sumaD2 (uint32)

// ----------------------------------------------------------------------------
// FRECUENCIA
// ----------------------------------------------------------------------------
// Flancos de bajada en D2 por cada ciclo de la senal de entrada.
// En el esquematico el LED del 4N25 se alimenta con AC a traves de R3 y D1
// esta en ANTIPARALELO: el LED solo conduce en el semiciclo POSITIVO.
//   -> D2 esta en LOW durante el semiciclo positivo y en HIGH en el negativo
//   -> 1 flanco de bajada por ciclo.
// Por eso NO se divide entre 2. VERIFICARLO con el osciloscopio (canal en D2)
// o con el "Periodo" que se imprime por Serial (debe ser 1/f, no 1/(2f)).
const uint8_t PULSOS_POR_CICLO = 1;
// Rechazo de glitches: se ignoran flancos a menos de 500 us del anterior
// valido. Limita la frecuencia maxima medible a 1/500us = 2000 Hz.
const unsigned long PERIODO_MINIMO_US = 500UL;
// Sin flancos durante este tiempo -> frecuencia = 0 (sin senal).
// Define la frecuencia minima medible: 1 / 2 s = 0.5 Hz.
const unsigned long TIMEOUT_SIN_PULSOS_US = 2000000UL;
// Correccion del reloj. El Uno usa un resonador ceramico de 16 MHz
// (tolerancia tipica +/-0.5 %). Calibrar con un generador de frecuencia
// conocida: CAL_FRECUENCIA = f_real / f_medida. En Proteus: 1.0000.
const float CAL_FRECUENCIA = 1.0000;

// ----------------------------------------------------------------------------
// TIEMPOS
// ----------------------------------------------------------------------------
const unsigned long VENTANA_ADC_MS = 100UL;   // muestreo de A0 por medicion
const unsigned long VENTANA_MAX_US = 1000000UL;
const unsigned long INTERVALO_ACTUALIZACION_MS = 500UL;

// ----------------------------------------------------------------------------
// VARIABLES COMPARTIDAS CON LA INTERRUPCION (volatile, leidas con
// interrupciones deshabilitadas porque son de 16/32 bits y el AVR es de 8)
// ----------------------------------------------------------------------------
volatile bool          isrHayReferencia = false; // hay un flanco previo valido
volatile unsigned long isrUltimoFlanco  = 0;     // micros() del ultimo flanco
volatile unsigned long isrSumaPeriodos  = 0;     // suma de periodos (us)
volatile unsigned int  isrNumPeriodos   = 0;     // periodos sumados
volatile unsigned long isrUltimoPeriodo = 0;     // ultimo periodo valido (us)

// ----------------------------------------------------------------------------
// RESULTADOS
// ----------------------------------------------------------------------------
float adcMedio = 0;          // promedio de las muestras de A0 (cuentas)
int   adcMaximo = 0;
int   adcMinimo = 0;
unsigned int muestrasADC = 0;
float voltajeA0 = 0;         // tension promedio en el pin A0 (V)
float voltajeC1 = 0;         // tension continua en C1 reconstruida (V), modo 0
float voltajePico = 0;       // pico de la senal de entrada (V)
float voltajeRMS = 0;        // RMS de la senal de entrada (V)
bool  saturado = false;      // A0 fuera del rango 0..VREF
bool  sinSenalAC = true;

float frecuencia = 0;        // Hz de la senal de entrada
float periodoMedioUs = 0;    // periodo de la senal (us)
unsigned int periodosEnVentana = 0;

unsigned long ultimaActualizacionMs = 0;

// ============================================================================
// INTERRUPCION: flanco de bajada en D2 (inicio del semiciclo positivo)
// Solo guarda tiempos; todos los calculos se hacen fuera de la ISR.
// ============================================================================
void isrOptoacoplador() {
  unsigned long ahora = micros();

  if (!isrHayReferencia) {          // primer flanco: solo sirve de referencia
    isrUltimoFlanco = ahora;
    isrHayReferencia = true;
    return;
  }

  unsigned long periodo = ahora - isrUltimoFlanco; // correcto aunque micros()
                                                   // desborde (aritmetica
                                                   // sin signo)
  if (periodo < PERIODO_MINIMO_US) {
    return;                         // glitch: se ignora y NO se mueve la
  }                                 // referencia

  isrSumaPeriodos += periodo;
  isrNumPeriodos++;
  isrUltimoPeriodo = periodo;
  isrUltimoFlanco = ahora;
}

// ============================================================================
// FRECUENCIA: promedio de los periodos medidos desde la ultima actualizacion.
//   periodoEventos = suma(periodos) / N            [us]
//   f = 1e6 / (periodoEventos * PULSOS_POR_CICLO)  [Hz]
// Equivale a N periodos completos entre el primer y el ultimo flanco de la
// ventana: el error por la resolucion de micros() (4 us) no se acumula.
// ============================================================================
void actualizarFrecuencia() {
  unsigned long suma, ultimoFlanco, ultimoPeriodo;
  unsigned int numero;
  bool hayReferencia;

  noInterrupts();                   // copia atomica de las variables de la ISR
  suma          = isrSumaPeriodos;
  numero        = isrNumPeriodos;
  ultimoFlanco  = isrUltimoFlanco;
  ultimoPeriodo = isrUltimoPeriodo;
  hayReferencia = isrHayReferencia;
  isrSumaPeriodos = 0;              // nueva ventana
  isrNumPeriodos  = 0;
  interrupts();

  periodosEnVentana = numero;
  unsigned long ahora = micros();   // leido DESPUES de la copia: ahora >= ultimoFlanco

  // Sin flancos recientes: no hay senal (o es menor que 0.5 Hz)
  if (!hayReferencia || (ahora - ultimoFlanco) > TIMEOUT_SIN_PULSOS_US) {
    frecuencia = 0;
    periodoMedioUs = 0;
    noInterrupts();
    if (isrUltimoFlanco == ultimoFlanco) {   // solo si no llego un flanco nuevo
      isrHayReferencia = false;
      isrUltimoPeriodo = 0;
    }
    interrupts();
    return;
  }

  float periodoEventosUs;
  if (numero > 0) {
    periodoEventosUs = (float)suma / (float)numero;
  } else if (ultimoPeriodo > 0) {
    // Senal mas lenta que la ventana (< ~2 Hz): usar el ultimo periodo
    // completo medido en lugar de congelar o mostrar 0.
    periodoEventosUs = (float)ultimoPeriodo;
  } else {
    frecuencia = 0;                 // solo hay 1 flanco: aun no hay periodo
    periodoMedioUs = 0;
    return;
  }

  periodoMedioUs = periodoEventosUs * PULSOS_POR_CICLO;
  frecuencia = CAL_FRECUENCIA * 1000000.0 / periodoMedioUs;
}

// ============================================================================
// VOLTAJE - MODO 0 (hardware actual: detector de pico con C1)
// ============================================================================
void medirVoltajePico(float frecuenciaHz) {
  uint32_t suma = 0;                // 1023 * ~900 muestras: no desborda
  unsigned int n = 0;
  int maximo = 0, minimo = 1023;

  // Promedio de ~900 muestras en 100 ms. Como A0 es continua, el PROMEDIO
  // es mejor estimador que el MAXIMO: el maximo se queda con el ruido
  // positivo (sesgo hacia arriba) y con un solo valor; el promedio reduce el
  // ruido aleatorio en ~sqrt(N).
  unsigned long inicio = millis();
  while (millis() - inicio < VENTANA_ADC_MS) {
    int x = analogRead(PIN_SENSOR);
    suma += x;
    n++;
    if (x > maximo) maximo = x;
    if (x < minimo) minimo = x;
  }

  muestrasADC = n;
  adcMaximo = maximo;
  adcMinimo = minimo;
  adcMedio = (float)suma / (float)n;
  saturado = (maximo >= ADC_SATURACION);
  sinSenalAC = (adcMedio < ADC_UMBRAL_SIN_SENAL);

  voltajeA0 = adcMedio * VREF_ADC / CUENTAS_ADC;
  float vC1Medio = voltajeA0 * GANANCIA_DIVISOR;
  voltajeC1 = vC1Medio;             // comparar con un voltimetro DC sobre C1

  // Correccion del rizado: con rectificacion de onda completa C1 se descarga
  // durante ~1/(2f) entre recargas. Rizado: dV = Vp / (2 f tau).
  // El promedio del diente de sierra es Vp - dV/2 = Vp * (1 - 1/(4 f tau)).
  // Con el hardware actual (tau = 9.4 s, 50 Hz) la correccion es 0.05 %.
  float vC1Pico = vC1Medio;
  if (frecuenciaHz > 0) {
    float k = 1.0 / (4.0 * frecuenciaHz * TAU_DESCARGA_S);
    if (k < 0.5) vC1Pico = vC1Medio / (1.0 - k);
  }

  if (sinSenalAC) {
    voltajePico = 0;                // por debajo de ~2 caidas de diodo el
    voltajeRMS = 0;                 // puente no conduce: no hay medicion
  } else {
    voltajePico = CAL_GANANCIA * vC1Pico + CAIDA_PUENTE_V;
    voltajeRMS = voltajePico / FACTOR_CRESTA;  // supone forma de onda conocida
  }
}

// ============================================================================
// VOLTAJE - MODO 1 (RMS verdadero, requiere hardware modificado)
//   Vrms = sqrt( (1/N) * sum( (Vi - Vmedio)^2 ) )
// Se resta el promedio para eliminar el offset de ~2.5 V del acondicionador.
// La ventana dura un numero ENTERO de periodos (usando la frecuencia medida
// por el optoacoplador) para que no queden fracciones de ciclo que sesguen
// el resultado.
// ============================================================================
void medirVoltajeRMSVerdadero(float frecuenciaHz) {
  unsigned long ventanaUs = VENTANA_ADC_MS * 1000UL;
  if (frecuenciaHz > 0) {
    float periodoUs = 1000000.0 / frecuenciaHz;
    unsigned long ciclos = (unsigned long)((float)ventanaUs / periodoUs + 0.5);
    if (ciclos < 1) ciclos = 1;
    while (ciclos > 1 && ciclos * periodoUs > VENTANA_MAX_US) ciclos--;
    float ventana = ciclos * periodoUs;
    ventanaUs = (ventana > VENTANA_MAX_US) ? VENTANA_MAX_US : (unsigned long)ventana;
  }

  // Se trabaja con d = x - 512 para que las sumas sean pequenas:
  // |d| <= 512 -> d^2 <= 262144; 16000 muestras -> 4.19e9 < 2^32.
  int32_t sumaD = 0;
  uint32_t sumaD2 = 0;
  unsigned int n = 0;
  int maximo = 0, minimo = 1023;

  unsigned long inicio = micros();
  while ((micros() - inicio) < ventanaUs && n < MAX_MUESTRAS_RMS) {
    int x = analogRead(PIN_SENSOR);
    int32_t d = x - 512;
    sumaD += d;
    sumaD2 += (uint32_t)(d * d);
    n++;
    if (x > maximo) maximo = x;
    if (x < minimo) minimo = x;
  }

  muestrasADC = n;
  adcMaximo = maximo;
  adcMinimo = minimo;
  float mediaD = (float)sumaD / (float)n;
  float varianza = (float)sumaD2 / (float)n - mediaD * mediaD; // cuentas^2
  if (varianza < 0) varianza = 0;

  adcMedio = mediaD + 512.0;                      // nivel DC (offset)
  voltajeA0 = adcMedio * VREF_ADC / CUENTAS_ADC;
  saturado = (maximo >= ADC_SATURACION) || (minimo <= (1023 - ADC_SATURACION));

  float vrmsA0 = sqrt(varianza) * VREF_ADC / CUENTAS_ADC;
  voltajeRMS = CAL_GANANCIA_RMS * vrmsA0 / ATENUACION_AC;

  float desvioMax = max((float)maximo - adcMedio, adcMedio - (float)minimo);
  voltajePico = CAL_GANANCIA_RMS * desvioMax * VREF_ADC / CUENTAS_ADC / ATENUACION_AC;
  sinSenalAC = (desvioMax < ADC_UMBRAL_SIN_SENAL);
  if (sinSenalAC) {
    voltajeRMS = 0;
    voltajePico = 0;
  }
}

// ============================================================================
// PRESENTACION
// ============================================================================
// Escribe una fila completa de 16 caracteres (rellena con espacios) para no
// dejar restos de textos anteriores y sin usar lcd.clear() (evita parpadeo).
void escribirFilaLCD(uint8_t fila, const char *texto) {
  char linea[17];
  size_t largo = strlen(texto);
  if (largo > 16) largo = 16;
  memcpy(linea, texto, largo);
  for (size_t i = largo; i < 16; i++) linea[i] = ' ';
  linea[16] = '\0';
  lcd.setCursor(0, fila);
  lcd.print(linea);
}

void mostrarLCD() {
  char numero[12];
  char texto[24];

  if (saturado) {
    escribirFilaLCD(0, "Vrms: SATURADO");
  } else {
    dtostrf(voltajeRMS, 6, 2, numero);          // "  7.07"
    snprintf(texto, sizeof(texto), "Vrms:%s V", numero);
    escribirFilaLCD(0, texto);
  }

  if (frecuencia <= 0) {
    escribirFilaLCD(1, "Frec:    --- Hz");
  } else {
    dtostrf(frecuencia, 7, 2, numero);          // "  50.00"
    snprintf(texto, sizeof(texto), "Frec:%s Hz", numero);
    escribirFilaLCD(1, texto);
  }
}

void enviarSerial() {
  // Se conservan los campos del original (ADC, Voltaje ADC, Voltaje RMS,
  // Frecuencia) y se anaden datos para verificar y calibrar.
  Serial.print("ADC: ");
  Serial.print(adcMedio, 1);
  Serial.print(" | Voltaje ADC: ");
  Serial.print(voltajeA0, 3);
#if !MODO_RMS_VERDADERO
  Serial.print(" V | VC1: ");
  Serial.print(voltajeC1, 3);
#endif
  Serial.print(" V | Vpico: ");
  Serial.print(voltajePico, 3);
  Serial.print(" V | Voltaje RMS: ");
  Serial.print(voltajeRMS, 3);
  Serial.print(" V | Frecuencia: ");
  Serial.print(frecuencia, 2);
  Serial.print(" Hz | Periodo: ");
  Serial.print(periodoMedioUs, 0);
  Serial.print(" us | N: ");
  Serial.print(periodosEnVentana);
  if (saturado) Serial.print(" | SATURADO");
  Serial.println();
}

// ============================================================================
void setup() {
  Serial.begin(9600);               // igual que el original (Virtual Terminal)

  pinMode(PIN_SENSOR, INPUT);
  pinMode(PIN_OPTO, INPUT_PULLUP);  // en paralelo con R4 = 10k externa

  lcd.begin(16, 2);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Detector RMS");
  lcd.setCursor(0, 1);
  lcd.print("Iniciando...");

  // FALLING = el transistor del 4N25 empieza a conducir = comienza el
  // semiciclo positivo (con un pequeno retardo que depende de la amplitud;
  // el retardo es igual en todos los ciclos, asi que no afecta al periodo).
  attachInterrupt(digitalPinToInterrupt(PIN_OPTO), isrOptoacoplador, FALLING);

  Serial.println(F("Medidor Vrms / frecuencia"));
#if MODO_RMS_VERDADERO
  Serial.println(F("Modo: RMS verdadero (requiere hardware modificado)"));
#else
  Serial.println(F("Modo: detector de pico (Vrms = Vpico / factor de cresta)"));
#endif
  Serial.println(F("AVISO: constantes de calibracion PROVISIONALES"));

  delay(2000);
  lcd.clear();

  noInterrupts();                   // descartar lo acumulado durante el inicio
  isrSumaPeriodos = 0;
  isrNumPeriodos = 0;
  interrupts();
  ultimaActualizacionMs = millis();
}

void loop() {
  // Planificacion con millis() en lugar de delay(): la cadencia es fija
  // (500 ms) sin importar cuanto tarden LCD y Serial.
  unsigned long ahora = millis();
  if (ahora - ultimaActualizacionMs < INTERVALO_ACTUALIZACION_MS) return;
  ultimaActualizacionMs = ahora;

  actualizarFrecuencia();           // primero: el voltaje usa la frecuencia
#if MODO_RMS_VERDADERO
  medirVoltajeRMSVerdadero(frecuencia);
#else
  medirVoltajePico(frecuencia);
#endif
  mostrarLCD();
  enviarSerial();
}
