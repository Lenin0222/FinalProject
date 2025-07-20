#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

const int pinPIR = 2;
const int pinBuzzer = 8;
const int pinBoton = 4;

LiquidCrystal_I2C lcd(0x27, 16, 2);

bool alarmaActiva = true;
bool movimientoDetectado = false;
bool lcdNecesitaActualizar = true;

int valorBotonActual = HIGH;
int valorBotonAnterior = HIGH;
unsigned long tiempoUltimoCambio = 0;
const unsigned long debounceDelay = 50;

unsigned long tiempoUltimaActivacionBuzzer = 0;
const long intervaloBuzzer = 250;

unsigned long tiempoUltimoMovimiento = 0;
const unsigned long tiempoEsperaSinMovimiento = 5000;  // 5 segundos

void setup() {
  Serial.begin(9600);
  pinMode(pinPIR, INPUT);
  pinMode(pinBuzzer, OUTPUT);
  pinMode(pinBoton, INPUT_PULLUP);

  lcd.init();
  lcd.backlight();

  lcd.setCursor(0, 0);
  lcd.print("Sistema");
  lcd.setCursor(0, 1);
  lcd.print("Iniciando...");
  delay(2000);
  lcd.clear();
  lcdNecesitaActualizar = true;
  tiempoUltimoMovimiento = millis(); // Inicializamos tiempo
}

