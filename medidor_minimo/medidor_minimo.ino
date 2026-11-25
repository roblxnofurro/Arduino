// VERSION MINIMA: es el codigo original con solo los cambios necesarios.
// Cada cambio esta marcado con "CORREGIDO:" y su motivo.

#include <LiquidCrystal.h>

// Configuracion de la pantalla LCD
LiquidCrystal lcd(7, 6, 5, 4, 3, 8);

// Divisor de voltaje
const int sensor = A0;

// Entrada del optoacoplador
const int opto = 2;

// Variables para medir y promediar la frecuencia
volatile unsigned long tiempoAnterior = 0;
volatile unsigned long sumaPeriodos = 0;
volatile unsigned int conteoPulsos = 0;
volatile unsigned long ultimoCruce = 0;

// Variables de medicion
float lecturaADC = 0; // CORREGIDO: float, porque ahora guarda un promedio
float voltajeADC = 0;
float voltajeDC = 0;
float voltajePico = 0;
float voltajeRMS = 0;
float frecuencia = 0;

// Constantes
const float REFERENCIA_ADC = 5.0;
const float DIVISOR = 2.0;
const float CAIDA_PUENTE = 1.50; // Valor ajustado en Proteus para 20 Vpp.
                                 // NOTA: en el montaje fisico hay que medirlo:
                                 // Vp en bornes - voltaje DC sobre C1
const float RAIZ_DOS = 1.4142;

// Funcion de interrupcion del optoacoplador
void detectarCruce() {
  unsigned long tiempoActual = micros();

  if (tiempoAnterior != 0) {
    unsigned long diferencia = tiempoActual - tiempoAnterior;

    // Filtro antirrebote de 500 us (hasta 2000 Hz)
    if (diferencia > 500) {
      sumaPeriodos += diferencia; // Acumula los periodos para promediarlos
      conteoPulsos++;
      tiempoAnterior = tiempoActual;
      ultimoCruce = tiempoActual;
    }
  } else {
    tiempoAnterior = tiempoActual;
    ultimoCruce = tiempoActual;
  }
}

void setup() {
  Serial.begin(9600);

  pinMode(sensor, INPUT);
  pinMode(opto, INPUT_PULLUP);

  // Inicializar pantalla LCD
  lcd.begin(16, 2);
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("Detector RMS");

  lcd.setCursor(0, 1);
  lcd.print("Iniciando...");

  // Activar interrupcion en D2
  attachInterrupt(digitalPinToInterrupt(opto), detectarCruce, FALLING);

  delay(2000);
  lcd.clear();
}

void loop() {
  // 1. Muestrear durante 25 ms
  // CORREGIDO: se calcula el PROMEDIO en vez de guardar solo el maximo.
  // C1 mantiene en A0 un voltaje continuo (casi sin rizado), asi que no hay
  // un pico que buscar. El maximo se queda con el ruido positivo y da
  // lecturas algo altas; el promedio de todas las muestras es mas estable.
  int lecturaMaxima = 0;
  unsigned long sumaLecturas = 0;
  unsigned int numeroLecturas = 0;
  unsigned long inicioMuestreo = millis();

  while (millis() - inicioMuestreo < 25) {
    int lecturaActual = analogRead(sensor);
    sumaLecturas += lecturaActual;
    numeroLecturas++;
    if (lecturaActual > lecturaMaxima) {
      lecturaMaxima = lecturaActual;   // se sigue guardando para detectar saturacion
    }
  }

  lecturaADC = (float)sumaLecturas / numeroLecturas;

  // CORREGIDO: 1024 en vez de 1023 (hoja de datos del ATmega328P:
  // ADC = Vin * 1024 / Vref).
  voltajeADC = lecturaADC * (REFERENCIA_ADC / 1024.0);

  // CORREGIDO: deteccion de saturacion. Si A0 llega a 5 V el ADC se queda
  // en 1023 y el voltaje calculado seria falso (mas bajo que el real).
  bool saturado = (lecturaMaxima >= 1020);

  // Voltaje antes del divisor
  voltajeDC = voltajeADC * DIVISOR;

  // Estimar el voltaje pico y RMS de entrada
  if (lecturaADC > 5) {
    voltajePico = voltajeDC + CAIDA_PUENTE;
    voltajeRMS = voltajePico / RAIZ_DOS;
  } else {
    voltajePico = 0;
    voltajeRMS = 0;
  }

  // 2. Obtener el promedio de periodos acumulados desde el ultimo ciclo de loop
  unsigned long sumaCopia;
  unsigned int conteoCopia;
  unsigned long ultimoPulso;

  noInterrupts();
  sumaCopia = sumaPeriodos;
  conteoCopia = conteoPulsos;
  ultimoPulso = ultimoCruce;
  sumaPeriodos = 0; // Reiniciar acumulador para la siguiente ventana de 500 ms
  conteoPulsos = 0;
  interrupts();

  // Calcular frecuencia promediada
  if ((micros() - ultimoPulso) < 1000000UL) {
    if (conteoCopia > 0) {
      float periodoPromedio = (float)sumaCopia / conteoCopia;
      // CORREGIDO: se quito el "/ 2.0". El LED del optoacoplador solo
      // enciende en el semiciclo positivo (D1 lo protege en el negativo), asi
      // que hay UN flanco FALLING por ciclo. Con el /2, 60 Hz se veia como 30 Hz.
      frecuencia = 1000000.0 / periodoPromedio;
    }
    // Si conteoCopia == 0 (frecuencias muy bajas < 2 Hz), mantiene la ultima frecuencia valida hasta que pase 1 seg
  } else {
    frecuencia = 0;
    noInterrupts();
    tiempoAnterior = 0;
    interrupts();
  }

  // Mostrar voltaje RMS en LCD
  lcd.setCursor(0, 0);
  if (saturado) {
    lcd.print("Vrms: SATURADO  ");   // CORREGIDO: aviso de fuera de rango
  } else {
    lcd.print("Vrms: ");
    lcd.print(voltajeRMS, 2);
    lcd.print(" V   ");
  }

  // Mostrar frecuencia en LCD
  lcd.setCursor(0, 1);
  lcd.print("Freq: ");
  lcd.print(frecuencia, 1);
  lcd.print(" Hz   ");

  // Mostrar resultados en Virtual Terminal / Monitor Serie
  Serial.print("ADC: ");
  Serial.print(lecturaADC);

  Serial.print(" | Voltaje ADC: ");
  Serial.print(voltajeADC, 2);

  Serial.print(" V | Voltaje RMS: ");
  Serial.print(voltajeRMS, 2);

  Serial.print(" V | Frecuencia: ");
  Serial.print(frecuencia, 2);

  Serial.print(" Hz");
  if (saturado) Serial.print(" | SATURADO");   // CORREGIDO: aviso de fuera de rango
  Serial.println();

  delay(500);
}
