#include <LiquidCrystal.h>
#include "AsyncTaskLib.h"
#include "DHT.h"
#include "StateMachineLib.h"
#include <Keypad.h>
#include <SPI.h>
#include <MFRC522.h>

// ==================== CONFIGURACIÓN HARDWARE ====================
namespace Hardware {
  // DHT Sensor
  const int DHT_PIN = 22;
  const int DHT_TYPE = DHT11;
  
  // LEDs
  const int LED_RED = 46;
  const int LED_BLUE = 17;
  const int LED_GREEN = 48;
  
  // Otros componentes
  const int LIGHT_SENSOR_PIN = A0;
  const int BUZZER_PIN = 8;
  const int RELAY_PIN = 9;
  
  // RFID
  const int RFID_RST_PIN = 19;
  const int RFID_SS_PIN = 53;
  
  // LCD
  const int LCD_RS = 12, LCD_EN = 11;
  const int LCD_D4 = 5, LCD_D5 = 4, LCD_D6 = 3, LCD_D7 = 2;
  
  // Teclado
  const byte KEYPAD_ROWS = 4;
  const byte KEYPAD_COLS = 4;
  byte keypadRowPins[KEYPAD_ROWS] = {30, 32, 34, 36};
  byte keypadColPins[KEYPAD_COLS] = {38, 40, 42, 44};
  char keypadKeys[KEYPAD_ROWS][KEYPAD_COLS] = {
    {'1','2','3','A'},
    {'4','5','6','B'},
    {'7','8','9','C'},
    {'*','0','#','D'}
  };
}

// ==================== CONFIGURACIÓN DEL SISTEMA ====================
namespace Config {
  const char CORRECT_PASSWORD[] = "1234";  // Contraseña actualizada según descripción
  const int MAX_PASSWORD_LENGTH = 8;
  const int MAX_LOGIN_ATTEMPTS = 3;
  const int MAX_ALARMS = 3;
  
  // Umbrales de sensores
  const float HIGH_TEMP_THRESHOLD = 40.0;
  const float ALARM_TEMP_THRESHOLD = 40.0;  
  const int LOW_LIGHT_THRESHOLD = 10;
  
  // Timeouts (milisegundos)
  const unsigned long STABILIZATION_TIME = 3000;
  const unsigned long ALARM_DURATION = 3000;
  const unsigned long PMV_HIGH_DURATION = 7000;
  const unsigned long PMV_LOW_DURATION = 4000;
  const unsigned long FAN_ON_TIME = 10000;
  const unsigned long FAN_OFF_TIME = 10000;
  const unsigned long BUZZER_INTERVAL = 150;
  const unsigned long LOGIN_SUCCESS_LED_TIME = 2000;
  const unsigned long LOGIN_ERROR_BLINK_TIME = 3000;
  

}

// ==================== ENUMERACIONES ====================
enum SystemState {
  STATE_INIT = 0,
  STATE_SECURITY,
  STATE_MONITORING,
  STATE_ALARM,
  STATE_BLOCKED,
  STATE_PMV_HIGH,
  STATE_PMV_LOW,
  STATE_LOGIN_SUCCESS,
  STATE_LOGIN_ERROR
};

enum SystemInput {
  INPUT_TIMEOUT = 0,
  INPUT_CORRECT_PASSWORD,
  INPUT_INCORRECT_PASSWORD,
  INPUT_RESET,
  INPUT_TEMP_HIGH,
  INPUT_TEMP_LOW,
  INPUT_LOGIN_SUCCESS_COMPLETE,
  INPUT_LOGIN_ERROR_COMPLETE,
  INPUT_UNKNOWN
};

// ==================== CLASES DE GESTIÓN ====================

class SensorManager {
private:
  DHT dht;
  float temperature;
  float humidity;
  int lightValue;
  unsigned long lastReadTime;
  
public:
  SensorManager() : dht(Hardware::DHT_PIN, Hardware::DHT_TYPE), 
                    temperature(0), humidity(0), lightValue(0), lastReadTime(0) {}
  
