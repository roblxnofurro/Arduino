# Auditoría técnica: medidor de Vrms y frecuencia con Arduino Uno

| Archivo | Contenido |
|---|---|
| `referencia/codigo_original/codigo_original.ino` | Código original, sin modificar |
| `medidor_rms_frecuencia/medidor_rms_frecuencia.ino` | Código corregido |
| `pruebas/` | Banco de pruebas en PC (modelo del circuito + Arduino simulado) |

> **Estado:** el programa corregido está validado **en simulación**, no en el montaje físico.
> Las constantes de calibración son **provisionales**. Ninguna cifra de este documento es
> una medición experimental. Las tablas de la Fase 5 están vacías a propósito.

---

## 0. Datos del circuito usados (tomados del esquemático de Proteus)

| Elemento | Valor | Fuente |
|---|---|---|
| Fuente V1 | VSINE; el comentario del código indica 20 Vpp (10 Vp) | Esquemático + código |
| Puente BR1 | "BRIDGE" genérico de Proteus | Esquemático |
| C1 | 470 µF, **en paralelo con la salida del puente** | Esquemático |
| R1 / R2 | 10 kΩ / 10 kΩ → A0 | Esquemático |
| R3, D1 | 1.8 kΩ en serie con el LED del 4N25; D1 (1N4007) en **antiparalelo** con el LED | Esquemático |
| U1 | 4N25. Colector (5) → D2 + R4 = 10 kΩ a VCC; emisor (4) → GND | Esquemático |
| LCD | LM016L, RS=7, E=6, D4..D7 = 5,4,3,8 | Código |

**Datos que no se pueden determinar y hay que confirmar** (ver la sección 9): amplitud y
frecuencia reales de V1, si el negativo del puente está unido al GND del Arduino, el puente
físico que se va a usar, los valores medidos de R1/R2, la tensión real del pin 5V y la forma
de onda.

---

## FASE 1. Auditoría del código original (sin modificarlo)

### 1.1 Qué llega realmente a A0

Este es el punto clave de toda la auditoría. C1 (470 µF) está conectado directamente a la
salida del puente. El conjunto **puente + C1 es un detector de pico**: C1 se carga hasta
`Vp − 2·Vf` en cada semiciclo y solo se descarga lentamente a través de R1 + R2.

- Constante de descarga: τ = (R1 + R2)·C1 = 20 kΩ · 470 µF = **9.4 s**
- Rizado con onda completa: ΔV ≈ V_C1 / (2·f·τ) = 8.5 V / (2·50·9.4) ≈ **9 mV** (0.1 %)

**En A0 hay una tensión continua, no la forma de onda.** Consecuencias:

1. Con este hardware **solo se puede medir el pico** de la entrada.
2. `Vrms = Vpico/√2` no es una medida de RMS: es una **suposición** de que la entrada es una
   senoidal pura.
3. Aplicar `√(ΣVi²/N)` a las muestras de A0 **no** daría el RMS de la entrada. Daría ≈ el
   valor de continua de C1, que es igual a su promedio. La fórmula solo tiene sentido si A0
   recibe la forma de onda (Opción B, sección 2.3).

### 1.2 Optoacoplador: ¿uno o dos pulsos por ciclo?

R3 alimenta el LED desde la fuente AC y D1 está en antiparalelo. Si el LED estuviera
alimentado desde la salida rectificada, D1 no tendría ninguna función, porque nunca habría
tensión inversa. Además, después de C1 la tensión es continua y no habría pulsos. Por tanto:

- El LED solo conduce en el **semiciclo positivo** (D1 lo protege en el negativo).
- D2 está en LOW durante el semiciclo positivo y en HIGH en el negativo.
- Hay **un único flanco de bajada (FALLING) por ciclo**.

Esto se dedujo del esquemático. Hay que **verificarlo** con el osciloscopio de Proteus
(canal en D2) o con el dato `Periodo` que ahora imprime el Serial (ver la Fase 5).

### 1.3 Lista de errores encontrados

Las cifras salen del banco de pruebas (`pruebas/`), que ejecuta el código original y el
corregido contra el mismo modelo del circuito.

