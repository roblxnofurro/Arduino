# Auditoría técnica: medidor de Vrms y frecuencia con Arduino Uno

| Archivo | Contenido |
|---|---|
| `referencia/codigo_original/codigo_original.ino` | Código original, sin modificar |
| `medidor_rms_frecuencia/medidor_rms_frecuencia.ino` | Código corregido (versión completa) |
| `medidor_minimo/medidor_minimo.ino` | Código original con solo los cambios necesarios |
| `pruebas/` | Banco de pruebas en PC (modelo del circuito + Arduino simulado) |

> **Estado:** el programa corregido está validado **en simulación**, no en el montaje físico.
> Las constantes de calibración son **provisionales**. Ninguna cifra de este documento es
> una medición experimental. Las tablas de la Fase 5 están vacías a propósito.

> ⚠️ **CORRECCIÓN IMPORTANTE (frecuencia).** La primera versión de esta auditoría afirmaba
> que el optoacoplador genera 1 flanco por ciclo y que el `/ 2.0` del original era un error.
> **Era falso.** Se dedujo de la imagen del esquema y el simulador modelaba esa misma
> suposición, así que no podía detectarla. Al probarlo en Proteus, sin el `/2` la frecuencia
> salía **duplicada** (120 Hz con 60 Hz de entrada). El circuito genera **2 flancos FALLING
> por ciclo** y el `/ 2.0` del original **era correcto**. Ambas versiones corregidas usan
> ahora 2 flancos por ciclo, y el simulador modela ese comportamiento por defecto (`ppc=2`).

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

**Verificado en Proteus: 2 flancos de bajada por ciclo** (uno por semiciclo). Sin dividir
entre 2, una entrada de 60 Hz se muestra como 120 Hz.

La deducción inicial a partir de la imagen (LED alimentado con AC y D1 en antiparalelo → 1
flanco) **era incorrecta**. La resolución de la imagen no permite seguir con certeza a qué
nodo va R3. El comportamiento observado indica que el LED recibe corriente en **ambos
semiciclos**.

Como los dos flancos están separados medio período, el promedio de los intervalos es T/2 y
la división entre 2 da la frecuencia correcta. Se puede confirmar con el osciloscopio de
Proteus: un canal en V1 y otro en D2 deben mostrar 2 bajadas por cada ciclo de la senoide.

### 1.3 Lista de errores encontrados

Las cifras salen del banco de pruebas (`pruebas/`), que ejecuta el código original y el
corregido contra el mismo modelo del circuito.

| # | Gravedad | Error | Efecto comprobado en simulación |
|---|---|---|---|
| ~~E1~~ | **Retirado** | ~~El `/ 2.0` de la frecuencia es un error~~. **No lo era**: el circuito genera 2 flancos por ciclo (verificado en Proteus, sección 1.2) | Con el `/2` el original muestra 60.0 Hz para 60 Hz. Quitarlo daba 120 Hz |
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
- **Cálculo de la frecuencia con `/ 2.0`:** correcto, porque hay 2 flancos por ciclo (sección 1.2).
- **Filtro de 500 µs:** es adecuado para 50/60 Hz. Con 2 flancos por ciclo limita la
  frecuencia máxima de la señal a ~1 kHz (2000 flancos/s).
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

