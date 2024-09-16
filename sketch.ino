#include <Wire.h>
#include <DHT.h>
#include <LiquidCrystal_I2C.h>
#include <RTClib.h>
#include <EEPROM.h>

#define UTC_OFFSET -3
#define DHTPIN 7
#define DHTTYPE DHT11
#define LOG_OPTION 1
#define SERIAL_OPTION 0
#define BUZZER_PIN 5 // Defina o pino para o buzzer
#define BUTTON_MENU_PIN 6
#define BUTTON_NEXT_PIN 8
#define BUTTON_SELECT_PIN 9

// Definir pinos dos LEDs para cada gatilho
#define LED_TEMP 2   // LED para temperatura fora do gatilho
#define LED_HUMI 3   // LED para umidade fora do gatilho
#define LED_LUZ 4    // LED para luminosidade fora do gatilho

DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2);
RTC_DS1307 RTC;

// Configurações da EEPROM
const int maxRecords = 100;
const int recordSize = 10; // Tamanho de cada registro em bytes
int startAddress = 0;
int endAddress = maxRecords * recordSize;
int currentAddress = 0;

int lastLoggedMinute = -1;

int ValorLDR; // Armazenar a leitura do sensor LDR
int IntensidadeLuz; // Transforma a leitura em uma escala de 0 a 100
int pinoLDR = A0; // PINO ANALÓGICO utilizado para ler o LDR
float temperature;
float humidity;

// Triggers de temperatura e umidade
float trigger_t_min = 15.0; // Exemplo: valor mínimo de temperatura
float trigger_t_max = 25.0; // Exemplo: valor máximo de temperatura
float trigger_u_min = 30.0; // Exemplo: valor mínimo de umidade
float trigger_u_max = 50.0; // Exemplo: valor máximo de umidade
float trigger_l_min = 0.0;  // Exemplo: valor mínimo de luz
float trigger_l_max = 30.0; // Exemplo: valor máximo de luz

byte flocoDeNeve[8] = {
  0b00100,
  0b01010,
  0b00100,
  0b11111,
  0b00100,
  0b01010,
  0b00100,
  0b00000
};

byte faceNeutra[8] = {
  0b00000,
  0b01010,
  0b00000,
  0b00000,
  0b11111,
  0b00000,
  0b00000,
  0b00000
};

byte sol[8] = {
  0b00000,
  0b10101,
  0b01110,
  0b11111,
  0b01110,
  0b10101,
  0b00000
};

enum Unit { CELSIUS, KELVIN, FAHRENHEIT };
Unit currentUnit = CELSIUS;

// Configuração:
void setup() {
  lcd.init();                // Inicializar a LCD  
  dht.begin();
  Serial.begin(9600); // Define a velocidade do monitor serial
  pinMode(pinoLDR, INPUT); // Define o pino que o LDR está ligado como entrada de dados

  RTC.begin();    // Inicialização do Relógio em Tempo Real
  RTC.adjust(DateTime(F(__DATE__), F(__TIME__))); // Ajustar a data e hora uma vez inicialmente, depois comentar
  EEPROM.begin();

  lcd.createChar(0, flocoDeNeve);
  lcd.createChar(1, faceNeutra);
  lcd.createChar(2, sol);

  // Configura os pinos dos LEDs como saídas
  pinMode(LED_TEMP, OUTPUT);
  pinMode(LED_HUMI, OUTPUT);
  pinMode(LED_LUZ, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_MENU_PIN, INPUT_PULLUP);
  pinMode(BUTTON_NEXT_PIN, INPUT_PULLUP);
  pinMode(BUTTON_SELECT_PIN, INPUT_PULLUP);

  // Inicializa os LEDs como desligados
  digitalWrite(LED_TEMP, LOW);
  digitalWrite(LED_HUMI, LOW);
  digitalWrite(LED_LUZ, LOW);
}

