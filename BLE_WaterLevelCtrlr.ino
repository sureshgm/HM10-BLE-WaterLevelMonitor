/*
  BLE Water level Indicator - Used for Over Head Tank (OHT) waterlevel Indication

Two Numbers HM10 are used for this wireless communication between the OHT and Controller.
The HM10 at the Tank end acts as peripheral and HM10 connected to arduino acts as controller / monitor.
Receives data from a peripheral HM10 Module connected to the Waterlevel sensor switches at the OHT.
Uses 4 LED to indicate 25, 50, 75, 100% water level.

This monitor is developed basically to facilitate wireless OHT monitoring for overflow of water when water pump is turned ON.

 The circuit:
 
 * BLE - Soft RX is digital pin A2 (connect to TX of other device)
 * BLE - Soft TX is digital pin A3 (connect to RX of other device)
 * D3, D4, D5, D6 - RED indicator LED's to monitor OHT connected.
 * D7 - Buzzer connected thru BC547 transistor
 * D8 - Switch to silence Buzzer on Over flow - will be enabled when tank is below 74%
 * Operating Voltage 3.3V - Do not operate at 5V will damage HM10 module.
 * Reading GPIO PIO4 to PIO7 (pin 26 to 30)
 * PIO3 is strobe to read input

 created 
 modified 01 April 2019
 by Suresh G M
 based on Arduino Software serial example

 This example code is in the public domain.

Date: 23 Apr 2019 - Implement Battery level, Buzzer for full tank, Silence buzzer, 30 second read, low current(remove LED/Connect to PIO2) 
on disconnect to peripheral all LED OFF, D3 Blink on Empty Tank
 */
#include <SoftwareSerial.h>

#define SENSOR_READ_INTERVAL  10;   // should not be less than 4 seconds

#define P100TANK   20
#define P75TANK   68
#define P50TANK   97
#define P25TANK   177
#define EMTY_TANK   250
#define TRGR_ON 50

// Responses
#define CON_OK  ATRESP[0]
#define CON_LOST  ATRESP[1]
#define WAKEUP  ATRESP[2]
#define RT_GETV   ATRESP[3]
#define RT_ADC4   ATRESP[4]


#define RT_bGPIO  "OK+PIO?:"
#define RT_xGPIO  "OK+COL"
// AT Commands
#define RD_BATT ATCMDS[0]
#define SETPIO3 ATCMDS[1]
#define RD_ADC4 ATCMDS[2]
#define CLRPIO3 ATCMDS[3]
#define RD_xGPIO  ATCMDS[4]
#define RD_bGPIO  ATCMDS[5]
#define RD_RSSI ATCMDS[6]

const String ATCMDS[] = {"AT+BATT?", "AT+PIO31", "AT+ADC4?", "AT+PIO30", "AT+COL??", "AT+PIO??", "AT+RSSI?"};
const String ATRESP[] = {"OK+CONN", "OK+LOST", "OK+WAKE", "OK+GET", "OK+ADC4"}; 
//const String ATCMD = "AT";
#define TRUE  1
#define FALSE 0

SoftwareSerial mySerial(A2, A3); // RX, TX

String RecvArray;
String ConvStr;
int reqintrvl;

const int FullTankLED = 6;
const int P75LED = 5;
const int P50LED = 4;
const int P25LED = 3; 
const int EmtyTankLED = 3;
const int Buzzer = 7;    
const int Switch1 = 8;

char Sw1DebounceCntr = 0;
unsigned long PrevmS = 0;
char CmdArr[16];
int ccntr = 0;
bool pConnected = FALSE;
float ADC4_Val;
long PIOval;
int WaterLvl;
bool SilenceBuzz;

/*
 * Clears all the LED Indication
 */
void ClearLEDInd(void)
{
  digitalWrite(P25LED, HIGH);
  digitalWrite(P50LED, HIGH);
  digitalWrite(P75LED, HIGH);
  digitalWrite(FullTankLED, HIGH);
}

void ClearArray(char *ArrIn, int NoBytes)
{
  while(NoBytes--)  {       // decrement array loc.
    ArrIn[NoBytes] = 0;  // clear all thye element from 0 to NoBytes-1
  }        
}

/*
 * Setup function
 */
void setup() {
                              // Configure all LED pin as output.
  pinMode(P25LED, OUTPUT);
  pinMode(P50LED, OUTPUT);
  pinMode(P75LED, OUTPUT);
  pinMode(FullTankLED, OUTPUT);
  
  pinMode(Switch1, INPUT_PULLUP);   // switch with input pull up
  
  pinMode(Buzzer, OUTPUT);
  digitalWrite(Buzzer, LOW);
  
  ClearLEDInd();              // Turn OFF all LED - writing 1 turns OFF the LED 
  RecvArray.reserve(16);      // reserve 16 bytes for Recv array
  RecvArray = "";
  ConvStr.reserve(16);        // ConvStr string to hold 16 bytes
  SilenceBuzz = 0;
  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  Serial.println("OHT water level Monitor");

  // set the data rate for the SoftwareSerial port
  mySerial.begin(9600);
  mySerial.setTimeout(200);    // Set time out of 1 sec for string reception
  mySerial.println("AT");
  reqintrvl = SENSOR_READ_INTERVAL;
}