- `PULSOS_POR_CICLO = 2` (verificado en Proteus, sección 1.2). Hace lo mismo que el `/2`
  del original, pero como constante con nombre.
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
V_pico    = CAL_GANANCIA · V_C1,pico + CAIDA_PUENTE_V
V_rms     = V_pico / FACTOR_CRESTA            (√2 si la entrada es senoidal)
```

- **Promedio de unas 900 muestras en 100 ms** en lugar del máximo: es insesgado y reduce el
  ruido aleatorio en un factor de ~√N.
- **Corrección del rizado:** el promedio del diente de sierra es `Vp − ΔV/2`. Con el hardware
  actual vale 0.05 %; se incluye para que el código siga siendo correcto si se reduce C1.
- **CAIDA_PUENTE_V** representa la caída de los 2 diodos que conducen. **No se supone**: se
  **mide** como `Vp en bornes − V_C1` (sección 10.4). El valor 1.50 V es provisional.
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
| Frecuencia máxima | ~1 kHz (filtro de 500 µs con 2 flancos por ciclo). En la práctica también la limita el t_off del 4N25 con 10 kΩ | |
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
| `PULSOS_POR_CICLO = 2`, equivalente al `/2.0` del original | — | Sección 1.2 (verificado en Proteus) |
| Vrms = Vpico / `FACTOR_CRESTA`, documentado como suposición; Opción B disponible | E2 | Secciones 1.1 y 2.3 |
| `CAL_GANANCIA` + `CAIDA_PUENTE_V` (calibración en 2 puntos) | E3 | Separa ganancia y offset |
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
| **f** | 50 intervalos de 10000 µs (2 por ciclo) → 10⁶/(10000·2) | **50.00 Hz** |

Con el código original: ADC máximo = 870 → 870·5/1023·2 + 1.5 = 10.004 V → 7.074 V; f = 50.00 Hz.

### 4.2 Resultados del banco de pruebas (`pruebas/`, simulación)

**Frecuencia** (senoidal 10 Vp, modelo con 2 flancos por ciclo, como en Proteus):

| f real (Hz) | 1 | 2 | 5 | 50 | 60 | 400 | 990 | 1100 |
|---|---|---|---|---|---|---|---|---|
| Original | 1.00 | 2.00 | 5.00 | 50.00 | 60.00 | 400.0 | 989.2 | 550.0 ✗ |
| Corregido (completo) | 1.00 | 2.00 | 5.00 | 50.00 | 60.00 | 400.0 | 990.0 | 550.0 ✗ |
| Corregido (mínimo) | 1.00 | 2.00 | 5.00 | 50.00 | 60.00 | 400.0 | 990.0 | 550.0 ✗ |

Por encima de ~1 kHz, los flancos llegan cada < 500 µs y el filtro descarta uno de cada dos.
La frecuencia de la señal en sí **no se corrigió**: el original ya la calculaba bien.

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
CAL_GANANCIA = 0.9995 y CAIDA_PUENTE_V = 1.109:

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

### 5.3 Verificación en 2 puntos (modo 0), después de medir los componentes

> Primero se miden VREF_ADC, R1, R2 y CAIDA_PUENTE_V (sección 10.4). Este procedimiento
> sirve para **comprobar** que no queda un error de ganancia sistemático. Va y Vb deben ser
> picos **medidos en los bornes** con el osciloscopio, no los indicados por el generador.
> Si la ganancia resultante difiere de 1 en más de ~1 %, hay que revisar las mediciones de
> los componentes antes de aceptarla.


1. Aplicar una amplitud baja conocida **Va** (por ejemplo ~30 % del máximo) y esperar
   **15 s** (por τ). Anotar el `Vpico` del Serial como **Ma**.
2. Aplicar una amplitud alta **Vb** (~90 % del máximo, sin SATURADO) y esperar. Anotar **Mb**.
   Si la referencia mide Vrms, el pico es Vrms·√2 (solo para senoidal).
3. Con los valores actuales del código G₀ = CAL_GANANCIA y O₀ = CAIDA_PUENTE_V:
   ```
   Ca = (Ma − O₀)/G₀        Cb = (Mb − O₀)/G₀
   CAL_GANANCIA = (Vb − Va) / (Cb − Ca)
   CAIDA_PUENTE_V = Va − CAL_GANANCIA · Ca
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
6. Frecuencia: entre 0.5 Hz y ~1 kHz. Exactitud limitada por el resonador (±0.5 % sin calibrar).
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
7. Captura del osciloscopio de Proteus en V1 y D2, para documentar los 2 flancos por ciclo y entender qué nodo alimenta a R3.
8. Fotos del montaje físico.

---

## 10. Revisión con la señal real del generador (60 Hz, 20 Vpp, senoidal, offset 0 V)

