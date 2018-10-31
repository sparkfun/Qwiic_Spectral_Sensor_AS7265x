/*
  Functions to erase and write to the 4mbit AT25SF041 SPI EEPROM
  This EEPROM is commonly found on the ESP32/8266 dev board
  as well as the Spectral sensors.

  This is primarily written so that we can read a firmware file from an SD card
  and program the EEPROM on the Spectral sensor boards.
*/

//Power up the SD card, mount it, erase the EEPROM, program it.
//Returns true if everything was successful
int programEEPROM()
{
  initializeIO(); //Set various pins to inputs and outputs

  //Hold the AS72651 in reset so it doesn't control the EEPROM
  digitalWrite(TRIAD_RST, LOW);

  //Unselect the Triad EEPROM
  pinMode(TRIAD_EE_CS, OUTPUT);
  digitalWrite(TRIAD_EE_CS, HIGH);

  if(mountSDCard() == false) //Start communication with SD card. This also starts SPI.
    return(fail);

  firmwareFile = SD.open("AS7265.bin"); //Firmware file to read. Limited file name length to 8.3 format
  if (firmwareFile)
    Serial.println("Firmware file opened");
  else
  {
    Serial.println("Firmware file failed to open");
    return(fail);
  }
  
  if(eepromIsConnected() == false) //Check that we can detect the EEPROM
  {
    Serial.println("EEPROM not detected. Check pogo connections.");
    return(fail);
  }
  Serial.println("EEPROM detected");

  eepromEraseChip(); //Erase the entirety of the EEPROM

  writeFileToEEPROM(firmwareFile); //Write the given file to EEPROM

  Serial.println("Verifying EEPROM");
  byte myData = eepromReadByte(0x02); //The characters ams are written to start of EEPROM. 
  if(myData != 's')
  {
    Serial.print("EEPROM read fail: ");
    Serial.write(myData);
    Serial.println();
    return(fail);
  }

  Serial.println("Firmware successfully written");

  return(success);  
}

//To get to the SD card we need to provide it power, enable the SPI switch
//And begin SD with the SD CS pin
//Finally, we open the firmware file to deal with
//Note: The firmware file name is limited to a length of 8 chars with 3 char extension (no long file names)
boolean mountSDCard()
{
  powerUpAndTest(); //Test and turn on 3.3V rail to power microSD card

  digitalWrite(SWITCH_ENABLE_SPI, HIGH); //Enable SPI

  //Mount SD card
  if (!SD.begin(MICROSD_CS)) {
    Serial.println("SD initialization failed!");
    return(false);
  }
  return(true);
}

//Given a file name, attemps to open that file from the SD card
//It will then record that file to EEPROM one page (256 bytes) at a time
void writeFileToEEPROM(File firmwareFile)
{
  long currentAddress = 0;

  //Page size on the AT25SF041 is 256 bytes
  int pageSize = 256;
  byte dataArray[pageSize];

  Serial.println("Begin writing firmware.");
  
  //Bulk write from the SD file to the EEPROM
  while (firmwareFile.available()) 
  {
    int bytesToWrite = pageSize; //Max number of bytes to read
    if(firmwareFile.available() < bytesToWrite) bytesToWrite = firmwareFile.available(); //Trim this read size as needed
    
    firmwareFile.read(dataArray, bytesToWrite); //Read the next set of bytes from file into our temp array

    while(eepromIsBusy()) delay(1); //Wait for write to complete before starting a new one

    //Write enable
    digitalWrite(TRIAD_EE_CS, LOW);
    SPI.transfer(0x06); //Sets the WEL bit to 1
    digitalWrite(TRIAD_EE_CS, HIGH);

    //Write enable
    digitalWrite(TRIAD_EE_CS, LOW);
    SPI.transfer(0x02); //Byte/Page program
    
    SPI.transfer(currentAddress >> 16); //Address byte MSB
    SPI.transfer(currentAddress >> 8); //Address byte MMSB
    SPI.transfer(currentAddress & 0xFF); //Address byte LSB

    for(int x = 0 ; x < bytesToWrite ; x++)
      SPI.transfer(dataArray[x]); //Data!

    digitalWrite(TRIAD_EE_CS, HIGH);

    currentAddress += bytesToWrite;

    if(currentAddress % 8192 == 0) Serial.print("."); //Print . every 8k
    
    if(currentAddress > 511999)
    {
      Serial.print("Whoa, we've written to the end of the EEPROM's available space. Bailing.");
      break;
    }
  }
  Serial.print("Done writing. Bytes written: ");
  Serial.println(currentAddress);

  firmwareFile.close();
}