| # | Gravedad | Error | Efecto comprobado en simulación |
|---|---|---|---|
| E1 | **Crítico** | `frecuencia = (1e6/periodo) / 2.0`. Divide entre 2 cuando hay 1 flanco por ciclo | Entrada de 50 Hz → muestra **25.0 Hz**; 60 Hz → 30.0 Hz (−50 % en todo el rango 1–1500 Hz) |
| E2 | **Crítico (conceptual)** | `Vrms = Vpico/√2` se presenta como RMS, pero solo vale para senoidal pura | Cuadrada de 10 Vp (Vrms real 10 V) → muestra 7.07 V (**−29 %**); triangular de 10 Vp (real 5.77 V) → 7.07 V (**+22 %**) |
| E3 | Alto | `CAIDA_PUENTE = 1.50` ajustada en **un solo punto** (20 Vpp en Proteus). Absorbe todos los demás errores (Vref, 1023, divisor) y no es la caída real de un puente físico | Si cada diodo real cae 0.55 V en lugar de 0.75 V: +4.0 % a 10 Vp y +7.9 % a 5 Vp. Con 0.45 V/diodo: hasta **+11.9 %** |
| E4 | Alto (solo físico) | `REFERENCIA_ADC = 5.0` supuesta. Por USB, el pin 5V real está entre ~4.6 y 5.2 V | Error de ganancia de hasta −8 % / +4 % en todas las lecturas (no aparece en Proteus) |
| E5 | Medio | Se usa el **máximo** de las muestras. En una señal continua con ruido, el máximo recoge el ruido positivo | Con ruido de 2 LSB rms: **+0.4 % a +0.6 %** de sesgo hacia arriba (7.10–7.12 V en vez de 7.07 V) |
| E6 | Medio (hardware) | τ = 9.4 s: la lectura **sube rápido pero baja muy lento**. El código no lo advierte | Escalón de 10 Vp a 5 Vp: tarda **~8.9 s** en llegar a ±1 %. Al quitar la señal tarda **~54 s** en mostrar 0. Esta es la causa de los "valores congelados" |
| E7 | Medio | No detecta saturación del ADC (A0 > 5 V → 1023) | Con 12 Vp (8.49 Vrms) muestra 8.13 V **sin ningún aviso** |
| E8 | Bajo | `ADC·5/1023` en vez de `ADC·Vref/1024` (datasheet del ATmega328P) | +0.1 % de ganancia (quedaba oculto dentro de CAIDA_PUENTE) |
| E9 | Bajo | `tiempoAnterior == 0` se usa como indicador de "sin referencia". `micros()` puede valer 0 legítimamente (tras desbordar a los ~71.6 min) | Un período perdido de vez en cuando; se sustituye por una bandera `bool` |
| E10 | Bajo | Timeout de 1 s: con una señal de 1 Hz (período = 1 s) la detección queda al límite | Frecuencia mínima poco fiable; ahora el timeout es de 2 s y se usa el último período medido |
| E11 | Bajo | En el LCD, `"Freq: " + 1000.0 + " Hz   "` ocupa 18 caracteres | Texto cortado por encima de 1000 Hz |
| E12 | Bajo | `RAIZ_DOS = 1.4142` truncado | 0.0015 %, despreciable |

### 1.4 Puntos del enunciado que resultaron correctos en el original

No conviene cambiar lo que ya funciona bien. Estas partes estaban correctas:

- **Variables compartidas con la ISR:** son `volatile` y se copian con `noInterrupts()`. Es
  lo correcto, porque las variables de 32 bits no se leen de forma atómica en un AVR de 8 bits.
- **Pérdida de pulsos:** no ocurre. `delay()`, `analogRead()`, el LCD y el `Serial` no
  deshabilitan las interrupciones, y el flag de INT0 retiene el flanco durante las secciones
  críticas, que son de pocos µs.
- **Reiniciar los acumuladores en cada ventana** es correcto, porque suma y conteo salen de
  la misma ventana. El período que cruza el borde se asigna a la ventana en la que termina y
  no se pierde.
- **Desbordamiento de `micros()`:** la resta sin signo `ahora − anterior` funciona bien
  aunque haya desbordado.
- **Filtro de 500 µs:** es adecuado para 50/60 Hz. Limita la frecuencia máxima a 2 kHz.
- **25 ms de muestreo:** cubren al menos 1.25 ciclos a 50 Hz. Como A0 es continua, la
  duración no era el problema; lo era usar el máximo (E5).

---

## FASE 2. Revisión del método de medición

### 2.1 Frecuencia: medición por período (se mantiene el método y se corrige)

La ISR guarda el tiempo de cada flanco válido y acumula los períodos. Cada 500 ms se calcula:

```
T_eventos = Σ períodos / N                  [µs]
f = CAL_FRECUENCIA · 10⁶ / (T_eventos · PULSOS_POR_CICLO)   [Hz]
```

