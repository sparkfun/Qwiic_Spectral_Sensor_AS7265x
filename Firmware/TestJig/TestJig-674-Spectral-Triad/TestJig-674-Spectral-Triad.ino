/*
  Triad Test Jig
*/

#include "SparkFun_Flying_Jalapeno_Arduino_Library.h" //http://librarymanager/All#Flying_Jalapeno
FlyingJalapeno FJ(13, 3.3); //Blink status msgs on pin 13. The FJ is setup for 3.3V I/O.

#include "SparkFun_AS7265X.h" //Click here to get the library: http://librarymanager/All#SparkFun_AS7265X
AS7265X sensor;

#include <SPI.h> //Needed for microSD reading
//#include "SdFat.h"
#include <SD.h>

//SdFat sd;
File firmwareFile;

//Pins that should be the same for all test jigs
#define STATUS_LED 13
#define SWITCH_ENABLE_I2C 22 //This controls the 74LVC4066 switch
#define SWITCH_ENABLE_SERIAL 23 //This controls the 74LVC4066 switch
#define SWITCH_ENABLE_SPI A1 //This controls the 74LVC4066 switch

//Pins specific to the target to be tested
#define TRIAD_INTERFACE_TYPE 24
#define TRIAD_INT 25
#define TRIAD_RST 26
#define TRIAD_EE_CS 29
#define MICROSD_CS  30

#define TARGET_I2C_ADDRESS 0x49 //Spectral Triad default is 0x49

boolean targetPowered = false; //Keeps track of whether power supplies are energized

const int success = 1; //All is well
const int fail = -1; //Error returned from a function

enum led_status {
  STATUS_OFF,
  STATUS_PRETEST_FAIL,
  STATUS_MAINTEST_FAIL,
  STATUS_PASS
};

void setup()
{
  //Setup hardware pins
  pinMode(STATUS_LED, OUTPUT);

  pinMode(SWITCH_ENABLE_SERIAL, OUTPUT);
  digitalWrite(SWITCH_ENABLE_SERIAL, LOW); //Disable
  pinMode(SWITCH_ENABLE_I2C, OUTPUT);
  digitalWrite(SWITCH_ENABLE_I2C, LOW); //Disable
  pinMode(SWITCH_ENABLE_SPI, OUTPUT);
  digitalWrite(SWITCH_ENABLE_SPI, LOW); //Disable

  //On this jig, reg 1 powers the switch at 3.3V
  FJ.setRegulatorVoltage1(3.3);

  FJ.setRegulatorVoltage2(3.3); //Not used on this jig

  FJ.enablePCA(); //Enable the I2C buffer

  Serial.begin(9600);
  Serial.println("Spectral Triad Test. Press button to begin.");
}

