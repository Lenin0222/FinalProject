#include <Arduino.h>         // Librería fundamental de Arduino
#include <Wire.h>            // Librería para comunicación I2C (necesaria para el LCD)
#include <LiquidCrystal_I2C.h> // Librería para la pantalla LCD con módulo I2C

// --- Definición de Pines ---
const int pinPIR = 2;        // Pin donde está conectado el sensor PIR (OUT)
const int pinBuzzer = 8;     // Pin donde está conectado el buzzer (+)
const int pinBoton = 4;      // Pin donde está conectado el botón

// --- Configuración de la Pantalla LCD I2C ---
// Objeto LCD: LiquidCrystal_I2C(dirección I2C, columnas, filas)
// La dirección I2C común es 0x27 o 0x3F. Si no funciona, prueba la otra.
LiquidCrystal_I2C lcd(0x27, 16, 2); // Si 0x27 no funciona, prueba con 0x3F: LiquidCrystal_I2C lcd(0x3F, 16, 2);

// --- Variables de Estado ---
enum EstadoAlarma {
  ALARMA_DESACTIVADA,     // Alarma apagada, esperando ser armada
  ALARMA_ARMANDO,         // Cuenta regresiva antes de activarse
  ALARMA_ACTIVADA,        // Alarma lista para detectar movimiento
  ALARMA_DISPARADA,       // Movimiento detectado y buzzer sonando
  ERROR_PIR_ARMADO        // Error al armar porque el PIR no fue detectado
};
EstadoAlarma estadoActualAlarma = ALARMA_DESACTIVADA; // La alarma inicia desactivada

bool movimientoDetectadoActual = false;    // Estado actual del sensor PIR
bool movimientoDetectadoAnterior = false;  // Para saber si en el ciclo anterior hubo movimiento
bool lcdNecesitaActualizar = true;         // Bandera para controlar cuándo actualizar el LCD completamente

// --- Variables para el Debounce del Botón ---
int valorBotonActual;              // Estado actual del botón (LOW si está presionado con INPUT_PULLUP)
int valorBotonAnterior = HIGH;     // Estado anterior del botón
unsigned long tiempoUltimoCambioBoton = 0; // Tiempo en el que el botón cambió de estado
const unsigned long debounceDelay = 50;  // Milisegundos de retardo para debounce

// --- Variables para el Buzzer Intermitente y Conteo ---
unsigned long tiempoUltimaAlternacionBuzzer = 0; // Para el ritmo ON/OFF
const long duracionPulsoBuzzer = 200;           // Duración de cada pulso (ON o OFF)
int conteoPulsosBuzzer = 0;                     // Contador de pulsos para limitar el sonido
const int maxPulsosBuzzer = 10;                 // 5 veces ON + 5 veces OFF = 10 pulsos para 5 pitidos completos
bool buzzerCompletadoCiclo = false;             // Indica si el buzzer ya terminó sus 5 pitidos

// --- Variables para el PIR y tiempo sin movimiento para el LCD ---
unsigned long tiempoUltimoMovimientoReal = 0;   // Tiempo en el que se detectó el último movimiento real
const unsigned long tiempoEsperaSinMovimiento = 500; // 0.5 segundos

// --- Constante para el tiempo de calibración del PIR ---
const unsigned long tiempoCalibracionPIR = 10000; // 10 segundos

// --- Variables para la desactivación automática ---
unsigned long tiempoInicioAlarmaActiva = 0; // Guarda el tiempo en que la alarma pasó a estado ALARMA_ACTIVADA
const unsigned long duracionAlarmaAutomatica = 5 * 60 * 1000; // 5 minutos en milisegundos

// --- Variables para el retardo de armado ---
unsigned long tiempoInicioArmado = 0;          // Guarda el tiempo en que se inició el proceso de armado
const unsigned long duracionRetardoArmado = 5000; // 5 segundos para el retardo de armado

// --- Variables para la verificación de conexión del PIR al armar ---
unsigned long tiempoInicioVerificacionPIRArmado = 0;
const unsigned long duracionVerificacionPIRArmado = 1000; // Tiempo para esperar una señal del PIR al intentar armar

