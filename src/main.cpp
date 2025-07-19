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

void loop() {
  int lectura = digitalRead(pinBoton);

  if (lectura != valorBotonAnterior) {
    tiempoUltimoCambio = millis();
  }

  if ((millis() - tiempoUltimoCambio) > debounceDelay) {
    if (lectura != valorBotonActual) {
      valorBotonActual = lectura;

      if (valorBotonActual == LOW) {
        alarmaActiva = !alarmaActiva;
        lcdNecesitaActualizar = true;
        digitalWrite(pinBuzzer, LOW);
        Serial.println(alarmaActiva ? "Alarma ACTIVADA" : "Alarma DESACTIVADA");
      }
    }
  }

  valorBotonAnterior = lectura;

  int estadoPIR = digitalRead(pinPIR);
  movimientoDetectado = (estadoPIR == HIGH);

  if (alarmaActiva) {
    if (movimientoDetectado) {
      // Actualizamos tiempo último movimiento detectado
      tiempoUltimoMovimiento = millis();

      // Sonido buzzer intermitente
      if ((millis() - tiempoUltimaActivacionBuzzer) > intervaloBuzzer) {
        digitalWrite(pinBuzzer, !digitalRead(pinBuzzer));
        tiempoUltimaActivacionBuzzer = millis();
      }

      // Mostrar mensaje movimiento
      lcd.setCursor(0, 0);
      lcd.print("!! MOVIMIENTO !!");
      lcd.setCursor(0, 1);
      lcd.print("Alarma ACTIVADA   ");

      lcdNecesitaActualizar = true; // para actualizar cuando no haya movimiento
    } else {
      digitalWrite(pinBuzzer, LOW);
      // Si ya pasó el tiempo sin movimiento, actualizamos el mensaje
      if ((millis() - tiempoUltimoMovimiento) > tiempoEsperaSinMovimiento) {
        if (lcdNecesitaActualizar) {
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Alarma ACTIVADA");
          lcd.setCursor(0, 1);
          lcd.print("Sin movimiento");
          lcdNecesitaActualizar = false;
        }
      }
    }
  } else {
    digitalWrite(pinBuzzer, LOW);
    if (lcdNecesitaActualizar) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(" !!DESACTIVADA!!");
      lcd.setCursor(0, 1);
      lcd.print(" Pulse el boton");
      lcdNecesitaActualizar = false;
    }
  }
}