void loop() 
{
  // --- 1. Lectura y Debounce del Botón ---
  int lecturaBoton = digitalRead(pinBoton);

  if (lecturaBoton != valorBotonAnterior) {
    tiempoUltimoCambioBoton = millis();
  }

  if ((millis() - tiempoUltimoCambioBoton) > debounceDelay) {
    if (lecturaBoton != valorBotonActual) {
      valorBotonActual = lecturaBoton;

      if (valorBotonActual == LOW) { // Botón presionado (LOW para INPUT_PULLUP)
        // Lógica de transición de estado al presionar el botón
        if (estadoActualAlarma == ALARMA_DESACTIVADA || estadoActualAlarma == ALARMA_ACTIVADA || estadoActualAlarma == ALARMA_DISPARADA) {
          if (estadoActualAlarma == ALARMA_DESACTIVADA) {
            estadoActualAlarma = ALARMA_ARMANDO; // Inicia el retardo de armado
            tiempoInicioArmado = millis();      // Guarda el tiempo de inicio del armado
            Serial.println("Alarma: Iniciando armado (5s de retardo)");
          } else { // Si está activada o disparada, se desactiva
            estadoActualAlarma = ALARMA_DESACTIVADA;
            Serial.println("Alarma: DESACTIVADA manualmente.");
          }
        }
        
        lcdNecesitaActualizar = true; // Forzar actualización LCD para el nuevo estado
        digitalWrite(pinBuzzer, LOW); // Asegurar que el buzzer esté apagado
        buzzerCompletadoCiclo = false; // Resetear banderas del buzzer
        conteoPulsosBuzzer = 0;       // Resetear contador de pulsos
      }
    }
  }
  valorBotonAnterior = lecturaBoton;

  // --- 2. Lógica principal de la alarma basada en estados ---
  switch (estadoActualAlarma) {
    case ALARMA_DESACTIVADA:
      if (lcdNecesitaActualizar) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("!! DESACTIVADA !!");
        lcd.setCursor(0, 1);
        lcd.print(" Pulsa el boton");
        lcdNecesitaActualizar = false;
        movimientoDetectadoAnterior = false; // Resetear estado de movimiento
      }
      break;

    case ALARMA_ARMANDO:
      // Control de la cuenta regresiva para el armado
      if ((millis() - tiempoInicioArmado) < duracionRetardoArmado) {
        int segundosRestantes = (duracionRetardoArmado - (millis() - tiempoInicioArmado)) / 1000 + 1; // +1 para que cuente 5,4,3,2,1
        if (lcdNecesitaActualizar) { // Opcional: para evitar parpadeo constante, solo si el LCD necesita actualizarse
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("!ARMANDO ALARMA!");
        }
        lcd.setCursor(0, 1);
        lcd.print("   Espere ");
        if (segundosRestantes < 10) lcd.print(" "); // Formato
        lcd.print(segundosRestantes);
        lcd.print("s ");
        lcdNecesitaActualizar = false; // Ya que estamos actualizando constantemente

      } else {
        estadoActualAlarma = ALARMA_ACTIVADA; // El retardo ha terminado, alarma activa
        tiempoInicioAlarmaActiva = millis(); // Registrar el tiempo de activación real
        lcdNecesitaActualizar = true; // Forzar actualización al pasar a ACTIVA
        Serial.println("Alarma: ACTIVADA (retardo finalizado).");
      }
      break;

    case ALARMA_ACTIVADA:
      // --- Desactivación automática después de 5 minutos ---
      if ((millis() - tiempoInicioAlarmaActiva) >= duracionAlarmaAutomatica) {
        estadoActualAlarma = ALARMA_DESACTIVADA; // Desactivar la alarma
        lcdNecesitaActualizar = true; // Forzar actualización del LCD
        Serial.println("Alarma: DESACTIVADA AUTOMATICAMENTE (5 minutos transcurridos).");
        break; // Salir de este case para que no detecte movimiento
      }

      movimientoDetectadoActual = (digitalRead(pinPIR) == HIGH); // Leer PIR solo cuando está activa

      if (movimientoDetectadoActual) { // Movimiento detectado
        tiempoUltimoMovimientoReal = millis(); // Actualiza tiempo de último movimiento
        estadoActualAlarma = ALARMA_DISPARADA; // Pasar a estado de disparo
        lcdNecesitaActualizar = true; // Forzar actualización LCD para nuevo estado
        Serial.println("Alarma: MOVIMIENTO DETECTADO. ALARMA DISPARADA.");
      } else { // No hay movimiento detectado
        if (lcdNecesitaActualizar || (movimientoDetectadoAnterior && !movimientoDetectadoActual) ) {
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Alarma ACTIVADA ");
          lcd.setCursor(0, 1);
          lcd.print("Sin movimiento");
          lcdNecesitaActualizar = false;
        }
      }
      movimientoDetectadoAnterior = movimientoDetectadoActual; // Actualizar para la próxima iteración
      break;

    case ALARMA_DISPARADA:
      movimientoDetectadoActual = (digitalRead(pinPIR) == HIGH); // Continuar leyendo PIR

      if (lcdNecesitaActualizar) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("!! MOVIMIENTO !!");
        lcd.setCursor(0, 1);
        lcd.print("ALARMA SONANDO!"); // Mensaje más claro
        lcdNecesitaActualizar = false;
      }

      // Control del buzzer intermitente (solo si no ha completado los 5 pitidos)
      if (!buzzerCompletadoCiclo) {
        if (conteoPulsosBuzzer < maxPulsosBuzzer) {
          if ((millis() - tiempoUltimaAlternacionBuzzer) > duracionPulsoBuzzer) {
            digitalWrite(pinBuzzer, !digitalRead(pinBuzzer));
            tiempoUltimaAlternacionBuzzer = millis();
            conteoPulsosBuzzer++;

            if (conteoPulsosBuzzer >= maxPulsosBuzzer) {
              digitalWrite(pinBuzzer, LOW);
              buzzerCompletadoCiclo = true;
              Serial.println("Buzzer completo 5 pitidos.");
            }
          }
        } else { // Si conteoPulsosBuzzer >= maxPulsosBuzzer pero buzzerCompletadoCiclo es false (redundancia de seguridad)
            digitalWrite(pinBuzzer, LOW);
            buzzerCompletadoCiclo = true;
        }
      } else { // Si el buzzer ya completó su ciclo, pero la alarma sigue disparada (ej. si hay movimiento continuo)
        digitalWrite(pinBuzzer, LOW); // Asegurarse de que el buzzer esté APAGADO
        // Podríamos volver al estado ALARMA_ACTIVADA si no hay movimiento
        // O quedarse en DISPARADA hasta que se desactive manualmente
        if (!movimientoDetectadoActual && (millis() - tiempoUltimoMovimientoReal) > tiempoEsperaSinMovimiento) {
             estadoActualAlarma = ALARMA_ACTIVADA; // Si ya no hay movimiento, vuelve al estado activa
             lcdNecesitaActualizar = true;
             conteoPulsosBuzzer = 0; // Reiniciar para el próximo disparo
             buzzerCompletadoCiclo = false;
             Serial.println("Alarma: Movimiento cesó, volviendo a estado ACTIVADA.");
        }
      }
      movimientoDetectadoAnterior = movimientoDetectadoActual;
      break;
  }

  // Si la alarma se desactiva manualmente o automáticamente, asegurar el estado inicial
  if (estadoActualAlarma == ALARMA_DESACTIVADA) {
      digitalWrite(pinBuzzer, LOW);
      buzzerCompletadoCiclo = false;
      conteoPulsosBuzzer = 0;
  }
}