  void begin() {
    dht.begin();
  }
  
  void updateTemperature() {
    float temp = dht.readTemperature();
    if (!isnan(temp)) {
      temperature = temp;
      Serial.print("Temperatura: ");
      Serial.println(temperature);
    }
  }
  
  void updateHumidity() {
    float hum = dht.readHumidity();
    if (!isnan(hum)) {
      humidity = hum;
      Serial.print("Humedad: ");
      Serial.println(humidity);
    }
  }
  
  void updateLight() {
    lightValue = analogRead(Hardware::LIGHT_SENSOR_PIN);
    Serial.print("Luz: ");
    Serial.println(lightValue);
  }
  
  float getTemperature() const { return temperature; }
  float getHumidity() const { return humidity; }
  int getLightValue() const { return lightValue; }
  
  bool isHighTemperature() const { return temperature > Config::HIGH_TEMP_THRESHOLD; }
  bool isAlarmTemperature() const { return temperature > Config::ALARM_TEMP_THRESHOLD; }
  bool isLowLight() const { return lightValue < Config::LOW_LIGHT_THRESHOLD; }
};

class LEDManager {
private:
  bool redState, blueState, greenState;
  unsigned long lastToggleTime;
  unsigned long greenStartTime;
  unsigned long blueBlinkStartTime;
  bool greenActive, blueBlinkActive;
  
public:
  LEDManager() : redState(false), blueState(false), greenState(false),
                 lastToggleTime(0), greenStartTime(0), blueBlinkStartTime(0),
                 greenActive(false), blueBlinkActive(false) {}
  
  void begin() {
    pinMode(Hardware::LED_RED, OUTPUT);
    pinMode(Hardware::LED_BLUE, OUTPUT);
    pinMode(Hardware::LED_GREEN, OUTPUT);
    turnOffAll();
  }
  
  void turnOffAll() {
    digitalWrite(Hardware::LED_RED, LOW);
    digitalWrite(Hardware::LED_BLUE, LOW);
    digitalWrite(Hardware::LED_GREEN, LOW);
    redState = blueState = greenState = false;
    greenActive = blueBlinkActive = false;
  }
  
  void setRed(bool state) {
    digitalWrite(Hardware::LED_RED, state ? HIGH : LOW);
    redState = state;
  }
  
  void setBlue(bool state) {
    digitalWrite(Hardware::LED_BLUE, state ? HIGH : LOW);
    blueState = state;
  }
  
  void setGreen(bool state) {
    digitalWrite(Hardware::LED_GREEN, state ? HIGH : LOW);
    greenState = state;
  }
  
  void startLoginSuccess() {
    greenActive = true;
    greenStartTime = millis();
    setGreen(true);
  }
  
  void startLoginError() {
    blueBlinkActive = true;
    blueBlinkStartTime = millis();
    lastToggleTime = millis();
    setBlue(true);
  }
  
  void blinkRed(int interval = 300) {
    if (millis() - lastToggleTime >= interval) {
      redState = !redState;
      setRed(redState);
      lastToggleTime = millis();
    }
  }
  
  void blinkRedFast() {
    blinkRed(100);
  }
  
  void blinkBlue(int interval = 300) {
    if (millis() - lastToggleTime >= interval) {
      blueState = !blueState;
      setBlue(blueState);
      lastToggleTime = millis();
    }
  }
  
  void update() {
    // Gestionar LED verde de login exitoso
    if (greenActive && (millis() - greenStartTime >= Config::LOGIN_SUCCESS_LED_TIME)) {
      setGreen(false);
      greenActive = false;
    }
    
    // Gestionar parpadeo azul de error de login
    if (blueBlinkActive) {
      if (millis() - blueBlinkStartTime >= Config::LOGIN_ERROR_BLINK_TIME) {
        setBlue(false);
        blueBlinkActive = false;
      } else {
        blinkBlue(200);
      }
    }
  }
  
