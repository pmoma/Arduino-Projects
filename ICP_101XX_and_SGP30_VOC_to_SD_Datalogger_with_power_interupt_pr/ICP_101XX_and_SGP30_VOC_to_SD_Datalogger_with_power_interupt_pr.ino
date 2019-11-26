 //SGP30 library from adafruit- no other citation info there, original script called sgp30test came with the library
 /*
  SD card datalogger
 The circuit:
 * SD card attached to SPI bus as follows:
 ** MOSI - pin 11
 ** MISO - pin 12
 ** CLK - pin 13
 ** CS - pin 4
 created  24 Nov 2010 modified 9 Apr 2012 by Tom Igoe
 This example code is in the public domain.
 */
 /******************************************************************
 Example for the ICP10xx library for the TDK InvenSense ICP-101xx series of barometric pressure sensors.

 This sketch outputs the current temperature in Fahrenheit, and the pressure in Pascal. Use the Serial Monitor to view the output.
 
 Measurements are made using the non-blocking method measureStart().
 This method immediately returns control to the caller while the sensor takes its measurement. Use the method dataReady() to check when the new sensor data is available.

 Written by Adrian Studer.
******************************************************************/
//There are some good random bits of code from the libraries that were cluttering things up so are stored at the very bottom


#include <icp101xx.h>
// Sensor is an ICP101xx object
ICP101xx sensor;
//Setup for SGP30 VOC shield
#include "Adafruit_SGP30.h"
Adafruit_SGP30 sgp;
#include <Wire.h>              //Wire shared between pressure VOC to run the I2C communication.
#include <SPI.h>               //SPI library for SD because of shield setup.
#include <SdFat.h>             //Setup for SD shield
const uint8_t sdChipSelect = 4;//pin that the sd card shield CS pin is attached to. Not always necessary, the example libraries are mostly written using 4 but sometimes use SS.
SdFat sd;                      //abbreviation but note that renaming sdfat as sd is called differently than the SD library, the capitalization matters, don't mix SD and sd
File myFile;                   //create the file
String buffer;                 //string to buffer output
char filename[19];             // SDcard uses 8.3 names (idk what this means) so 16 bytes is enough NOte room for the '\0' char is needed! Shoutout to Robtillaart from the arduino forums for this block of file creation code.
int time2=millis();            //creating timer for the buffer cycle



void setup() {
  //Overview- File setup, start the pressure and temp sensor, then start the gas senosr, then start the SD card and prepare the file
  Serial.begin(9600);
  // reserve 1kB for String used as a buffer
  buffer.reserve(1024);

  // Initialize sensor. Optional: Pass pointer to Wire to use other than default I2C port. For example: sensor.begin(&Wire1);
  sensor.begin();  
  // Start a first measurement cycle, immediately returning control. Optional: Measurement mode
  //    sensor.FAST: ~3ms
  //    sensor.NORMAL: ~7ms (default)
  //    sensor.ACCURATE: ~24ms
  //    sensor.VERY_ACCURATE: ~95ms
  sensor.measureStart(sensor.ACCURATE);

  //Booting sgp30
  if (! sgp.begin()){
    Serial.println("Sensor not found :(");
    while (1);
  } 



  //Sensor stuff above here SD and file stuff below
  Serial.print("Initializing SD card...");
  // see if the card is present and can be initialized:
  if (!sd.begin(sdChipSelect)) {
    Serial.println("Card failed, or not present"); 
    while (1);
  }
  Serial.println("card initialized.");

  // Get the count of free clusters in the volume.
  uint32_t freeClusters = sd.vol()->freeClusterCount();
  Serial.print("Free clusters: "); Serial.println(freeClusters);
  // Calculate free space in KB.
  float freeKB = 0.512*freeClusters*sd.vol()->blocksPerCluster();
  Serial.print("Free space: "); Serial.print(freeKB); Serial.println(" KB");

  //create a new file for each activation to avoid restart issues
  unsigned int nr = 1; 
  while (nr != 0) {
    sprintf(filename, "datalog_%03d.tsv", nr);//you can change the name but the "char filename[##]" at the beginning must match the name character length- Also, name created as character array not String
    if (sd.exists(filename) == false) break;
    Serial.print(filename); Serial.println(" exists.");
    nr++;
  }
  Serial.print(filename); Serial.println( " created.");
  myFile = sd.open(filename, FILE_WRITE);
  if(myFile) {
    Serial.println("Opened succesfully");
   }
  else{
    Serial.println("Something went wrong opening the SD file. Closing.");
    exit(1);
   }

  //Print the file headers here
  myFile.println("Begin datalogging");
  myFile.print(freeKB); myFile.println(" KB remaining on card");
  myFile.println("Temp C, Pressure Pa, eCO2 ppm, tVOC ppb");
  myFile.close();
  
}
 
