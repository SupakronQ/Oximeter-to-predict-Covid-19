#include <Wire.h>
#include "MAX30105.h"
#include "spo2_algorithm.h"
#include <LiquidCrystal_I2C.h>
#include <Adafruit_MLX90614.h>

MAX30105 particleSensor;
LiquidCrystal_I2C lcd(0x3F, 16, 2);
Adafruit_MLX90614 mlx = Adafruit_MLX90614();

#define MAX_BRIGHTNESS 255

#if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega168__)
//Arduino Uno doesn't have enough SRAM to store 100 samples of IR led data and red led data in 32-bit format
//To solve this problem, 16-bit MSB of the sampled data will be truncated. Samples become 16-bit data.
uint16_t irBuffer[100]; //infrared LED sensor data
uint16_t redBuffer[100];  //red LED sensor data
#else
uint32_t irBuffer[100]; //infrared LED sensor data
uint32_t redBuffer[100];  //red LED sensor data
#endif

int32_t bufferLength; //data length
int32_t spo2; //SPO2 value
int8_t validSPO2; //indicator to show if the SPO2 calculation is valid
int32_t heartRate; //heart rate value
int8_t validHeartRate; //indicator to show if the heart rate calculation is valid

byte pulseLED = 11; //Must be on PWM pin
byte readLED = 13; //Blinks with each data read
int Temperature;

#define Greenled 8
#define Redled 9
#define Yellowled 10

void setup()
{
  Serial.begin(115200); // initialize serial communication at 115200 bits per second:

  pinMode(pulseLED, OUTPUT);
  pinMode(readLED, OUTPUT);

  // lcd Set up
  lcd.begin();
  lcd.backlight();
  lcd.setCursor(3,1);
  lcd.print("Running......");
  delay(3000);
  lcd.clear();
  
  pinMode(Greenled,OUTPUT);
  pinMode(Redled,OUTPUT);
  pinMode(Yellowled,OUTPUT);

  digitalWrite(Greenled,LOW);
  digitalWrite(Redled,LOW);
  digitalWrite(Yellowled,LOW);

  // Initialize sensor
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) //Use default I2C port, 400kHz speed
  {
    Serial.println(F("MAX30102 was not found. Please check wiring/power."));
    while (1);
  }

  Serial.println(F("Attach sensor to finger with rubber band. Press any key to start conversion"));
  while (Serial.available() == 0) ; //wait until user presses a key
  Serial.read();

  byte ledBrightness = 60; //Options: 0=Off to 255=50mA
  byte sampleAverage = 4; //Options: 1, 2, 4, 8, 16, 32
  byte ledMode = 2; //Options: 1 = Red only, 2 = Red + IR, 3 = Red + IR + Green
  byte sampleRate = 100; //Options: 50, 100, 200, 400, 800, 1000, 1600, 3200
  int pulseWidth = 411; //Options: 69, 118, 215, 411
  int adcRange = 4096; //Options: 2048, 4096, 8192, 16384

  particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange); //Configure sensor with these settings
  particleSensor.enableDIETEMPRDY();
  mlx.begin();

}

void loop()
{
  bufferLength = 100; //buffer length of 100 stores 4 seconds of samples running at 25sps
  float temperature = particleSensor.readTemperatureF();
  //read the first 100 samples, and determine the signal range
  lcd.setCursor(3,1);
  lcd.print("Running......");

  for (byte i = 0 ; i < bufferLength ; i++)
  {
    while (particleSensor.available() == false) //do we have new data?
      particleSensor.check(); //Check the sensor for new data

    redBuffer[i] = particleSensor.getRed();
    irBuffer[i] = particleSensor.getIR();
    particleSensor.nextSample(); //We're finished with this sample so move to next sample

    Serial.print(F("red="));
    Serial.print(redBuffer[i], DEC);
    Serial.print(F(", ir="));
    Serial.println(irBuffer[i], DEC);
  }

  //calculate heart rate and SpO2 after first 100 samples (first 4 seconds of samples)
  maxim_heart_rate_and_oxygen_saturation(irBuffer, bufferLength, redBuffer, &spo2, &validSPO2, &heartRate, &validHeartRate);
  lcd.clear();
  //Continuously taking samples from MAX30102.  Heart rate and SpO2 are calculated every 1 second
  while (1)
  {
    //dumping the first 25 sets of samples in the memory and shift the last 75 sets of samples to the top
    for (byte i = 25; i < 100; i++)
    {
      redBuffer[i - 25] = redBuffer[i];
      irBuffer[i - 25] = irBuffer[i];
    }
    Temperature = mlx.readObjectTempC();
    //take 25 sets of samples before calculating the heart rate.
    for (byte i = 75; i < 100; i++)
    {
      while (particleSensor.available() == false) //do we have new data?
        particleSensor.check(); //Check the sensor for new data

      digitalWrite(readLED, !digitalRead(readLED)); //Blink onboard LED with every data read
      

      redBuffer[i] = particleSensor.getRed();
      irBuffer[i] = particleSensor.getIR();
      particleSensor.nextSample(); //We're finished with this sample so move to next sample

      //send samples and calculation result to terminal program through UART

      Serial.print(F("red="));
      Serial.print(redBuffer[i], DEC);
      Serial.print(F(", ir="));
      Serial.println(irBuffer[i], DEC);

      // Serial.print(F(", HR="));
      // Serial.print(heartRate, DEC);

      // Serial.print(F(", SPO2="));
      // Serial.println(spo2, DEC);

      // Serial.println(Temperature);

      lcd.setCursor(0, 0);
      lcd.print("HR:     bpm");

      lcd.setCursor(0, 1);
      lcd.print("O2:     % T:  *C");

      lcd.setCursor(3, 0);
      lcd.print(heartRate);

      lcd.setCursor(3, 1);
      lcd.print(spo2);

      lcd.setCursor(12, 1);
      lcd.print(Temperature);

      if((spo2 >= 96) && (Temperature < 38)){
        digitalWrite(Redled,LOW);
        digitalWrite(Yellowled,LOW);
        digitalWrite(Greenled,HIGH);   
      }
      else if(( spo2 < 93) && (Temperature > 37)){
        digitalWrite(Greenled,LOW);
        digitalWrite(Yellowled,LOW);
        digitalWrite(Redled,HIGH);
      }

      else if(( spo2 < 96) && (Temperature > 37)){
        digitalWrite(Greenled,LOW);
        digitalWrite(Yellowled,HIGH);
        digitalWrite(Redled,LOW);
      }
      else{

        digitalWrite(Greenled,LOW);
        digitalWrite(Yellowled,LOW);
        digitalWrite(Redled,LOW);
        
      }
  
    }

    //After gathering 25 new samples recalculate HR and SP02
    maxim_heart_rate_and_oxygen_saturation(irBuffer, bufferLength, redBuffer, &spo2, &validSPO2, &heartRate, &validHeartRate);
  }
}