- `PULSOS_POR_CICLO = 1` (justificado en la sección 1.2). Es una constante explícita, no un
  "/2" escondido en la fórmula.
- Promediar N períodos equivale a medir el tiempo entre el primer y el último flanco. El
  error de cuantización de `micros()` (4 µs) **no se acumula**: es de ±8 µs sobre ~500 ms, es
  decir ±0.002 %.
- Si en la ventana no hubo ningún período completo (f < ~2 Hz), se usa el último período
  medido en lugar de congelar el valor.
- Si pasan 2 s sin flancos, f = 0. La frecuencia mínima medible es 0.5 Hz.

### 2.2 Voltaje con el hardware actual (Opción A, por defecto)

```
V_A0      = (Σ ADC / N) · VREF_ADC / 1024
V_C1      = V_A0 · (R1 + R2) / R2
V_C1,pico = V_C1 / (1 − 1/(4·f·τ))           (corrección del rizado, τ = (R1+R2)·C1)
V_pico    = CAL_GANANCIA · V_C1,pico + CAL_OFFSET_V
V_rms     = V_pico / FACTOR_CRESTA            (√2 si la entrada es senoidal)
```

- **Promedio de unas 900 muestras en 100 ms** en lugar del máximo: es insesgado y reduce el
  ruido aleatorio en un factor de ~√N.
- **Corrección del rizado:** el promedio del diente de sierra es `Vp − ΔV/2`. Con el hardware
  actual vale 0.05 %; se incluye para que el código siga siendo correcto si se reduce C1.
- **CAL_OFFSET_V** representa la caída de los 2 diodos que conducen. **No se supone**: se
  obtiene por calibración en 2 puntos (Fase 5). El valor 1.50 V es provisional.
- **FACTOR_CRESTA** queda como constante explícita y documentada. Con este hardware **es
  imposible** medir la forma de onda, así que el Vrms solo es correcto si se conoce.

### 2.3 Para RMS verdadero hay que cambiar el hardware (Opción B, NO aplicada)

`MODO_RMS_VERDADERO 1` implementa:

```
Vrms = (1/k) · VREF/1024 · √( Σ(xᵢ − x̄)² / N )
```

`x̄` es el nivel de continua (offset) y `k` la atenuación. La ventana dura un **número entero
de períodos**, usando la frecuencia medida por el optoacoplador, para no sesgar el resultado.

Hardware necesario, **solo si se autoriza**:

```
 V1 (terminal 1) ──┬──[ Cc 10µF ]──[ Ra 33k ]──┬── A0
                   │                           ├──[ Rb 20k ]── +5V
                   └── (LED del 4N25 igual)    └──[ Rc 20k ]── GND
 V1 (terminal 2) ── GND
```

- Atenuación: k = (Rb‖Rc)/(Ra + Rb‖Rc) = 10k/43k = **0.2326**. Con 10 Vp, A0 = 2.5 V ± 2.33 V.
- Cc hace que el offset sea exactamente 2.5 V. El corte es fc = 1/(2π·43k·10µF) ≈ 0.37 Hz:
  menos de 0.3 % de error por encima de 5 Hz.
- **El puente y C1 deben salir del camino de medición.** Si un terminal de V1 se une a GND
  sin quitar el puente, uno de sus diodos cortocircuita la fuente en el semiciclo negativo.
- Una vez cambiado el hardware, se cambia `#define MODO_RMS_VERDADERO 1`.

### 2.4 Limitaciones del circuito actual

| Limitación | Valor | Causa |
|---|---|---|
| Vp máximo medible | ≈ 11.5 V (≈ 8.1 Vrms senoidal) | A0 ≤ 5 V → V_C1 ≤ 10 V, más 2·Vf |
| Vp mínimo medible | ≈ 2·Vf (~1.1–1.5 V) | Por debajo, el puente no conduce |
| Resolución | 1 LSB = 4.9 mV en A0 = **6.9 mV en Vrms** | ADC de 10 bits y divisor ×2 |
| Bajada de la lectura | τ = 9.4 s (asienta en τ·ln(V₁/V₂)) | C1 se descarga solo por R1+R2 |
| Forma de onda | Hay que suponerla | Detector de pico |
| Amplitud mínima para la frecuencia | Desconocida (IF pico = 0.44 mA a 2 Vp; 4.9 mA a 10 Vp) | CTR del 4N25 (≥ 20 %), R4 ‖ pull-up interno |
| Frecuencia máxima | 2 kHz (filtro). En la práctica también la limita el t_off del 4N25 con 10 kΩ | |
| Precisión del reloj | Resonador cerámico del Uno, ±0.5 % típico | Se corrige con CAL_FRECUENCIA |
| Aislamiento | Solo la parte de frecuencia está aislada; **A0 está unido galvánicamente a la fuente** | Divisor directo |