**Referencia en vacío del generador:** Vp = 10 V, Vpp = 20 V, Vrms = 10/√2 = 7.071 V,
f = 60 Hz, T = 16.667 ms.

Cada conclusión lleva una etiqueta:

- **[CÓDIGO]**: comprobado leyendo el código.
- **[ESQUEMA]**: deducido del esquemático.
- **[SIM]**: comprobado en el banco de pruebas (modelo, no medición).
- **[HIPÓTESIS]**: pendiente de medir.

### 10.1 Auditoría de voltaje: respuestas punto por punto

| # | Pregunta | Respuesta |
|---|---|---|
| 1 | ¿25 ms capturan el pico? | A 60 Hz son 1.5 ciclos, así que sí. Pero **no hay pico que capturar**: en A0 hay continua porque C1 retiene el pico. El problema del original no era la duración, sino usar el máximo, que está sesgado por el ruido. [ESQUEMA][SIM] |
| 2 | ¿La frecuencia de muestreo es suficiente? | `analogRead` tarda ~112 µs → ~8.9 kS/s → ~149 muestras por ciclo a 60 Hz. Si A0 recibiera la onda, el error máximo al buscar el pico sería del 0.02 %. Para RMS verdadero (Opción B) también sobra. [CÓDIGO] |
| 3 | ¿El puente produce onda completa? | Sí, pero C1 la filtra: lo que llega al divisor es continua con un rizado de **7.5 mV** a 60 Hz (8.5 V / (2·60·9.4 s)). [ESQUEMA] |
| 4 | ¿Cuál es la caída real del puente? | **No se puede saber sin medirla.** El 1.50 V es un valor ajustado en Proteus, no medido. Se determina como `Vp en bornes − V_C1` (sección 10.4). En Proteus, si el voltímetro DC sobre C1 marca 8.50 V con 10 Vp, el 1.50 V queda confirmado para ese modelo. [HIPÓTESIS] |
| 5 | ¿El divisor es 2? | Nominalmente sí: (10k+10k)/10k = 2. Con resistencias del 5 %, el peor caso va de 1.905 a 2.105 (**±5 %** de error en todas las lecturas). Hay que medir R1 y R2. [ESQUEMA] |
| 6 | ¿Afecta el offset DC? | Con este circuito, **sí y mucho**. El puente carga C1 al mayor de los dos picos: Vp + \|Vo\|. Con Vo = ±1 V el programa marca 7.77 V, mientras que el RMS real es √(7.071² + 1²) = 7.14 V (**+8.8 %**). La frecuencia no se ve afectada (60.00 Hz). Como está confirmado Vo = 0, no aplica, pero conviene comprobarlo con el osciloscopio en acoplamiento DC. [SIM] |
| 7 | ¿Señal rectificada o polarizada? | **Rectificada y retenida en el pico.** No hay polarización (offset) en A0. [ESQUEMA] |
| 8 | ¿La fórmula corresponde al circuito? | `Vrms = (V_C1 + caída del puente)/√2` es válida **si la onda en bornes es senoidal y sin offset**. `√(ΣVi²/N)` **no** se puede aplicar a A0 con este hardware (sección 1.1). [ESQUEMA] |

### 10.2 Auditoría de frecuencia a 60 Hz