void loop()
{
  if (FJ.isTestPressed() == true)
  {
    //Order of test operations
    //Look for shorts
    //Power up target
    //Look for valid data over I2C from the target
    //Look for valid data over UART from the target

    FJ.statOn(); //Indicate start of test
    Serial.println("Spectral Triad test started");
    setTestLEDs(STATUS_OFF);

    //Power up 5V regulator and test for shorts
    if (powerUpAndTest() == fail)
    {
      bailPretest("Power up failed");
      return; //Bail if shorts are detected
    }

    Serial.println("Pretest PASS");

    if(programEEPROM() == fail)
    {
      bailTest("EEPROM program failed");
      return;
    }

    //Once firmware has been written, release EEPROM CS pin (high impedance)
    digitalWrite(TRIAD_EE_CS, LOW);
    pinMode(TRIAD_EE_CS, INPUT);

    digitalWrite(TRIAD_RST, LOW); //Hold Triad in reset during power cycle
    digitalWrite(TRIAD_INTERFACE_TYPE, HIGH); //Change the interface pin to I2C

    //Power cycle everything
    powerOffTarget(); //Turn all GPIOs to inputs
    FJ.disableRegulator1();
    delay(250);
    FJ.enableRegulator1();
    initializeIO(); //Re-init GPIOs
    
    digitalWrite(TRIAD_RST, HIGH); //Once firmware has been written, release Triad out of reset

    //Check for I2C - the unit should respond to I2C ack
    if (testI2C(TARGET_I2C_ADDRESS) == fail)
    {
      bailTest("I2C failed");
      return;
    }

    digitalWrite(TRIAD_RST, LOW); //Hold Triad in reset during power cycle
    digitalWrite(TRIAD_INTERFACE_TYPE, LOW); //Change the interface pin to Serial

    //Power cycle everything
    powerOffTarget(); //Turn all GPIOs to inputs
    FJ.disableRegulator1();
    delay(100);
    FJ.enableRegulator1();
    initializeIO(); //Re-init GPIOs
    
    digitalWrite(TRIAD_RST, HIGH); //Once firmware has been written, release Triad out of reset

    //Check for UART - unit should report special characters over serial1
    if (testUART() == fail)
    {
      bailTest("UART failed");
      return;
    }

    //All done!
    powerOffTarget(); //Turn all GPIOs to inputs

    Serial.println("All tests pass!");
    setTestLEDs(STATUS_PASS);
    FJ.disableRegulator1(); //Power off regulator
    FJ.disableRegulator2(); //Power off regulator

    Serial.println();

    FJ.statOff();
  }
  else
  {
    delay(25); //Debounce between button checks
  }

}


//Do a standard I2C ack test at given address
//Returns success if sensor acks
int testI2C(byte address)
{
  FJ.enablePCA(); //Enable the I2C buffer
  digitalWrite(SWITCH_ENABLE_I2C, HIGH); //Enable the I2C connection
  Wire.begin();

  //1000 is too short
  delay(1750); //It takes more than 600ms to go from RST to I2C available

  //From AS7265x library, Temperature example.
  if(sensor.begin() == false)
  {
    Serial.println("Sensor does not appear to be connected. Please check wiring.");
    digitalWrite(SWITCH_ENABLE_I2C, LOW); //Disable the I2C connection
    FJ.disablePCA(); //Enable the I2C buffer
    return (fail);
  }

  int oneSensorTemp = sensor.getTemperature(); //Returns the temperature of master IC
  Serial.print("Main IC temp: ");
  Serial.print(oneSensorTemp);
  Serial.println("C");

  digitalWrite(SWITCH_ENABLE_I2C, LOW); //Disable the I2C connection
  FJ.disablePCA(); //Enable the I2C buffer
  return (success);
}

//Waits for correct characters from serial 1
//Gives up after 500ms
int testUART()
{
  //Look for a response of "OK" to "AT"

  Serial1.begin(115200); //Triad is tied to Serial1 and speaks 115200 default
  digitalWrite(SWITCH_ENABLE_SERIAL, HIGH); //Enable the UART connection

  digitalWrite(TRIAD_RST, LOW); //Hold Triad in reset
  digitalWrite(TRIAD_INTERFACE_TYPE, LOW); //Change the interface pin to Serial
  delay(50);
  digitalWrite(TRIAD_RST, HIGH); //Bring Triad out of reset. Should now be in serial mode.

  delay(1000); //It takes a considerable amount of time for sensor to come out of reset

  while(Serial1.available()) Serial1.read(); //Clear anything the sensor has sent us

  Serial1.print("AT\n\r");
  Serial1.print("AT\n\r");

  int timeout = 0;
  while (timeout++ < 500)
  {
    if (Serial1.available())
    {
      byte incoming = Serial1.read();
      if (incoming == 'O')
      {
        while (!Serial1.available());
        if (Serial1.read() == 'K')
        {
          digitalWrite(SWITCH_ENABLE_SERIAL, LOW); //Disable the UART connection
          return (success); //We've got good data!
        }
      }

      Serial.write(Serial1.read());
    }
    delay(1);
  }
  digitalWrite(SWITCH_ENABLE_SERIAL, LOW); //Disable the UART connection
  return (fail);
}