void loop() { // run over and over
  if(digitalRead(Switch1) != HIGH)  {               // read switch press, 0 if pressed
    if(Sw1DebounceCntr < 100) Sw1DebounceCntr++;  // increment debounce counter if switch pressed
    if(Sw1DebounceCntr == TRGR_ON)  SilenceBuzz ^= 1; // on trigger value perform action
  }
  else  {
    if(Sw1DebounceCntr) Sw1DebounceCntr--;      // if switch release decrement debounce counter
  }
  if((millis() - PrevmS) > 999)  {             // task for every 1 second
    PrevmS = millis();                         // eqaute previous ms to present milli second count
    if(digitalRead(FullTankLED) != HIGH) {     // if full tank LED is ON
      if(reqintrvl & 0x0003) 
        digitalWrite(Buzzer, LOW);            // buzzer is off for 3 seconds
      else if(SilenceBuzz != TRUE) digitalWrite(Buzzer, HIGH); //  - buzzer ON for 1 second
    }
    else  {
      digitalWrite(Buzzer, LOW);    // else tun OFF the Buzzer
      SilenceBuzz = FALSE;          // reset silence Buzzer when not full tank
    }
    if(reqintrvl == 5) {            // 5th second - Print Status
      if(SilenceBuzz) Serial.print("Buz OFF");
      else Serial.print("Buz ON ");
      if(digitalRead(FullTankLED) != HIGH)  Serial.print("Tank FULL");
      if(digitalRead(P25LED) != LOW) Serial.print("Tank Low");
    }
    if(reqintrvl == 4) {            // 4th second - check for connection
      ClearLEDInd();                // Turn OFF all the LED's
      if(pConnected != TRUE)  {
        mySerial.print("AT"); 
        Serial.println("** Disconnected **");
        reqintrvl = SENSOR_READ_INTERVAL;
        WaterLvl = 300;
      }
      else  {
        Serial.println("** Connected **");
        if(WaterLvl < P100TANK)     // D6 for full tank
          digitalWrite(FullTankLED, LOW);
        if(WaterLvl < P75TANK)      // D5 for 3/4th of Tank
          digitalWrite(P75LED, LOW);
        if(WaterLvl < P50TANK)      // D4 for 1/2 tank
          digitalWrite(P50LED, LOW);
        if(WaterLvl < P25TANK)      // D3 for 1/4 tank
          digitalWrite(P25LED, LOW);             
      }      
    }

     if(reqintrvl == 3)           // 3rd second - Gett Radio signal level    
      mySerial.print(RD_RSSI);
    if(reqintrvl == 2)           // 2nd second - strobe Sensor
      mySerial.print(SETPIO3);    
    if(reqintrvl == 1)           // 1st second read sensor value
      mySerial.print(RD_ADC4);   
    if(reqintrvl == 0)            // Clear strobe (Sensor OFF)
      mySerial.print(CLRPIO3);    
    
    if(reqintrvl) 
      reqintrvl--;    // if 10 sends are not over decrement counter till 10 seconds
    else  
      reqintrvl = SENSOR_READ_INTERVAL;         // reload counter for 10 seconds      
  }
  
  if(mySerial.available()) {      // if Serial data is available
    RecvArray = mySerial.readString();  // Receive the string from the serial port
    Serial.println(RecvArray);            // then print it to console
    RecvArray.trim();
    if(RecvArray == CON_OK) pConnected = TRUE;
    if(RecvArray == CON_LOST) pConnected = FALSE; 
    if(RecvArray.startsWith(RT_xGPIO)) {
      ConvStr = RecvArray.substring(7);    // Characters from 9 to 12 indicate the GPIO
      PIOval = ConvStr.toInt();
      Serial.println(PIOval, HEX);
    }
    if(RecvArray.startsWith(RT_ADC4))  {
      ConvStr = RecvArray.substring(8);    // Characters from 9 to 12 indicate the GPIO
      ADC4_Val = ConvStr.toFloat();
      WaterLvl = (int) (ADC4_Val * 100.0);
      //Serial.println(ADC4_Val);
      Serial.println(WaterLvl);
    }
  }

  if (Serial.available()) {             // if data available from console 
    char hrx_data = Serial.read();      // read the received data
    if(hrx_data == '\r' || hrx_data == '\n')  { // if New line or Carriage return char received
      if(ccntr) {                       // if characters are received before \n \r
        mySerial.print(CmdArr);       // send them to the Peripheral module
        Serial.println(CmdArr);         // print to console
        ClearArray(CmdArr, 16);         // clear the array
        ccntr = 0;                  
      }
    }
    else  {
      if(ccntr < 16)                  // else keep receiving characters till max 16
        CmdArr[ccntr++] = hrx_data;   // put data in array
    }
  }
}