| Punto | Conclusión |
|---|---|
| Flancos por ciclo | **2** [VERIFICADO EN PROTEUS]: sin dividir entre 2 se mostraban 120 Hz. La primera versión de este informe decía 1, y era incorrecto. |
| Salida del 4N25 | D2 baja una vez por semiciclo. El período del Serial ya está multiplicado por PULSOS_POR_CICLO: con 60 Hz debe mostrar ≈ 16667 µs, con N ≈ 60 intervalos por ventana de 500 ms. |
| ¿Dividir entre 2? | **Sí.** El original lo hacía correctamente. La versión completa usa `PULSOS_POR_CICLO = 2` y la mínima conserva el `/ 2.0`. |
| Precisión de `micros()` | 4 µs de resolución. Con ~60 intervalos por ventana de 500 ms, el error es ±8 µs / 500 ms = **±0.002 %**. El límite real es el resonador cerámico del Uno (±0.5 % típico = **±0.3 Hz** a 60 Hz) [HIPÓTESIS]. En Proteus el reloj es exacto. |
| Filtro de 500 µs | Adecuado. Los flancos llegan cada 8.3 ms. El filtro elimina los rebotes del flanco lento del optoacoplador, porque la entrada crece despacio cerca del cruce por cero. |
| Interrupciones | Correctas en el original (volatile + copia atómica). Se mantienen. |
| Acumulación y reinicio | Correctos. Se cambió el centinela `0` por una bandera. |
| Pérdida de pulsos | No hay [CÓDIGO]. Puede haberla si la amplitud es tan baja que el 4N25 no conmuta [HIPÓTESIS]. |
| Estabilidad | [SIM]: 60.00 Hz en todas las ventanas, incluso con un generador de 50 Ω y con offset. |

### 10.3 El generador de funciones: 50 Ω y Hi-Z

Este factor **no está en el código** y puede explicar por sí solo que las lecturas no
coincidan con la pantalla del generador.

**a) Modo de carga configurado [SIM].** Muchos generadores muestran la amplitud suponiendo
una carga de 50 Ω. Si está en modo "50 Ω" y se conecta a este circuito, que es de alta
impedancia, la tensión real es **el doble: 40 Vpp (20 Vp)**.

- En la simulación, C1 llega a **17.5 V**, así que A0 recibiría **~8.8 V**, por encima del
  máximo absoluto del ATmega328P (VCC + 0.5 V). El programa corregido muestra `SATURADO`;
  el original mostraba 8.13 V sin avisar.
- **Riesgo de dañar A0.** Antes de conectar: poner el generador en **Hi-Z ("High Z")** y
  comprobar con el osciloscopio que en los bornes hay 20 Vpp.

**b) Aunque esté en Hi-Z, la salida sigue teniendo 50 Ω [SIM, modelo simplificado].** El
circuito consume corriente del generador:

- Rama R3 + LED: ~4.9 mA en el pico positivo.
- Rama R3 + D1: ~5.2 mA en el pico negativo.
- Pulsos de recarga de C1 a través del puente, justo en los picos.

Resultado estimado con 50 Ω:

| Magnitud | Generador en vacío | En bornes del circuito | Programa corregido |
|---|---|---|---|
| Vp | 10.000 V | 9.54 V (cima aplanada) | 9.53 V |
| Vrms | 7.071 V | 6.88 V (True-RMS) | **6.74 V** |
| f | 60 Hz | 60 Hz | 60.00 Hz |

Interpretación:

- El instrumento mide bien el **pico que realmente recibe** (9.53 frente a 9.54 V).
- Esa tensión es ~4.6 % menor que la del generador en vacío. Es una caída real, no un error
  del programa.
- Además, la carga aplana la cima de la onda, que deja de ser senoidal. Por eso `Vp/√2`
  queda ~2 % por debajo del RMS verdadero en bornes.
- **No se debe "calibrar" para que marque 7.07 V**, porque se estaría ocultando un efecto
  físico. La referencia correcta es la **tensión medida en los bornes** con el osciloscopio
  o con un multímetro True-RMS.
- Con la Opción B (entrada de 43 kΩ, sin puente ni C1), la simulación da 6.90 V frente a
  6.90 V True-RMS en bornes. Mide el RMS real aunque la onda esté deformada.
- Los valores 9.54 V y 6.88 V salen de un modelo con diodos ideales más una caída fija y
  50 Ω puros. **Hay que medirlos.**

**c) Tierras [HIPÓTESIS, verificar].** La malla del BNC del generador y la pinza de tierra
del osciloscopio suelen estar conectadas a la tierra de la red. Si el Arduino se alimenta
por USB desde un PC de sobremesa, su GND (que es el − del puente) también suele estarlo.
Si la malla del generador va a una entrada del puente, **uno de los diodos queda
cortocircuitado por la tierra**:

