#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP085_U.h>
#include <SoftwareSerial.h>
 
SoftwareSerial BTSerial(2, 3); //Connect HC-06. Use your (TX, RX) settings

/*  Pulse Sensor Amped 1.5    by Joel Murphy and Yury Gitman   http://www.pulsesensor.com
----------------------  Notes ----------------------  ----------------------
This code:
1) Blinks an LED to User's Live Heartbeat   PIN 13
2) Fades an LED to User's Live HeartBeat    PIN 5
3) Determines BPM
4) Prints All of the Above to Serial
Read Me:
https://github.com/WorldFamousElectronics/PulseSensor_Amped_Arduino/blob/master/README.md
 ----------------------       ----------------------  ----------------------
*/

Adafruit_BMP085_Unified bmp = Adafruit_BMP085_Unified(10085);
//  Variables
int pulsePin = 0;                 // Pulse Sensor purple wire connected to analog pin 0
int fadeRate = 0;                 // used to fade LED on with PWM on fadePin
int altitude;
// Volatile Variables, used in the interrupt service routine!
volatile int BPM;                   // int that holds raw Analog in 0. updated every 2mS
volatile int Signal;                // holds the incoming raw data
volatile int IBI = 600;             // int that holds the time interval between beats! Must be seeded!
volatile boolean Pulse = false;     // "True" when User's live heartbeat is detected. "False" when not a "live beat".
volatile boolean QS = false;        // becomes true when Arduoino finds a beat.
char BPM_buff[4];
char Altitude_buff[5]; 
// SET THE SERIAL OUTPUT TYPE TO YOUR NEEDS
// PROCESSING_VISUALIZER works with Pulse Sensor Processing Visualizer
//      https://github.com/WorldFamousElectronics/PulseSensor_Amped_Processing_Visualizer
// SERIAL_PLOTTER outputs sensor data for viewing with the Arduino Serial Plotter
//      run the Serial Plotter at 115200 baud: Tools/Serial Plotter or Command+L

void setup(){
  Serial.begin(9600);             // we agree to talk fast!
  interruptSetup();                 // sets up to read Pulse Sensor signal every 2mS
  BTSerial.begin(9600);  // set the data rate for the BT port
  Serial.println("Pressure Sensor Test"); Serial.println("");
    /* Initialise the sensor */
  if(!bmp.begin()) {
    Serial.print("Ooops, no BMP085 detected ... Check your wiring or I2C ADDR!");
    while(1);
  }
  /* Display some basic information on this sensor */
  displaySensorDetails();
}
//  Where the Magic Happens
void loop(){
  serialOutput();  
  sensors_event_t event;
  bmp.getEvent(&event);
  if (event.pressure) {
    float seaLevelPressure = SENSORS_PRESSURE_SEALEVELHPA;
    altitude = (bmp.pressureToAltitude(seaLevelPressure, event.pressure));

    itoa(BPM, BPM_buff,10);
    Serial.println(BPM_buff);
    itoa(altitude, Altitude_buff,10);
    Serial.println(Altitude_buff);
    BTSerial.write("BBB");
    BTSerial.write(BPM_buff);
    BTSerial.write("SSS");
    BTSerial.write(Altitude_buff);
    BTSerial.write("EEE");
  }else{
    Serial.println("Sensor error");
  }
  if(QS == true){     // A Heartbeat Was Found
    QS = false;                      // reset the Quantified Self flag for next time
  }
  delay(2000);                             //  take a break
}

void displaySensorDetails(void)
{
  sensor_t sensor;
  bmp.getSensor(&sensor);
  Serial.println("------------------------------------");
  Serial.print  ("Sensor:       "); Serial.println(sensor.name);
  Serial.print  ("Driver Ver:   "); Serial.println(sensor.version);
  Serial.print  ("Unique ID:    "); Serial.println(sensor.sensor_id);
  Serial.print  ("Max Value:    "); Serial.print(sensor.max_value); Serial.println(" hPa");
  Serial.print  ("Min Value:    "); Serial.print(sensor.min_value); Serial.println(" hPa");
  Serial.print  ("Resolution:   "); Serial.print(sensor.resolution); Serial.println(" hPa");  
  Serial.println("------------------------------------");
  Serial.println("");
  delay(500);
}