  bool isLoginSuccessComplete() const {
    return !greenActive; // Cambiar la lógica
  }

  bool isLoginErrorComplete() const {
    return !blueBlinkActive; // Cambiar la lógica
  }

};

class SecurityManager {
private:
  char enteredPassword[Config::MAX_PASSWORD_LENGTH + 1];
  int passwordIndex;
  int loginAttempts;
  bool resetPressed;
  
public:
  SecurityManager() : passwordIndex(0), loginAttempts(0), resetPressed(false) {
    clearPassword();
  }
  
  void clearPassword() {
    memset(enteredPassword, 0, sizeof(enteredPassword));
    passwordIndex = 0;
  }
  
  void reset() {
    clearPassword();
    loginAttempts = 0;
    resetPressed = false; // Importante: limpiar la bandera de reset
  }

  
  bool addDigit(char digit) {
    if (passwordIndex < Config::MAX_PASSWORD_LENGTH && digit >= '0' && digit <= '9') {
      enteredPassword[passwordIndex++] = digit;
      return true;
    }
    return false;
  }
  
  bool verifyPassword() {
    enteredPassword[passwordIndex] = '\0';
    bool correct = (strcmp(enteredPassword, Config::CORRECT_PASSWORD) == 0);
    
    if (!correct) {
      loginAttempts++;
    } else {
      loginAttempts = 0;
    }
    
    return correct;
  }
  
  void setResetPressed() { resetPressed = true; }
  bool isResetPressed() const { return resetPressed; }
  bool isBlocked() const { return loginAttempts >= Config::MAX_LOGIN_ATTEMPTS; }
  int getAttempts() const { return loginAttempts; }
  char* getCurrentPassword() { return enteredPassword; }
  int getPasswordLength() const { return passwordIndex; }
};

class AlarmManager {
private:
  int alarmCount;
  bool alarmBlocked;
  unsigned long alarmStartTime;
  unsigned long stabilizationTime;
  
public:
  AlarmManager() : alarmCount(0), alarmBlocked(true), alarmStartTime(0), stabilizationTime(0) {}
  
  void reset() {
    alarmCount = 0;
    alarmBlocked = true;
    alarmStartTime = 0;
  }
  
  void startStabilization() {
    alarmBlocked = true;
    stabilizationTime = millis();
  }
  
  void triggerAlarm() {
    alarmCount++;
    alarmStartTime = millis();
    Serial.print("Alarma activada. Contador: ");
    Serial.println(alarmCount);
  }
  
  void update() {
    if (alarmBlocked && (millis() - stabilizationTime >= Config::STABILIZATION_TIME)) {
      alarmBlocked = false;
    }
  }
  
  bool isStabilized() const {
    return !alarmBlocked || (millis() - stabilizationTime >= Config::STABILIZATION_TIME);
  }
  
  bool isAlarmTimeout() const {
    return (millis() - alarmStartTime >= Config::ALARM_DURATION);
  }
  
  bool isMaxAlarmsReached() const { return alarmCount >= Config::MAX_ALARMS; }
  int getAlarmCount() const { return alarmCount; }
  bool isBlocked() const { return alarmBlocked; }
};

class DisplayManager {
private:
  LiquidCrystal lcd;
  
public:
  DisplayManager() : lcd(Hardware::LCD_RS, Hardware::LCD_EN, 
                        Hardware::LCD_D4, Hardware::LCD_D5, 
                        Hardware::LCD_D6, Hardware::LCD_D7) {}
  
  void begin() {
    lcd.begin(16, 2);
  }
  
  void clear() {
    lcd.clear();
  }
  
  void showWelcome() {
    clear();
    lcd.print("Sistema de");
    lcd.setCursor(0, 1);
    lcd.print("monitoreo");
  }
  
  void showSecurityPrompt() {
    clear();
    lcd.print("Ingrese clave:");
    lcd.setCursor(0, 1);
  }
  