> ⚠️ **Montaje físico:** nunca conectar la red eléctrica (110/220 V) directamente.
> Usar un transformador cuyo pico no supere ~11 V con el divisor 10k/10k. Con un
> transformador de 12 VAC (≈17 Vp), A0 superaría los 5 V: habría que cambiar R1
> (por ejemplo a 27 kΩ) **y** actualizar `R1_OHM` en el código.

---

## FASE 3. Programa corregido

Archivo: `medidor_rms_frecuencia/medidor_rms_frecuencia.ino`.

| Cambio | Corrige | Justificación |
|---|---|---|
| `PULSOS_POR_CICLO = 1`, sin `/2.0` | E1 | Sección 1.2 |
| Vrms = Vpico / `FACTOR_CRESTA`, documentado como suposición; Opción B disponible | E2 | Secciones 1.1 y 2.3 |
| `CAL_GANANCIA` + `CAL_OFFSET_V` (calibración en 2 puntos) | E3 | Separa ganancia y offset |
| `VREF_ADC` y R1/R2/C1 como constantes medibles | E4, E8 | Ganancia derivada de los componentes |
| Promedio en lugar de máximo, ventana de 100 ms | E5 | Estimador insesgado |
| Detección de saturación → `"Vrms: SATURADO"` y aviso por Serial | E7 | No mostrar valores falsos |
| Bandera `isrHayReferencia`; timeout de 2 s; uso del último período | E9, E10 | Robustez a baja frecuencia |
| Filas del LCD de exactamente 16 caracteres, con `dtostrf` | E11 | Sin texto cortado ni restos |
| `millis()` en lugar de `delay(500)` | — | Cadencia fija de 500 ms |
| Serial: se añaden Vpico, Periodo y N | — | Necesario para verificar y calibrar |

**Se conserva:** las conexiones (LCD 7,6,5,4,3,8; A0; D2; FALLING; INPUT_PULLUP), el Serial a
9600 baudios (Virtual Terminal), la pantalla de inicio y los campos del Serial original.
No se usan librerías nuevas.

Ejemplo de salida por Serial:

```
ADC: 869.4 | Voltaje ADC: 4.245 V | Vpico: 9.995 V | Voltaje RMS: 7.068 V | Frecuencia: 50.00 Hz | Periodo: 20000 us | N: 25
```

LCD:

```
Vrms:  7.07 V
Frec:  50.00 Hz
```

---

## FASE 4. Validación matemática

### 4.1 Cálculo a mano (V1 = 10 Vp, 50 Hz, Vf = 0.75 V por diodo)

| Paso | Fórmula | Valor |
|---|---|---|
| Tensión en C1 | 10 − 2·0.75 | 8.500 V |
| Tensión en A0 | 8.5 · 10k/20k | 4.250 V |
| ADC ideal | 4.25 · 1024/5 | 870.4 cuentas |
| ADC promedio (simulado, incluye rizado) | — | 869.4 |
| V_A0 reconstruida | 869.4 · 5/1024 | 4.2451 V |
| V_C1 | 4.2451 · 2 | 8.4902 V |
| Corrección de rizado | k = 1/(4·50·9.4) = 0.00053 | 8.4948 V |
| V_pico | 8.4948 + 1.50 | 9.9948 V |
| **V_rms** | 9.9948 / √2 | **7.067 V** (real 7.071 V, **−0.05 %**, < 1 LSB) |
| **f** | 25 períodos de 20000 µs → 10⁶/20000 | **50.00 Hz** |

Con el código original: ADC máximo = 870 → 870·5/1023·2 + 1.5 = 10.004 V → 7.074 V; f = 25.00 Hz.

### 4.2 Resultados del banco de pruebas (`pruebas/`, simulación)

**Frecuencia** (senoidal 10 Vp):

| f real (Hz) | 1 | 2 | 5 | 50 | 60 | 400 | 1000 | 1500 |
|---|---|---|---|---|---|---|---|---|
| Original | 0.50 | 1.00 | 2.50 | 25.00 | 30.00 | 200.0 | 500.0 | 750.0 |
| **Corregido** | **1.00** | **2.00** | **5.00** | **50.00** | **60.00** | **400.0** | **1000.0** | **1500.0** |