//Send command to do a full erase of the entire SPI EEPROM content
void eepromEraseChip()
{
  //Assume SPI was already started for SD purposes

  //Write enable
  digitalWrite(TRIAD_EE_CS, LOW);
  SPI.transfer(0x06); //Sets the WEL bit to 1
  digitalWrite(TRIAD_EE_CS, HIGH);

  digitalWrite(TRIAD_EE_CS, LOW);
  SPI.transfer(0xC7); //Do entire chip erase
  digitalWrite(TRIAD_EE_CS, HIGH);

  Serial.println("Erasing EEPROM - takes up to 10 seconds");
  while (eepromIsBusy() == true)
  {
    Serial.print(".");

    for (int x = 0 ; x < 50 ; x++)
    {
      delay(10);
      if (eepromIsBusy() == false) break;
    }
  }
  Serial.println("Erase complete");
}

//Reads a byte from a given location
byte eepromReadByte(unsigned long address)
{
  //Begin reading
  digitalWrite(TRIAD_EE_CS, LOW);
  SPI.transfer(0x03); //Read command, no dummy bytes
  SPI.transfer(address >> 16); //Address byte MSB
  SPI.transfer(address >> 8); //Address byte MMSB
  SPI.transfer(address & 0xFF); //Address byte LSB
  byte response = SPI.transfer(0xFF); //Read in a byte back from EEPROM
  digitalWrite(TRIAD_EE_CS, HIGH);

  return (response);
}

//Writes a byte to a specific location
void eepromWriteByte(unsigned long address, byte thingToWrite)
{
  //Write enable
  digitalWrite(TRIAD_EE_CS, LOW);
  SPI.transfer(0x06); //Sets the WEL bit to 1
  digitalWrite(TRIAD_EE_CS, HIGH);

  //Write enable
  digitalWrite(TRIAD_EE_CS, LOW);
  SPI.transfer(0x02); //Byte/Page program
  SPI.transfer(address >> 16); //Address byte MSB
  SPI.transfer(address >> 8); //Address byte MMSB
  SPI.transfer(address & 0xFF); //Address byte LSB
  SPI.transfer(thingToWrite); //Data!
  digitalWrite(TRIAD_EE_CS, HIGH);
}

//Returns true if the device Busy bit is set
boolean eepromIsBusy()
{
  if (eepromReadStatus1() & 0x01) return (true);
  return (false);
}

//Checks the status byte of the EEPROM
byte eepromReadStatus1()
{
  digitalWrite(TRIAD_EE_CS, LOW);
  SPI.transfer(0x05); //Read status byte 1
  byte response = SPI.transfer(0xFF); //Get byte 1
  digitalWrite(TRIAD_EE_CS, HIGH);

  return (response);
}

//Reads the Manufacturer ID
boolean eepromIsConnected()
{
  //Begin reading
  digitalWrite(TRIAD_EE_CS, LOW);
  SPI.transfer(0x9F); //Read manufacturer and device ID
  byte manuID = SPI.transfer(0xFF);
  byte deviceID1 = SPI.transfer(0xFF);
  byte deviceID2 = SPI.transfer(0xFF);
  digitalWrite(TRIAD_EE_CS, HIGH);

  //Serial.print("Manu ID: 0x");
  //Serial.println(manuID, HEX);

  if(manuID == 0x1F) return(true); //ID is Adesto
  return(false);
}