  void showPasswordMask(int length) {
    lcd.setCursor(0, 1);
    for (int i = 0; i < 16; i++) {
      lcd.print(i < length ? "*" : " ");
    }
  }
  
  void showCorrectPassword() {
    clear();
    lcd.print("Clave correcta");
  }
  
  void showIncorrectPassword(int attempts) {
    clear();
    lcd.print("Clave incorrecta");
    lcd.setCursor(0, 1);
    lcd.print("Intentos: ");
    lcd.print(attempts);
  }
  
  void showStabilizing() {
    clear();
    lcd.print("Estabilizando");
    lcd.setCursor(0, 1);
    lcd.print("sensores...");
  }
  
  void showMonitoringData(float temp, float humidity, int light) {
    clear();
    lcd.print("T:");
    lcd.print(temp);
    lcd.print((char)223);
    lcd.print("C H:");
    lcd.print(humidity);
    lcd.print("%");
    lcd.setCursor(0, 1);
    lcd.print("Luz:");
    lcd.print(light);
  }
  
  void showAlarm(int alarmNum, int maxAlarms) {
    clear();
    lcd.print("ALARMA! ");
    lcd.print(alarmNum);
    lcd.print("/");
    lcd.print(maxAlarms);
    lcd.setCursor(0, 1);
    lcd.print("Temp o luz alta");
  }
  
  void showBlocked(bool byAlarms = false) {
    clear();
    lcd.print("SISTEMA");
    lcd.setCursor(0, 1);
    lcd.print("BLOQUEADO");
    delay(2000);
    
    clear();
    if (byAlarms) {
      lcd.print("Exceso de");
      lcd.setCursor(0, 1);
      lcd.print("alarmas!");
    } else {
      lcd.print("Presione # para");
      lcd.setCursor(0, 1);
      lcd.print("reiniciar");
    }
  }
  
  void showPMVHigh(bool fanOn) {
    clear();
    lcd.print("PMV ALTO ");
    lcd.print(fanOn ? "ON" : "OFF");
    lcd.setCursor(0, 1);
    lcd.print("VENTILADOR");
  }
  
  void showPMVLow() {
    clear();
    lcd.print("PMV BAJO");
    lcd.setCursor(0, 1);
    lcd.print("LUZ ON");
  }
};

class FanController {
private:
  bool fanOn;
  unsigned long lastToggleTime;
  
public:
  FanController() : fanOn(false), lastToggleTime(0) {}
  
  void begin() {
    pinMode(Hardware::RELAY_PIN, OUTPUT);
    turnOff();
  }
  
  void turnOff() {
    digitalWrite(Hardware::RELAY_PIN, LOW);
    fanOn = false;
  }
  
  void update() {
    unsigned long currentTime = millis();
    unsigned long interval = fanOn ? Config::FAN_ON_TIME : Config::FAN_OFF_TIME;
    
    if (currentTime - lastToggleTime >= interval) {
      fanOn = !fanOn;
      digitalWrite(Hardware::RELAY_PIN, fanOn ? HIGH : LOW);
      lastToggleTime = currentTime;
      Serial.print("Ventilador ");
      Serial.println(fanOn ? "ENCENDIDO" : "APAGADO");
    }
  }
  
  bool isOn() const { return fanOn; }
};

class BuzzerController {
private:
  bool buzzerState;
  unsigned long lastToggleTime;
  
public:
  BuzzerController() : buzzerState(false), lastToggleTime(0) {}
  
  void begin() {
    pinMode(Hardware::BUZZER_PIN, OUTPUT);
    turnOff();
  }
  
  void turnOff() {
    noTone(Hardware::BUZZER_PIN);
    buzzerState = false;
  }
  
  void update() {
    if (millis() - lastToggleTime >= Config::BUZZER_INTERVAL) {
      lastToggleTime = millis();
      buzzerState = !buzzerState;
      tone(Hardware::BUZZER_PIN, buzzerState ? 1000 : 1500);
    }
  }
};