**Vrms** (senoidal 50 Hz, modelo con Vf = 0.75 V, igual al supuesto por el código):

| Vp | 2 | 3 | 5 | 8 | 10 | 11 | 12 |
|---|---|---|---|---|---|---|---|
| Real | 1.414 | 2.121 | 3.536 | 5.657 | 7.071 | 7.778 | 8.485 |
| Original | 1.413 | 2.118 | 3.535 | 5.657 | 7.074 | 7.779 | 8.132 ✗ (sin aviso) |
| **Corregido** | 1.413 | 2.118 | 3.534 | 5.654 | 7.068 | 7.775 | **SATURADO** |

> En Proteus, el voltaje del código original salía bien **solo porque** CAIDA_PUENTE se
> ajustó a ese mismo modelo. El error grave de la simulación era la frecuencia. En el
> montaje físico aparecen E3, E4 y E5.

**Caída real del diodo distinta de la supuesta** (corregido sin calibrar → calibrado en 2 puntos).
Simulación con Vf = 0.55 V/diodo. La calibración con 4 Vp y 10 Vp dio
CAL_GANANCIA = 0.9995 y CAL_OFFSET_V = 1.109:

| Vp | Real | Sin calibrar | Calibrado |
|---|---|---|---|
| 3 | 2.121 | 2.401 (+13.2 %) | 2.124 (+0.1 %) |
| 6 | 4.243 | 4.522 (+6.6 %) | 4.244 (+0.03 %) |
| 8 | 5.657 | 5.937 (+4.9 %) | 5.658 (+0.02 %) |
| 11 | 7.778 | 8.058 (+3.6 %) | 7.778 (0.0 %) |

**Forma de onda** (10 Vp, 50 Hz):

| Forma | Vrms real | Modo 0 (hardware actual) | Modo 1 (hardware Opción B) |
|---|---|---|---|
| Senoidal | 7.071 | 7.068 | 7.071 |
| Cuadrada | 10.000 | 7.072 (−29 %) | 10.003 |
| Triangular | 5.774 | 7.066 (+22 %) | 5.774 |

**Escalón y apagado** (hardware actual): 10 → 5 Vp: dentro de ±1 % a los 8.9 s
(teoría τ·ln(8.5/3.5) = 8.3 s). 10 → 0 Vp: muestra 0 a los 53.9 s (teoría 53.3 s).

### 4.3 Qué valida y qué no valida esta simulación

- **Valida:** las fórmulas, la lógica de la ISR, la gestión de ventanas y timeouts, el
  formato del LCD y la saturación.
- **No valida:** el comportamiento eléctrico real del 4N25, del puente con corrientes de
  pico de carga, del ruido real, de la tensión del pin 5V ni del resonador. Tampoco se pudo
  compilar con el compilador AVR real (`avr-gcc`): en este entorno no se pudo instalar.
  **Compílalo en el IDE de Arduino** antes de cargarlo.

Para reproducir: `cd pruebas && ./ejecutar_pruebas.sh bin && ./bin/sim_corregido vp=10 f=60 traza=1`.

---

## FASE 5. Calibración y pruebas experimentales

### 5.1 Equipo necesario

- Multímetro **True-RMS** como referencia de Vrms (uno que no sea True-RMS solo vale para senoidal).
- Osciloscopio o frecuencímetro como referencia de frecuencia. También sirve la red de
  50/60 Hz, cuya desviación es de unos ±0.1 %.
- Fuente AC de amplitud conocida: un transformador de baja tensión o un generador de funciones.

### 5.2 Verificaciones previas

1. Medir el pin **5V** del Arduino con el multímetro → escribirlo en `VREF_ADC`.
2. Medir R1 y R2 **sin conectar** → escribir `R1_OHM` y `R2_OHM`.
3. Verificar 1 pulso por ciclo: con una señal de frecuencia conocida f, el `Periodo` del
   Serial debe ser ≈ 10⁶/f µs. Si sale la mitad, poner `PULSOS_POR_CICLO = 2` (y averiguar por
   qué el circuito no coincide con el esquemático).
4. Comprobar que el negativo del puente está unido al GND del Arduino.

### 5.3 Calibración de voltaje en 2 puntos (modo 0)

1. Aplicar una amplitud baja conocida **Va** (por ejemplo ~30 % del máximo) y esperar
   **15 s** (por τ). Anotar el `Vpico` del Serial como **Ma**.
