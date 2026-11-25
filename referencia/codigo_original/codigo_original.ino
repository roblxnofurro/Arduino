// CODIGO ORIGINAL - SE CONSERVA SIN MODIFICAR COMO REFERENCIA PARA LA AUDITORIA.
// La version corregida esta en ../../medidor_rms_frecuencia/medidor_rms_frecuencia.ino

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
int lecturaADC = 0;
float voltajeADC = 0;
float voltajeDC = 0;
float voltajePico = 0;
float voltajeRMS = 0;
float frecuencia = 0;

// Constantes
const float REFERENCIA_ADC = 5.0;
const float DIVISOR = 2.0;
const float CAIDA_PUENTE = 1.50; // Calibrado para 20 Vpp -> 7.07 Vrms
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
  // 1. Muestrear durante 25 ms para capturar el pico real de la onda
  int lecturaMaxima = 0;
  unsigned long inicioMuestreo = millis();

  while (millis() - inicioMuestreo < 25) {
    int lecturaActual = analogRead(sensor);
    if (lecturaActual > lecturaMaxima) {
      lecturaMaxima = lecturaActual;
    }
  }

  lecturaADC = lecturaMaxima;
  voltajeADC = lecturaADC * (REFERENCIA_ADC / 1023.0);

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
      frecuencia = (1000000.0 / periodoPromedio) / 2.0;
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
  lcd.print("Vrms: ");
  lcd.print(voltajeRMS, 2);
  lcd.print(" V   ");

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

  Serial.println(" Hz");

  delay(500);
}