class RFIDManager {
private:
  MFRC522 mfrc522;
  MFRC522::MIFARE_Key key;
  
public:
  RFIDManager() : mfrc522(Hardware::RFID_SS_PIN, Hardware::RFID_RST_PIN) {}
  
  void begin() {
    SPI.begin();
    mfrc522.PCD_Init();
    
    // Preparar la clave por defecto (generalmente es FF FF FF FF FF FF)
    for (byte i = 0; i < 6; i++) {
      key.keyByte[i] = 0xFF;
    }
  }
  
  SystemInput checkCards() {
    if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
      return INPUT_UNKNOWN;
    }
    
    // Seleccionar la tarjeta
    if (mfrc522.PICC_GetType(mfrc522.uid.sak) != MFRC522::PICC_TYPE_MIFARE_MINI &&
        mfrc522.PICC_GetType(mfrc522.uid.sak) != MFRC522::PICC_TYPE_MIFARE_1K &&
        mfrc522.PICC_GetType(mfrc522.uid.sak) != MFRC522::PICC_TYPE_MIFARE_4K) {
      mfrc522.PICC_HaltA();
      return INPUT_UNKNOWN;
    }
    
    // Autenticar usando la clave A
    byte status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, 6, &key, &(mfrc522.uid));
    if (status != MFRC522::STATUS_OK) {
      Serial.print("Fallo en autenticación: ");
      Serial.println(mfrc522.GetStatusCodeName(status));
      mfrc522.PICC_HaltA();
      return INPUT_UNKNOWN;
    }
    
    // Leer el bloque 6
    byte buffer[18];
    byte size = sizeof(buffer);
    status = mfrc522.MIFARE_Read(6, buffer, &size);
    
    SystemInput result = INPUT_UNKNOWN;
    
    if (status == MFRC522::STATUS_OK) {
      // Verificar el primer byte del bloque 6
      if (buffer[0] == 1) {
        Serial.println("Tarjeta PMV_ALTO detectada (valor: 1)");
        result = INPUT_TEMP_HIGH;
      } else if (buffer[0] == 2) {
        Serial.println("Tarjeta PMV_BAJO detectada (valor: 2)");
        result = INPUT_TEMP_LOW;
      } else {
        Serial.print("Valor desconocido en bloque 6: ");
        Serial.println(buffer[0]);
      }
    } else {
      Serial.print("Error leyendo bloque: ");
      Serial.println(mfrc522.GetStatusCodeName(status));
    }
    
    // Detener la comunicación con la tarjeta
    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
    
    return result;
  }


  //METODO PARA ESCRIBIR LOS VALORES A LAS TARGETAS
  bool writeValueToCard(byte value) {
    if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
      return false;
    }
    
    // Autenticar
    byte status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, 6, &key, &(mfrc522.uid));
    if (status != MFRC522::STATUS_OK) {
      mfrc522.PICC_HaltA();
      return false;
    }
    
    // Preparar datos (16 bytes, solo el primero tiene el valor)
    byte dataBlock[16] = {0};
    dataBlock[0] = value;
    
    // Escribir el bloque
    status = mfrc522.MIFARE_Write(6, dataBlock, 16);
    bool success = (status == MFRC522::STATUS_OK);
    
    if (success) {
      Serial.print("Valor ");
      Serial.print(value);
      Serial.println(" escrito correctamente en bloque 6");
    } else {
      Serial.print("Error escribiendo: ");
      Serial.println(mfrc522.GetStatusCodeName(status));
    }
    
    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
    
    return success;
  }
};

// ==================== INSTANCIAS GLOBALES ====================
SensorManager sensors;
LEDManager leds;
SecurityManager security;
AlarmManager alarms;
DisplayManager display;
FanController fan;
BuzzerController buzzer;
RFIDManager rfid;