2. Aplicar una amplitud alta **Vb** (~90 % del máximo, sin SATURADO) y esperar. Anotar **Mb**.
   Si la referencia mide Vrms, el pico es Vrms·√2 (solo para senoidal).
3. Con los valores actuales del código G₀ y O₀ (al principio 1.000 y 1.50):
   ```
   Ca = (Ma − O₀)/G₀        Cb = (Mb − O₀)/G₀
   CAL_GANANCIA = (Vb − Va) / (Cb − Ca)
   CAL_OFFSET_V = Va − CAL_GANANCIA · Ca
   ```
4. Escribir los valores, cargar de nuevo y **verificar en puntos distintos** de los de calibración.

### 5.4 Calibración de frecuencia

Con una referencia fiable: `CAL_FRECUENCIA = f_referencia / f_medida`, con el valor por
defecto 1.0000 al medir.

### 5.5 Protocolo de pruebas

Para cada punto: aplicar la señal, **esperar 15 s** si la amplitud bajó, anotar 5 lecturas
seguidas y calcular la media y la dispersión.

`Error % = (medido − real) / real · 100`

**Tabla A. Frecuencia** (Vp fija y conocida). A rellenar con mediciones reales:

| Frecuencia real (Hz) | Frecuencia medida (Hz) | Error (%) |
|---|---|---|
| 10 | por medir | |
| 50 | por medir | |
| 60 | por medir | |
| 100 | por medir | |
| 500 | por medir | |
| 1000 | por medir | |

**Tabla B. Voltaje** (senoidal, f fija). A rellenar con mediciones reales:

| Vrms real (multímetro) | Vrms medido (Arduino) | Error (%) | ¿Punto de calibración? |
|---|---|---|---|
| ~1.5 V | por medir | | |
| ~3 V | por medir | | sí (Va) |
| ~5 V | por medir | | |
| ~7.5 V | por medir | | sí (Vb) |
| ~8 V | por medir | | (debe avisar si satura) |

**Tabla C. Combinada** (formato pedido):

| Frecuencia real | Frecuencia medida | Vrms real | Vrms medido | Error f (%) | Error V (%) |
|---|---|---|---|---|---|
| | | | | | |

**Pruebas adicionales:** amplitud mínima a la que la frecuencia deja de leerse (bajar Vp hasta
que aparezca `---`); tiempo de bajada tras un escalón; estabilidad (desviación de 20 lecturas
seguidas con la señal fija).

---

## 6. Limitaciones conocidas del sistema corregido

1. **No es un medidor True-RMS con el hardware actual.** Mide el pico y supone la forma de onda.
2. Rango aproximado de 1.5 a 11.5 Vp con 10k/10k. Por encima muestra SATURADO.
3. La lectura **baja lento** (τ = 9.4 s). Es una limitación física de C1 y R1+R2. Reducir C1
   a 47 µF daría τ = 0.94 s con ~1 % de rizado (el código lo compensa), pero eso es un
   cambio de hardware que requiere autorización.
4. La caída de los diodos varía con la corriente y la temperatura (~2 mV/°C por diodo). La
   calibración en 2 puntos corrige la parte lineal, no la no lineal.
5. Resolución de 6.9 mV en Vrms. La exactitud depende de VREF (el 5V del USB varía con la
   carga) y de la calibración.
6. Frecuencia: entre 0.5 Hz y 2 kHz. Exactitud limitada por el resonador (±0.5 % sin calibrar).
   La amplitud mínima para que el 4N25 conmute está **sin determinar**.
7. Si se pierde un pulso (señal muy débil o ruidosa), ese período cuenta doble en su ventana.
   La ventana siguiente se recupera sola.
8. En el AVR, `double` es `float` de 32 bits (~7 cifras): es suficiente para estas magnitudes.

## 7. Información que hay que confirmar (no se ha inventado)

1. Modelo exacto de la placa (se asume Arduino Uno R3 con ATmega328P a 16 MHz).
2. Amplitud y frecuencia de V1 en Proteus, y de la fuente real (¿transformador de cuántos V?).
3. Forma de onda real de la entrada.
4. Puente físico (¿1N4007 discretos?, ¿W10M?, ¿KBP?) y su caída real.
5. Valores medidos de R1, R2, R3, R4 y C1.
6. Si en el esquemático el negativo del puente está unido al GND del Arduino.
7. Captura del osciloscopio de Proteus en D2, para confirmar 1 flanco por ciclo.
8. Fotos del montaje físico.