// Programa:
void loop() {
  DateTime now = RTC.now();

  // Calculando o deslocamento do fuso horário
  int offsetSeconds = UTC_OFFSET * 3600; // Convertendo horas para segundos
  now = now.unixtime() + offsetSeconds; // Adicionando o deslocamento ao tempo atual

  // Convertendo o novo tempo para DateTime
  DateTime adjustedTime = DateTime(now);

  // Verifica se o minuto atual é diferente do minuto do último registro
  if (adjustedTime.minute() != lastLoggedMinute) {
    lastLoggedMinute = adjustedTime.minute();

    // Ler os valores de temperatura e umidade
    humidity = dht.readHumidity();
    temperature = dht.readTemperature();
    ValorLDR = analogRead(pinoLDR); // Faz a leitura do sensor, em um valor que pode variar de 0 a 1023
    IntensidadeLuz =map(ValorLDR, 44, 937, 0, 100); // Converte o valor para uma escala de 0 a 100

        if (temperature < trigger_t_min || temperature > trigger_t_max ||
        humidity < trigger_u_min || humidity > trigger_u_max ||
        IntensidadeLuz < trigger_l_min || IntensidadeLuz > trigger_l_max) {
          // Salva os dados na EEPROM
            saveLog(now, temperature, humidity, IntensidadeLuz);
              if (LOG_OPTION) get_log();
        }
   

  }

  // Alterna entre as telas de exibição
  static unsigned long lastDisplaySwitch = 0;
  if (millis() - lastDisplaySwitch >= 1000) {
    lastDisplaySwitch = millis();
    static bool showTemperature = true;
    showTemperature = !showTemperature;
    if (showTemperature) {
      MostraUmidadeTemperatura();
    } else {
      mostraLuz();
    }
  }

  checkButton();
}

void checkButton() {
  static bool buttonPressed = false;
  if (digitalRead(BUTTON_MENU_PIN) == LOW) {
    if (!buttonPressed) {
      buttonPressed = true;
      showMenu();
    }
  } else {
    buttonPressed = false;
  }
}

void showMenu() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Selecione Unidade:");
  lcd.setCursor(0, 1);
  lcd.print("Celsius");

  int menuIndex = 0;
  const char* menuItems[] = {"Celsius", "Kelvin", "Fahrenheit"};
  const int menuLength = sizeof(menuItems) / sizeof(menuItems[0]);

  while (true) {
    if (digitalRead(BUTTON_NEXT_PIN) == LOW) {
      menuIndex = (menuIndex + 1) % menuLength;
      lcd.setCursor(0, 1);
      lcd.print("                "); // Limpa a linha
      lcd.setCursor(0, 1);
      lcd.print(menuItems[menuIndex]);
      delay(200); // Debounce
    }

    if (digitalRead(BUTTON_SELECT_PIN) == LOW) {
      currentUnit = static_cast<Unit>(menuIndex);
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Unidade");
      lcd.setCursor(0, 1);
      lcd.print("Selecionada:");
      delay(1000); // Aguardar para que a mensagem seja lida
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(menuItems[menuIndex]);
      delay(1000); // Aguardar para que a unidade selecionada seja lida
      lcd.clear();
      break;
    }
  }
}

void verificaLimites() {
  // Ler os valores dos sensores
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  ValorLDR = analogRead(pinoLDR); // Faz a leitura do sensor, em um valor que pode variar de 0 a 1023
  IntensidadeLuz = map(ValorLDR, 44, 937, 0, 100); // Converte o valor para uma escala de 0 a 100

  // Verifica os gatilhos de temperatura
  if (temperature < trigger_t_min || temperature > trigger_t_max) {
    digitalWrite(LED_TEMP, HIGH);  // Acende o LED de temperatura
    beep(200);
  } else {
    digitalWrite(LED_TEMP, LOW);   // Apaga o LED de temperatura
  }

  // Verifica os gatilhos de umidade
  if (humidity < trigger_u_min || humidity > trigger_u_max) {
    digitalWrite(LED_HUMI, HIGH);  // Acende o LED de umidade
    beep(200);
  } else {
    digitalWrite(LED_HUMI, LOW);   // Apaga o LED de umidade
  }

  // Verifica os gatilhos de luminosidade
  if (IntensidadeLuz < trigger_l_min || IntensidadeLuz > trigger_l_max) {
    digitalWrite(LED_LUZ, HIGH);   // Acende o LED de luminosidade
    beep(200);
  } else {
    digitalWrite(LED_LUZ, LOW);    // Apaga o LED de luminosidade
  }
}