Keypad keypad = Keypad(makeKeymap(Hardware::keypadKeys), 
                      Hardware::keypadRowPins, Hardware::keypadColPins, 
                      Hardware::KEYPAD_ROWS, Hardware::KEYPAD_COLS);

StateMachine stateMachine(9, 25);
SystemInput currentInput = INPUT_UNKNOWN;
unsigned long stateStartTime = 0;

// ==================== TAREAS ASÍNCRONAS ====================
AsyncTask taskSensors(1000, true, []() {
  sensors.updateTemperature();
  sensors.updateHumidity();
  sensors.updateLight();
});

AsyncTask taskDisplay(1500, true, []() {
  SystemState state = (SystemState)stateMachine.GetState();
  Serial.println(state);
  switch (state) {
    case STATE_MONITORING:
      if (alarms.isStabilized()) {
        display.showMonitoringData(sensors.getTemperature(), 
                                 sensors.getHumidity(), 
                                 sensors.getLightValue());
      }
      break;
    case STATE_PMV_HIGH:
      display.showPMVHigh(fan.isOn());
      break;
    case STATE_PMV_LOW:
      display.showPMVLow();
      break;
  }
});


AsyncTask taskKeypad(100, true, []() {
  char key = keypad.getKey();
  if (!key) return;
  
  SystemState state = (SystemState)stateMachine.GetState();
  
  if (state == STATE_BLOCKED && key == '#') {
    Serial.println("Reset presionado - reiniciando sistema");
    security.setResetPressed();
    currentInput = INPUT_RESET; // Establecer input inmediatamente
  } else if (state == STATE_SECURITY) {
    if (key == '*') {
      if (security.verifyPassword()) {
        display.showCorrectPassword();
        leds.startLoginSuccess();
        currentInput = INPUT_CORRECT_PASSWORD;
      } else {
        display.showIncorrectPassword(security.getAttempts());
        leds.startLoginError();
        currentInput = INPUT_INCORRECT_PASSWORD;
        security.clearPassword();
      }
    } else if (key == '#') {
      security.clearPassword();
      display.showSecurityPrompt();
    } else if (security.addDigit(key)) {
      display.showPasswordMask(security.getPasswordLength());
    }
  }
});


AsyncTask taskRFID(100, true, []() {
  if (stateMachine.GetState() == STATE_MONITORING) {
    SystemInput rfidInput = rfid.checkCards();
    if (rfidInput != INPUT_UNKNOWN) {
      currentInput = rfidInput;
    }
  }
});

AsyncTask taskLEDs(50, true, []() {
  leds.update();
  
  SystemState state = (SystemState)stateMachine.GetState();
  switch (state) {
    case STATE_ALARM:
      leds.blinkRed();
      break;
    case STATE_BLOCKED:
      leds.blinkRedFast();
      break;
    case STATE_PMV_LOW:
      leds.blinkBlue();
      break;
  }
  
  // Verificar completación de animaciones de login
  if (state == STATE_LOGIN_SUCCESS && leds.isLoginSuccessComplete()) {
    currentInput = INPUT_LOGIN_SUCCESS_COMPLETE;
  } else if (state == STATE_LOGIN_ERROR && leds.isLoginErrorComplete()) {
    currentInput = INPUT_LOGIN_ERROR_COMPLETE;
  }
});

AsyncTask taskBuzzer(50, true, []() {
  if (stateMachine.GetState() == STATE_ALARM) {
    buzzer.update();
  }
});

AsyncTask taskFan(1000, true, []() {
  if (stateMachine.GetState() == STATE_PMV_HIGH) {
    fan.update();
  }
});