void loop() { 
  //Overview- VOC measurements, pressure and temperature measurements, setup the data in the buffer, write the buffer to the SD card and clear it out, then calculate the time the buffer cycle took using time2 or the measurement cycle using time1   
    int time1= millis();//A timer to check how long it takes to run, called again at the very end
  
  //Make sure the VOC sensor is running
  if (! sgp.IAQmeasure()) {
    Serial.println("Measurement failed");//should never happen if sensor is recognized earlier unless it fails internally
    return;
  }

  // Do all the other things you need to do above here if possible to give dataReady time. When the measurement is complete, dataReady() will return true.
  if (sensor.dataReady()) {
    // Read and output measured temperature in Celsius and pressure in Pascals
    //Serial.print(sensor.getTemperatureC()); Serial.print(","); Serial.println(sensor.getPressurePa()); //print it if you want but thats what the logger is for it gets to be a lot.(without temp can be viewed in plotter)  
    sensor.measureStart(sensor.ACCURATE); // Start the next measurement cycle.
  }

  //prep data in buffer to store on SD card
  buffer += String(sensor.getTemperatureC()); buffer += "\t";
  buffer += String(sensor.getPressurePa()); buffer += "\t";
  buffer += String(sgp.eCO2); buffer += "\t";
  buffer += String(sgp.TVOC); buffer += "\n";

  //Serial.println(buffer);
  Serial.print("length :"); Serial.println(buffer.length());

  // check if the SD card is available to write data without blocking
  // chunkSize can be modified butincreasing it too much will hit the buffer size limit around 300, which was causing it not to be able to reach higher chunk limits and therefore couldnt reset, 256 is about 12 records  
  // Also the sd write time is about 15 ms regardless of how short the buffer is so writing 12 takes 30 ms and writing 4 takes 28 ms, so might as well avoid as many sd writes as possible although this will technically cause burst sampling       
  unsigned int chunkSize = 256;
  if (chunkSize && buffer.length() >= chunkSize) {
    myFile = sd.open(filename, FILE_WRITE);
    myFile.print(buffer);
    myFile.close();
    // remove written data from buffer
    buffer.remove(0);
    //int cycletime = (millis()-time2); Serial.println(cycletime); time2= millis();
    // this will tell you the actual time it takes between write cycles
    //with 100 ms delay 12 lines took ~1400 ms not 1200 ms, and given the 11x 15-16 ms per measurement and 1x 30 second write cycle this about adds up to the missing 200 ms
  }
  
  int timesince = (millis()-time1);
  Serial.print("timesince :");
  Serial.println(timesince);
  delay(100);
}






/* return absolute humidity [mg/m^3] with approximation formula-this just came in the original code so I left it in PM
* @param temperature [°C]
* @param humidity [%RH]
uint32_t getAbsoluteHumidity(float temperature, float humidity) {
    // approximation formula from Sensirion SGP30 Driver Integration chapter 3.15- from the adafruit library I didn't write the formula however without a humidity and temp sensor attached this function actually won't be called
    const float absoluteHumidity = 216.7f * ((humidity / 100.0f) * 6.112f * exp((17.62f * temperature) / (243.12f + temperature)) / (273.15f + temperature)); // [g/m^3]
    const uint32_t absoluteHumidityScaled = static_cast<uint32_t>(1000.0f * absoluteHumidity); // [mg/m^3]
    return absoluteHumidityScaled;
    }
*/

//-------------------------------------------------------------------------------------

/*Serial.print("Found SGP30 serial #");//this might be helpful if I had more than one lol, they will all be different and need to be baselined in the same environment to be comparable signals
  Serial.print(sgp.serialnumber[0], HEX);
  Serial.print(sgp.serialnumber[1], HEX);
  Serial.println(sgp.serialnumber[2], HEX);  */
  // If you have a baseline measurement from before you can assign it to start, to 'self-calibrate' //this would then be needed and then s
  //sgp.setIAQBaseline(0x8E68, 0x8F41);  // Will vary for each sensor!
  
// If you have a temperature / humidity sensor, you can set the absolute humidity to enable the humditiy compensation for the air quality signals
  //float temperature = 22.1; // [°C] if using a sensor replace 22.1 and 45.2 with sensor.temp and sensor.humidity or however the sensor calls those measurements
  //float humidity = 45.2; // [%RH]
  //sgp.setHumidity(getAbsoluteHumidity(temperature, humidity));

//-------------------------------------------------------------------------------------

  //Serial.print("TVOC ");        Serial.print(sgp.TVOC); Serial.print(" ppb VOC"); Serial.println();
  //Serial.print("eCO2 ");        Serial.print(sgp.eCO2); Serial.print(" ppm CO2"); Serial.println();
  //Serial.print("Raw H2 ");      Serial.print(sgp.rawH2);                          Serial.println();
  //Serial.print("Raw Ethanol "); Serial.print(sgp.rawEthanol);                     Serial.println();

//-------------------------------------------------------------------------------------
 
 /* 
  // open the file. note that only one file can be open at a time,
  // so you have to close this one before opening another.
  myFile = sd.open(filename, FILE_WRITE);
  // if the file is available, write to it:
  if (myFile) {
    
    myFile.println(buffer);
    myFile.close();
    // print to the serial port too:
    Serial.println(buffer);
    //Serial.println("test");
  }
  // if the file isn't open, pop up an error:
  else {
    Serial.print("error opening datalog");
  }
*/