void MostraUmidadeTemperatura() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Umidade(%):");
  lcd.print(humidity);

  lcd.setCursor(0, 1);
  lcd.print("Temp:");
  float tempToShow = convertTemperature(temperature);
  lcd.print(tempToShow);
  lcd.print(getUnitString());
}

float convertTemperature(float tempCelsius) {
  switch (currentUnit) {
    case CELSIUS: return tempCelsius;
    case KELVIN: return tempCelsius + 273.15;
    case FAHRENHEIT: return tempCelsius * 9.0 / 5.0 + 32.0;
  }
  return tempCelsius; // Retorna Celsius por padrão se algo der errado
}

const char* getUnitString() {
  switch (currentUnit) {
    case CELSIUS: return "C";
    case KELVIN: return "K";
    case FAHRENHEIT: return "F";
  }
  return "C"; // Retorna Celsius por padrão se algo der errado
}

void mostraLuz() {
  lcd.clear();
  verificaLimites();
  ValorLDR = analogRead(pinoLDR); // Faz a leitura do sensor, em um valor que pode variar de 0 a 1023
  IntensidadeLuz = map(ValorLDR, 44, 937, 0, 100); // Converte o valor para uma escala de 0 a 100



  lcd.setCursor(0, 0);
  lcd.print("Luz 0-1023= ");

  lcd.print(ValorLDR);

  lcd.setCursor(0, 1);
  lcd.print("Luz % = ");
  lcd.print(IntensidadeLuz);
  lcd.print("%   "); // Adicionado espaços extras para limpar possíveis caracteres residuais

  // Desenhar a figura dependendo da intensidade da luz
  lcd.setCursor(14, 1);
  if (IntensidadeLuz < 30) {
    lcd.write(byte(0)); // Floco de neve
  } else if (IntensidadeLuz < 66) {
    lcd.write(byte(1)); // Face neutra
  } else {
    lcd.write(byte(2)); // Sol
  }
}

void saveLog(DateTime now, float temperature, float humidity, int IntensidadeLuz) {
  int tempInt = (int)(temperature * 100);
  int humiInt = (int)(humidity * 100);
  int luzInt = (int)(IntensidadeLuz * 100);

  EEPROM.put(currentAddress, now.unixtime());
  EEPROM.put(currentAddress + 4, tempInt);
  EEPROM.put(currentAddress + 6, humiInt);
  EEPROM.put(currentAddress + 8, luzInt);

  getNextAddress();
}

void getNextAddress() {
  currentAddress += recordSize;
  if (currentAddress >= endAddress) {
    currentAddress = 0; // Volta para o começo se atingir o limite
  }
}

void beep(int duration) {
  tone(BUZZER_PIN, 1000); // 1000 Hz é o tom do buzzer
  delay(duration);        // Duração do apito
  noTone(BUZZER_PIN);     // Desliga o buzzer
}

void get_log() {
  Serial.println("Data stored in EEPROM:");
  Serial.println("Timestamp\t\tTemperature\tHumidity\tLight");

  for (int address = startAddress; address < endAddress; address += recordSize) {
    long timeStamp;
    int tempInt, humiInt, luzInt;

    // Ler dados da EEPROM
    EEPROM.get(address, timeStamp);
    EEPROM.get(address + 4, tempInt);
    EEPROM.get(address + 6, humiInt);
    EEPROM.get(address + 8, luzInt);

    // Converter valores
    float temperature = tempInt / 100.0;
    float humidity = humiInt / 100.0;
    float luz = luzInt / 100.0;

    // Verificar se os dados são válidos antes de imprimir
    if (timeStamp != 0xFFFFFFFF) { // 0xFFFFFFFF é o valor padrão de uma EEPROM não inicializada
      DateTime dt = DateTime(timeStamp);
      Serial.print(dt.timestamp(DateTime::TIMESTAMP_FULL));
      Serial.print("\t");
      Serial.print(convertTemperature(temperature));
      Serial.print(" ");
      Serial.print(getUnitString());
      Serial.print("\t\t");
      Serial.print(humidity);
      Serial.print(" %\t\t");
      Serial.print(luz);
      Serial.println(" %");
    }
  }
}