// ==================== FUNCIONES DE ESTADO ====================
void enterInit() {
  Serial.println("Estado: INIT");
  
  // Detener todas las tareas y componentes
  leds.turnOffAll();
  buzzer.turnOff();
  fan.turnOff();
  
  // Resetear managers
  security.reset();
  alarms.reset();
  
  display.showWelcome();
  delay(2000);
  
  // Asegurar que el input se establezca para la transición
  currentInput = INPUT_TIMEOUT;
}
void enterSecurity() {
  Serial.println("Estado: SECURITY");
  
  leds.turnOffAll();
  buzzer.turnOff();
  fan.turnOff();
  
  // Si venimos de un reset, limpiar todo
  if (security.isResetPressed()) {
    security.reset();
    alarms.reset();  // AGREGAR ESTA LÍNEA
  }
  
  display.showSecurityPrompt();
  security.clearPassword();
}
void enterLoginSuccess() {
  Serial.println("Estado: LOGIN_SUCCESS");
  // El LED verde ya fue activado en taskKeypad
}

void enterLoginError() {
  Serial.println("Estado: LOGIN_ERROR");
  // El LED azul ya fue activado en taskKeypad
}

void enterMonitoring() {
  Serial.println("Estado: MONITORING");
  
  leds.turnOffAll();
  buzzer.turnOff();
  fan.turnOff();
  
  alarms.startStabilization();
  display.showStabilizing();
  stateStartTime = millis();
}

void enterAlarm() {
  Serial.println("Estado: ALARM");
  
  leds.turnOffAll();
  buzzer.turnOff();
  fan.turnOff();
  
  alarms.triggerAlarm();
  display.showAlarm(alarms.getAlarmCount(), Config::MAX_ALARMS);
  stateStartTime = millis();
}

void enterBlocked() {
  Serial.println("Estado: BLOCKED");
  
  leds.turnOffAll();
  buzzer.turnOff();
  fan.turnOff();
  
  bool byAlarms = alarms.isMaxAlarmsReached();
  display.showBlocked(byAlarms);
}

void enterPMVHigh() {
  Serial.println("Estado: PMV_HIGH");
  
  leds.turnOffAll();
  buzzer.turnOff();
  
  display.showPMVHigh(false);
  stateStartTime = millis();
}

void enterPMVLow() {
  Serial.println("Estado: PMV_LOW");
  
  leds.turnOffAll();
  buzzer.turnOff();
  fan.turnOff();
  
  display.showPMVLow();
  stateStartTime = millis();
}