// --- Contador de disparos de alarma ---
int contadorDisparosAlarma = 0; // Contador para las veces que la alarma se ha disparado

// --- Función para verificar la conexión del PIR ---
// Retorna true si el PIR detecta algo (un HIGH) en un corto período, false si no.
// Con una resistencia pulldown, un pin desconectado o un PIR en reposo (sin movimiento) leerá LOW.
// Solo si el PIR está conectado Y detecta movimiento, leerá HIGH.
// Esto ayuda a confirmar que el PIR está funcional.
bool verificarConexionPIR() {
  unsigned long tiempoInicio = millis();
  while ((millis() - tiempoInicio) < duracionVerificacionPIRArmado) {
    if (digitalRead(pinPIR) == HIGH) {
      return true; // PIR detectó algo, está conectado y funcional.
    }
    delay(10); // Pequeño delay para no sobrecargar el bucle.
  }
  return false; // PIR no detectó un HIGH en el tiempo de verificación (desconectado o no funcional).
}


// --- Función setup(): Se ejecuta una sola vez al iniciar el Arduino ---
void setup() {
  Serial.begin(9600); // Inicia la comunicación serial para depuración

  // Configuración de pines
  // IMPORTANTE: Asegúrate de tener una resistencia de 10K Ohm del pinPIR a GND.
  pinMode(pinPIR, INPUT);          // El pin del PIR es una entrada
  pinMode(pinBuzzer, OUTPUT);      // El pin del buzzer es una salida
  pinMode(pinBoton, INPUT_PULLUP); // Si usas resistencia pull-up INTERNA de Arduino (botón a GND)

  // Inicialización de la pantalla LCD
  lcd.init();        // Inicializa el LCD
  lcd.backlight();   // Enciende la luz de fondo del LCD

  // --- Secuencia de Inicio y Calibración del PIR ---
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Sistema de Alarma");
  lcd.setCursor(0, 1);
  lcd.print("Iniciando...");
  delay(2000); // Espera 2 segundos para mostrar el mensaje de inicio

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Calibrando PIR...");

  for (int i = tiempoCalibracionPIR / 1000; i >= 0; i--) {
    lcd.setCursor(0, 1);
    lcd.print("Espere ");
    if (i < 10) lcd.print(" ");
    lcd.print(i);
    lcd.print("s ");
    Serial.print("Tiempo restante: ");
    Serial.print(i);
    Serial.println("s");
    delay(1000);
  }

  Serial.println("PIR Calibrado. Sistema LISTO.");
  // --- FIN Secuencia de Calibración ---

  // Asegurarse de que el buzzer esté APAGADO al final de la Inicialización
  noTone(pinBuzzer);
  digitalWrite(pinBuzzer, LOW);
  buzzerCompletadoCiclo = false;
  conteoPulsosBuzzer = 0;
  estadoActualAlarma = ALARMA_DESACTIVADA; // La alarma se inicializa DESACTIVADA
  lcdNecesitaActualizar = true; // Forzar actualización inicial del LCD
  tiempoUltimoMovimientoReal = millis();
  movimientoDetectadoAnterior = false;
  tiempoInicioAlarmaActiva = 0;
}