- El puente pasa a ser de media onda, con **una sola** caída de diodo.
- El generador queda cortocircuitado en el semiciclo negativo, limitado por sus 50 Ω.
- La lectura subiría unos +0.5 V en Vrms (≈ Vf/√2), y la onda negativa quedaría recortada
  en el osciloscopio.

Cómo comprobarlo:

- Con todo enchufado y apagado, medir continuidad entre la malla del generador y el GND
  del Arduino.
- Si hay continuidad, alimentar el Arduino desde un portátil **con batería** (o con un
  aislador USB). Otra opción es usar un transformador de aislamiento entre el generador y
  el circuito.
- **Nunca** poner la pinza de tierra del osciloscopio en el GND del Arduino mientras otra
  sonda está en la entrada del puente.

### 10.4 Medición de las constantes físicas (sin ajustar para forzar 7.07 V)

Con el generador en **Hi-Z**, 60 Hz, 20 Vpp, offset 0 V, conectado, y **esperando 30 s**:

1. **VREF_ADC:** multímetro DC entre 5V y GND del Arduino, con el LCD conectado.
2. **Divisor, medido en el circuito:** con un multímetro **de batería** (flotante), medir
   la tensión DC sobre C1 (`V_C1,med`) y entre A0 y GND (`V_A0,med`). La relación real es
   `V_C1,med / V_A0,med`. Con el circuito apagado y R1/R2 desconectadas, medir R1 y R2 y
   escribirlas en `R1_OHM` y `R2_OHM`. Ambos métodos deben coincidir en menos del 1 %.
3. **Comprobación de VREF y del divisor:** el campo `VC1` del Serial debe coincidir con
   `V_C1,med` en menos del 1 %. Si no coincide, VREF_ADC o R1/R2 están mal medidos.
4. **Pico en bornes:** osciloscopio en las dos entradas AC del puente, con la pinza de
   tierra en el lado de la malla del generador (sección 10.3c), en acoplamiento DC. Anotar
   `Vp,bornes` (comprobando también que la media es 0 V) y la frecuencia.
5. **Caída del puente:** `CAIDA_PUENTE_V = Vp,bornes − V_C1,med`. Escribirla y cargar el
   programa.
6. **CAL_GANANCIA** se queda en 1.000. Solo se modifica si la verificación en varios puntos
   (sección 5.3) muestra un error de ganancia sistemático después de los pasos 1–5.

**En Proteus**, los pasos equivalentes son: un DC voltmeter sobre C1 y otro en A0, y el
osciloscopio ARD1 para el pico y para D2. VREF es 5.00 V exactos y VSINE es una fuente
ideal sin 50 Ω.

### 10.5 Validación teórica a 60 Hz y 20 Vpp

Se supone una caída del puente de 1.50 V, sin verificar. El símbolo ⇒ indica valores que
dependen de esa suposición.

| Magnitud | Cálculo | Valor esperado |
|---|---|---|
| V_C1 | 10 − 1.50 | ⇒ 8.50 V |
| V_A0 | 8.50 / 2 | ⇒ 4.25 V |
| ADC | 4.25 · 1024 / 5 | ⇒ 870.4 |
| Rizado en C1 | 8.5 / (2·60·9.4) | 7.5 mV |
| Corrección del rizado | 1 / (1 − 1/(4·60·9.4)) | ×1.00044 |
| Vpico | V_C1 · 1.00044 + 1.50 | ⇒ 10.00 V |
| **Vrms** | 10.00 / √2 | **7.07 V** |
| Período | 10⁶ / 60 | **16667 µs** |
| Intervalos por ventana | 500 ms / 8.333 ms | ≈ 60 |
| **Frecuencia** | 10⁶ / (8333.3 · 2) | **60.00 Hz** |

Sensibilidad (por qué hay que medir y no suponer):

- Un error de **0.1 V** en la caída del puente → 0.071 V en Vrms (**1.0 %**).
- Un error del **1 %** en VREF o en el divisor → **0.85 %** en Vrms.