// ==================== CONFIGURACIÓN DE MÁQUINA DE ESTADOS ====================
void setupStateMachine() {
  // Transiciones desde INIT
  stateMachine.AddTransition(STATE_INIT, STATE_SECURITY, 
    []() { return currentInput == INPUT_TIMEOUT; });
  
  // Transiciones desde SECURITY
  stateMachine.AddTransition(STATE_SECURITY, STATE_LOGIN_SUCCESS, 
    []() { return currentInput == INPUT_CORRECT_PASSWORD; });
  stateMachine.AddTransition(STATE_SECURITY, STATE_LOGIN_ERROR, 
    []() { return currentInput == INPUT_INCORRECT_PASSWORD && !security.isBlocked(); });
  stateMachine.AddTransition(STATE_SECURITY, STATE_BLOCKED, 
    []() { return currentInput == INPUT_INCORRECT_PASSWORD && security.isBlocked(); });
  
  // Transiciones desde LOGIN_SUCCESS
  stateMachine.AddTransition(STATE_LOGIN_SUCCESS, STATE_MONITORING, 
    []() { return currentInput == INPUT_LOGIN_SUCCESS_COMPLETE; });
  
  // Transiciones desde LOGIN_ERROR
  stateMachine.AddTransition(STATE_LOGIN_ERROR, STATE_SECURITY, 
    []() { return currentInput == INPUT_LOGIN_ERROR_COMPLETE; });

  
  // Transiciones desde MONITORING
  stateMachine.AddTransition(STATE_MONITORING, STATE_ALARM, 
    []() { 
      return alarms.isStabilized() && 
             (sensors.isAlarmTemperature() && sensors.isLowLight());
    });
  stateMachine.AddTransition(STATE_MONITORING, STATE_PMV_HIGH, 
    []() { return currentInput == INPUT_TEMP_HIGH; });
  stateMachine.AddTransition(STATE_MONITORING, STATE_PMV_LOW, 
    []() { return currentInput == INPUT_TEMP_LOW; });
  
  // Transiciones desde ALARM
  stateMachine.AddTransition(STATE_ALARM, STATE_MONITORING, 
    []() { 
      return alarms.isAlarmTimeout() && !alarms.isMaxAlarmsReached();
    });
  stateMachine.AddTransition(STATE_ALARM, STATE_BLOCKED, 
    []() { 
      return alarms.isAlarmTimeout() && alarms.isMaxAlarmsReached();
    });
  
  // Transiciones desde PMV_HIGH
  stateMachine.AddTransition(STATE_PMV_HIGH, STATE_MONITORING, 
    []() { 
      return (millis() - stateStartTime >= Config::PMV_HIGH_DURATION);
    });
  stateMachine.AddTransition(STATE_PMV_HIGH, STATE_ALARM, 
    []() { return sensors.isHighTemperature(); });
  
  // Transiciones desde PMV_LOW
  stateMachine.AddTransition(STATE_PMV_LOW, STATE_MONITORING, 
    []() { 
      return (millis() - stateStartTime >= Config::PMV_LOW_DURATION);
    });
  stateMachine.AddTransition(STATE_PMV_LOW, STATE_ALARM, 
    []() { return sensors.isHighTemperature(); });
  
  // Transiciones desde BLOCKED
  stateMachine.AddTransition(STATE_BLOCKED, STATE_SECURITY, 
    []() { 
      return security.isResetPressed() || currentInput == INPUT_RESET; 
    });

  
  // Configurar funciones de entrada
  stateMachine.SetOnEntering(STATE_INIT, enterInit);
  stateMachine.SetOnEntering(STATE_SECURITY, enterSecurity);
  stateMachine.SetOnEntering(STATE_LOGIN_SUCCESS, enterLoginSuccess);
  stateMachine.SetOnEntering(STATE_LOGIN_ERROR, enterLoginError);
  stateMachine.SetOnEntering(STATE_MONITORING, enterMonitoring);
  stateMachine.SetOnEntering(STATE_ALARM, enterAlarm);
  stateMachine.SetOnEntering(STATE_BLOCKED, enterBlocked);
  stateMachine.SetOnEntering(STATE_PMV_HIGH, enterPMVHigh);
  stateMachine.SetOnEntering(STATE_PMV_LOW, enterPMVLow);
}

// ==================== SETUP Y LOOP ====================
void setup() {
  Serial.begin(9600);
  
  // Inicializar componentes
  sensors.begin();
  leds.begin();
  display.begin();
  fan.begin();
  buzzer.begin();
  rfid.begin();
  
  // Configurar máquina de estados
  setupStateMachine();
  stateMachine.SetState(STATE_INIT, false, true);
  
  // Iniciar tareas
  taskSensors.Start();
  taskDisplay.Start();
  taskKeypad.Start();
  taskRFID.Start();
  taskLEDs.Start();
  taskBuzzer.Start();
  taskFan.Start();
  
  Serial.println("Sistema iniciado");
}





void loop() {
  // Actualizar todas las tareas asíncronas
  taskSensors.Update();
  taskDisplay.Update();
  taskKeypad.Update();
  taskRFID.Update();
  taskLEDs.Update();
  taskBuzzer.Update();
  taskFan.Update();
  
  // Actualizar managers
  alarms.update();
  
  // Actualizar la máquina de estados
  stateMachine.Update();
  
  // Limpiar el input después de un delay más corto para transiciones rápidas
  static unsigned long lastInputTime = 0;
  if (currentInput != INPUT_UNKNOWN) {
    if (millis() - lastInputTime > 100) { // 100ms de delay
      currentInput = INPUT_UNKNOWN;
    }
  } else {
    lastInputTime = millis();
  }
  
  // Pequeño delay para estabilidad del sistema
  delay(10);
}