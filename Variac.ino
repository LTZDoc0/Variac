#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// imposta LCD 20x4 indirizzo i2c Hex27
LiquidCrystal_I2C lcd(0x27, 20, 4);

// Pin per encoder GND comune
const int pinCLK = 2;
const int pinDT  = 3;

//Impostazione dei limiti della tensione
const uint16_t lowVoltLimit  = 400;   // 40.0 V (x10)
const uint16_t highVoltLimit = 2300;  // 230.0 V (x10)

// PWM
uint16_t pwmValue = 0;

// Encoder stato
uint8_t lastState = 0;
int8_t accumulator = 0;

// Timing accelerazione
unsigned long lastStepTime = 0;

// Display
uint16_t lastVoltShown = 65535;

// Tabella quadratura stati encoder anti merda
const int8_t table[16] =
{
   0, -1,  1,  0,
   1,  0,  0, -1,
  -1,  0,  0,  1,
   0,  1, -1,  0
};

// PWM
void setupPWM()
{
  pinMode(9, OUTPUT);

  TCCR1A = 0;
  TCCR1B = 0;

  TCCR1A |= (1 << COM1A1);
  TCCR1A |= (1 << WGM11);
  TCCR1B |= (1 << WGM12) | (1 << WGM13);

  TCCR1B |= (1 << CS10);

  ICR1 = 1023;
  OCR1A = pwmValue;
}

// Setup pullup pin encoder inzializza LCD e stampa la scritta "Voltage:"
void setup()
{
  pinMode(pinCLK, INPUT_PULLUP);
  pinMode(pinDT, INPUT_PULLUP);

  lcd.init();
  lcd.backlight();

  pwmValue = 0;

  setupPWM();

  lastState = readState();

  lcd.setCursor(0, 0);
  lcd.print("Voltage:");
}

// Loop he fa due cose
void loop()
{
  readEncoder();
  updateDisplay();
}

//  encoder
uint8_t readState()
{
  uint8_t port = PIND;
  uint8_t a = (port >> 2) & 1;
  uint8_t b = (port >> 3) & 1;
  return (a << 1) | b;
}

void readEncoder()
{
  uint8_t state = readState();
  uint8_t index = (lastState << 2) | state;
  int8_t movement = table[index];

  if (movement != 0)
  {
    accumulator += movement;

    if (accumulator >= 4 || accumulator <= -4)
    {

      int8_t direction = (accumulator > 0) ? 1 : -1;
      accumulator = 0;

      unsigned long now = millis();
      unsigned long dt = now - lastStepTime;
      lastStepTime = now;

      uint8_t step = 1;

      if (dt < 40)       step = 40;
      else if (dt < 100) step = 10;
      else               step = 1;

      uint16_t pwmMin = 0;
      uint16_t pwmMax = (highVoltLimit * 1023UL) / 2300;

      if (direction > 0)
      {
        if (pwmValue + step <= pwmMax) pwmValue += step;
        else pwmValue = pwmMax;
      }
       else
     {
        if (pwmValue >= pwmMin + step) pwmValue -= step;
        else pwmValue = pwmMin;
      }
    }
  }

  lastState = state;
  OCR1A = pwmValue;
}

// Scrive sul display quando serve
void updateDisplay() 
{

  uint16_t volt_x10 = lowVoltLimit +
    (uint32_t)pwmValue * (highVoltLimit - lowVoltLimit) / 1023;

  // prima correzione della linearità  (123 → 230) tutta da rivedere
  volt_x10 = lowVoltLimit +
    (uint32_t)(volt_x10 - lowVoltLimit) * 2300 / 1230;

  // seconda correzione della linearità (193 → 230)
  volt_x10 = lowVoltLimit +
    (uint32_t)(volt_x10 - lowVoltLimit) * 2300 / 1930;

  if (volt_x10 > highVoltLimit) volt_x10 = highVoltLimit;
  if (volt_x10 == lastVoltShown) return;

  lastVoltShown = volt_x10;

  lcd.setCursor(8, 0);
  lcd.print("        ");
  lcd.setCursor(8, 0);
  lcd.print(volt_x10 / 10);
  lcd.print(".");
  lcd.print(volt_x10 % 10);
  lcd.print("V");
}