Pantalla esperada (con fuente ideal, como en Proteus):

```
Vrms:  7.07 V
Frec:  60.00 Hz
```

Serial esperado (simulado con fuente ideal):

```
ADC: 869.5 | Voltaje ADC: 4.246 V | VC1: 8.491 V | Vpico: 9.995 V | Voltaje RMS: 7.068 V | Frecuencia: 60.00 Hz | Periodo: 16667 us | N: 60
```

El código original, en las mismas condiciones, muestra `Vrms: 7.07 V` y `Freq: 60.0 Hz`.

Resumen de la simulación [SIM]:

| Caso | Original | Corregido | Referencia |
|---|---|---|---|
| Fuente ideal (Proteus) | 7.07 V / 60.0 Hz | 7.07 V / 60.00 Hz | 7.071 V / 60 Hz |
| Generador 50 Ω en Hi-Z | 6.75 V / 60.0 Hz | 6.74 V / 60.00 Hz | 6.88 V True-RMS en bornes (7.071 V en vacío) |
| Generador en modo 50 Ω (40 Vpp reales) | 8.13 V / 60.0 Hz, sin aviso | **SATURADO** / 60.00 Hz | 13.7 Vrms (19.0 Vp) en bornes: **riesgo para A0** |
| Offset +1 V | 7.78 V / 59.9 Hz | 7.77 V / 60.00 Hz | 7.14 V |

### 10.6 Pruebas experimentales con el generador

Configuración en todas las pruebas: **Hi-Z**, senoidal, offset 0 V, verificación previa de
tierras (10.3c) y espera de 30 s tras cada cambio de amplitud. En cada punto se anotan
10 lecturas del Serial y se calcula su media y su desviación.

- `Error vs bornes = (Arduino − bornes)/bornes · 100`: **evalúa el instrumento**.
- `Error vs generador = (Arduino − generador)/generador · 100`: incluye la carga y la
  impedancia del generador.

**Tabla 1. Punto principal.** A rellenar con mediciones reales:

| Parámetro | Configurado en generador | Medido en bornes (osciloscopio / True-RMS) | Medido por Arduino | Error vs bornes (%) | Error vs generador (%) |
|---|---|---|---|---|---|
| Frecuencia | 60 Hz | | | | |
| Período | 16.667 ms | | | | |
| Vpp | 20 V | | — | | |
| Vpico | 10 V | | | | |
| Vrms | 7.071 V | | | | |
| Offset | 0 V | | — | | |

**Tabla 2. Frecuencia** (20 Vpp). A rellenar con mediciones reales:

| f configurada (Hz) | f en osciloscopio | f Arduino | Período Serial (µs) | Error (%) |
|---|---|---|---|---|
| 10 | | | | |
| 30 | | | | |
| 60 | | | | |
| 120 | | | | |
| 500 | | | | |
| 1000 | | | | |

**Tabla 3. Amplitud** (60 Hz). A rellenar con mediciones reales:

| Vpp configurado | Vp en bornes | Vrms True-RMS en bornes | Vrms Arduino | Error vs bornes (%) |
|---|---|---|---|---|
| 6 | | | | |
| 10 | | | | |
| 14 | | | | |
| 20 | | | | |
| 22 (debe acercarse a SATURADO) | | | | |

**Tabla 4. Pruebas de robustez:**

| Prueba | Procedimiento | Resultado esperado | Resultado medido |
|---|---|---|---|
| Amplitud mínima para la frecuencia | Bajar Vpp hasta que aparezca `---` | Desconocido (depende del CTR del 4N25) | |
| Bajada tras un escalón | 20 → 10 Vpp; medir el tiempo hasta ±1 % | ≈ τ·ln(8.5/3.5) ≈ 8–9 s | |
| Señal desconectada | Quitar el generador | f = `---` en ≤ 2 s; Vrms baja a 0 en ~50 s (C1) | |
| Estabilidad | 20 lecturas a 60 Hz / 20 Vpp | Desviación ≤ 1 LSB (6.9 mV) y ≤ 0.01 Hz | |
