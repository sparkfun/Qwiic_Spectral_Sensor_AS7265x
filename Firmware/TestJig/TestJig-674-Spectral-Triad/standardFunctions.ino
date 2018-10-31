/*
  Functions commonly used for jig testing but not unique to a given jig
*/

//We have to set any GPIO that is connected to the target to High Impedance
//Otherwise, the ATmega will parasitically power the target
void powerOffTarget()
{
  Serial.println("Powering off target");

  FJ.disableRegulator1();
  FJ.disableRegulator2();

  digitalWrite(SWITCH_ENABLE_SERIAL, LOW); //Disable
  digitalWrite(SWITCH_ENABLE_I2C, LOW); //Disable
  digitalWrite(SWITCH_ENABLE_SPI, LOW); //Disable

  //Go to high impedance for all I/O
  pinMode(TRIAD_INTERFACE_TYPE, INPUT);
  pinMode(TRIAD_INT, INPUT);
  pinMode(TRIAD_RST, INPUT);
  pinMode(TRIAD_EE_CS, INPUT);
  pinMode(MICROSD_CS, INPUT);

  //Turn off anything that may be providing power to target
  SD.end(); //Supported in Arduino 1.8.7
  SPI.end();
  Wire.end();
  Serial1.end();

  //Force I2C pins to inputs
  pinMode(18, INPUT);
  pinMode(19, INPUT);
  pinMode(20, INPUT);
  pinMode(21, INPUT);

  //Force SPI pins to inputs
  for (int x = 50 ; x < 60 ; x++)
  {
    digitalWrite(x, LOW);
    pinMode(x, INPUT);
  }
}

//This sets up the pins to various ins and outs
//This is external to setup so that we can call it after a powerOffTarget()
void initializeIO()
{
  //SPI.begin();
  //SPISettings(4000000, MSBFIRST, SPI_MODE0);

  pinMode(TRIAD_INTERFACE_TYPE, OUTPUT);
  digitalWrite(TRIAD_INTERFACE_TYPE, HIGH); //Set to I2C, not Serial
  pinMode(TRIAD_INT, INPUT);
  pinMode(TRIAD_RST, OUTPUT);
  digitalWrite(TRIAD_RST, HIGH); //Do not reset Triad
  pinMode(TRIAD_EE_CS, INPUT); //Go to high impedance for now

  setTestLEDs(STATUS_OFF);
}

//Power everything off
//Set fail LEDs
//Print a message letting user know what happened
void bailTest(String message)
{
  powerOffTarget();

  setTestLEDs(STATUS_MAINTEST_FAIL);

  Serial.println(message);
  Serial.println();
}

//Power everything off
//Set fail LEDs
//Print a message letting user know what happened
void bailPretest(String message)
{
  FJ.disableRegulator1();
  FJ.disableRegulator2();

  setTestLEDs(STATUS_PRETEST_FAIL);

  Serial.println(message);
  Serial.println();
}

//There's really only four states the LEDs could be in:
//Off
//Pretest Failed, both main LEDs off
//Pretest passed, main test fail
//Pretest passed, main test passed
void setTestLEDs(led_status state)
{
  if (state == STATUS_OFF)
  {
    digitalWrite(LED_PRETEST_PASS, LOW);
    digitalWrite(LED_PRETEST_FAIL, LOW);
    digitalWrite(LED_PASS, LOW);
    digitalWrite(LED_FAIL, LOW);
  }
  else if (state == STATUS_PRETEST_FAIL)
  {
    digitalWrite(LED_PRETEST_PASS, LOW);
    digitalWrite(LED_PRETEST_FAIL, HIGH);
    digitalWrite(LED_PASS, LOW);
    digitalWrite(LED_FAIL, LOW);
  }
  else if (state == STATUS_MAINTEST_FAIL)
  {
    digitalWrite(LED_PRETEST_PASS, HIGH);
    digitalWrite(LED_PRETEST_FAIL, LOW);
    digitalWrite(LED_PASS, LOW);
    digitalWrite(LED_FAIL, HIGH);
  }
  else if (state == STATUS_PASS)
  {
    digitalWrite(LED_PRETEST_PASS, HIGH);
    digitalWrite(LED_PRETEST_FAIL, LOW);
    digitalWrite(LED_PASS, HIGH);
    digitalWrite(LED_FAIL, LOW);
  }
}

//Turns on regulators and tests for shorts
//Returns success or fail
int powerUpAndTest()
{
  if (FJ.testRegulator1() == false)
  {
    Serial.println("Whoa! Short on power rail 1");
    return (fail);
  }

  if (FJ.testRegulator2() == false)
  {
    Serial.println("Whoa! Short on power rail 2");
    return (fail);
  }
  Serial.println("No shorts detected!");

  FJ.enableRegulator1();
  FJ.enableRegulator2();
  Serial.println("3.3V and 5V regulators powered up");
  return (success);
}