// --- Función loop(): Se ejecuta repetidamente mientras el Arduino esté encendido ---
void loop() {
  // --- 1. Lectura y Debounce del Botón ---
  // Esta parte siempre se ejecuta para que el botón sea siempre responsivo.
  int lecturaBoton = digitalRead(pinBoton);

  if (lecturaBoton != valorBotonAnterior) {
    tiempoUltimoCambioBoton = millis();
  }

  if ((millis() - tiempoUltimoCambioBoton) > debounceDelay) {
    if (lecturaBoton != valorBotonActual) {
      valorBotonActual = lecturaBoton;

      if (valorBotonActual == LOW)
      { // Botón presionado (LOW para INPUT_PULLUP)
        // Lógica de transición de estado al presionar el botón
        if (estadoActualAlarma == ALARMA_DESACTIVADA || estadoActualAlarma == ERROR_PIR_ARMADO) {
          // Si está desactivada O en error por PIR, intenta armar/reiniciar
          Serial.println("Intentando armar alarma. Verificando PIR...");
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Verificando PIR");
          lcd.setCursor(0, 1);
          lcd.print("para armar...");
          delay(1000); // Muestra el mensaje por 1 segundo

          // Con la resistencia pulldown, si verificarConexionPIR() devuelve false,
          // es muy probable que el PIR no esté conectado o esté defectuoso.
          if (verificarConexionPIR()) {
            estadoActualAlarma = ALARMA_ARMANDO;      // Si el PIR responde, armar normalmente
            tiempoInicioArmado = millis();            // Guarda el tiempo de inicio del armado
            Serial.println("Alarma: Iniciando armado (5s de retardo)");
            lcdNecesitaActualizar = true;             // Forzar actualización
            tone(pinBuzzer, 800, 100); // Tono de "Adios"
          } else {
            estadoActualAlarma = ERROR_PIR_ARMADO; // Si no responde, ir a estado de error
            Serial.println("ERROR: PIR no detectado al intentar armar la alarma.");
            lcdNecesitaActualizar = true; // Forzar actualización del error
            // El tono de error se maneja en el propio case de ERROR_PIR_ARMADO
          }
        } else if (estadoActualAlarma == ALARMA_ACTIVADA || estadoActualAlarma == ALARMA_DISPARADA) {
          // Si está activa o disparada, la desactiva
          estadoActualAlarma = ALARMA_DESACTIVADA; // Desactivar desde Activa o Disparada
          Serial.println("Alarma: DESACTIVADA manualmente.");
          lcdNecesitaActualizar = true;             // Forzar actualización
          tone(pinBuzzer, 1200, 100); // Tono de "Bienvenido"
        }

        // Asegurar que el buzzer esté apagado después del tono corto
        noTone(pinBuzzer);
        digitalWrite(pinBuzzer, LOW);
        buzzerCompletadoCiclo = false; // Resetear banderas del buzzer
        conteoPulsosBuzzer = 0;       // Resetear contador de pulsos
      }
    }
  }
  valorBotonAnterior = lecturaBoton; // Guardar el estado actual para la próxima iteración

  // --- 2. Lógica principal de la alarma basada en estados ---
  switch (estadoActualAlarma) {
    case ALARMA_DESACTIVADA:
      if (lcdNecesitaActualizar) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("!! DESACTIVADA !!");
        lcd.setCursor(0, 1);
        lcd.print(" Disparos: ");
        lcd.print(contadorDisparosAlarma); // Muestra el contador
        lcdNecesitaActualizar = false;
        movimientoDetectadoAnterior = false; // Resetear estado de movimiento
      }
      break;

    case ALARMA_ARMANDO:
      if ((millis() - tiempoInicioArmado) < duracionRetardoArmado) {
        int segundosRestantes = (duracionRetardoArmado - (millis() - tiempoInicioArmado)) / 1000 + 1;
        if (lcdNecesitaActualizar) {
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("!ARMANDO ALARMA!");
        }
        lcd.setCursor(0, 1);
        lcd.print("   Lista en ");
        if (segundosRestantes < 10) lcd.print(" ");
        lcd.print(segundosRestantes);
        lcd.print("s ");
        lcdNecesitaActualizar = false;

      } else {
        estadoActualAlarma = ALARMA_ACTIVADA; // El retardo ha terminado, alarma activa
        tiempoInicioAlarmaActiva = millis(); // Registrar el tiempo de activación real
        lcdNecesitaActualizar = true;         // Forzar actualización
        Serial.println("Alarma: ACTIVADA (retardo finalizado).");
      }
      break;

    case ALARMA_ACTIVADA:
      if ((millis() - tiempoInicioAlarmaActiva) >= duracionAlarmaAutomatica) {
        estadoActualAlarma = ALARMA_DESACTIVADA;
        lcdNecesitaActualizar = true;
        Serial.println("Alarma: DESACTIVADA AUTOMATICAMENTE (5 minutos transcurridos).");
        break;
      }

      // Con la resistencia pulldown, un LOW aquí es confiable para "Sin movimiento".
      movimientoDetectadoActual = (digitalRead(pinPIR) == HIGH);

      if (movimientoDetectadoActual) {
        estadoActualAlarma = ALARMA_DISPARADA;
        lcdNecesitaActualizar = true;
        Serial.println("Alarma: MOVIMIENTO DETECTADO. ALARMA DISPARADA.");
        tiempoUltimoMovimientoReal = millis();
        contadorDisparosAlarma++;
      } else {
        if (lcdNecesitaActualizar || (movimientoDetectadoAnterior && !movimientoDetectadoActual) ) {
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Alarma ACTIVADA ");
          lcd.setCursor(0, 1);
          lcd.print("Sin movimiento");
          lcdNecesitaActualizar = false;
        }
      }
      movimientoDetectadoAnterior = movimientoDetectadoActual;
      break;

    case ALARMA_DISPARADA:
      movimientoDetectadoActual = (digitalRead(pinPIR) == HIGH);

      if (lcdNecesitaActualizar) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("!! MOVIMIENTO !!");
        lcd.setCursor(0, 1);
        lcd.print("ALARMA SONANDO!");
        lcdNecesitaActualizar = false;
        conteoPulsosBuzzer = 0;
        buzzerCompletadoCiclo = false;
        tiempoUltimaAlternacionBuzzer = millis();
      }

      if (!buzzerCompletadoCiclo) {
        if ((millis() - tiempoUltimaAlternacionBuzzer) >= duracionPulsoBuzzer) {
          if (conteoPulsosBuzzer < maxPulsosBuzzer) {
            if (conteoPulsosBuzzer % 2 == 0) {
              tone(pinBuzzer, 1200);
            } else {
              noTone(pinBuzzer);
            }
            conteoPulsosBuzzer++;
            tiempoUltimaAlternacionBuzzer = millis();
          } else {
            noTone(pinBuzzer);
            buzzerCompletadoCiclo = true;
          }
        }
      }

      if (!movimientoDetectadoActual && (millis() - tiempoUltimoMovimientoReal) > tiempoEsperaSinMovimiento) {
            estadoActualAlarma = ALARMA_ACTIVADA;
            lcdNecesitaActualizar = true;
            noTone(pinBuzzer);
            digitalWrite(pinBuzzer, LOW);
            conteoPulsosBuzzer = 0;
            buzzerCompletadoCiclo = false;
            Serial.println("Alarma: Movimiento cesó, volviendo a estado ACTIVADA.");
      }
      movimientoDetectadoAnterior = movimientoDetectadoActual;
      break;

    case ERROR_PIR_ARMADO:
      // Cuando estamos en este estado, mostramos el error y hacemos sonar el buzzer intermitentemente
      // pero permitimos que el loop continúe para que el botón pueda ser leído y la alarma desactivada.
      if (lcdNecesitaActualizar) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("ERROR: PIR NO");
        lcd.setCursor(0, 1);
        lcd.print("DETECTADO AL ARMAR");
        lcdNecesitaActualizar = false;
      }

      // Buzzer intermitente para el error (no bloqueante)
      if ((millis() - tiempoUltimaAlternacionBuzzer) >= 500) { // Tono de error cada 0.5s
        if (digitalRead(pinBuzzer) == LOW) { // Si el buzzer está apagado, enciéndalo
          tone(pinBuzzer, 800);
        } else { // Si está encendido, apágalo
          noTone(pinBuzzer);
        }
        tiempoUltimaAlternacionBuzzer = millis();
      }
      break;
  }

  // Si la alarma se desactiva manualmente o automáticamente, asegurar que el buzzer esté apagado
  // Esto también cubre la salida del estado ERROR_PIR_ARMADO si se presiona el botón.
  if (estadoActualAlarma == ALARMA_DESACTIVADA) {
      noTone(pinBuzzer);
      digitalWrite(pinBuzzer, LOW);
      buzzerCompletadoCiclo = false;
      conteoPulsosBuzzer = 0;
  }
}