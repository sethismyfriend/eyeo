
/*
  WiFi UDP Send and Receive String using OSC protocol for Intel Galileo
  By Seth Hunter  seth.e.hunter@intel.com
  May 02, 2014
  
  Dependencies: 
  OSC: OSCdata.h modified for Intel Galileo architecture. 
  
  Notes: 
  -Wifi network sometimes requires a WPA supplicant and when restarting this is overwritten. 
  Need to test to make sure this does not happen during the installation.
  See: http://www.malinov.com/Home/sergey-s-blog/intelgalileo-addingwifi
  
  Router and Firewall:
  -Windows and McCaffe Firewalls must be off on the remote PC for messages to get through
  -Set Router setting to 60 days, and Static IPs in the variables below for the machines.
  
  Serial Debugging:
  -For installation turn off serial and use OSC to test so the buffers don't fill
  
  MPR121 Wiring Diagrames are located here:
  Touchpad: https://learn.sparkfun.com/tutorials/mpr121-hookup-guide/capacitive-touch-keypad
  Breakout: https://learn.sparkfun.com/tutorials/mpr121-hookup-guide/capacitive-touch-sensor-breakout-board
  
 */
#include "mpr121.h"
#include <Wire.h>
#include <SPI.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <OSCMessage.h>
#include <OSCBundle.h>

//Resistive Sensing 
#define SENSOR_HYSTERESIS 20  // ignore incremental changes smaller than this value...  needed to kill noise
#define NOISE_FLOOR 1000 // out of 1023, higher than this is considered open circuit.
#define SENSOR_PIN  A0  // also known as P14

//touch panel
#define REDPIN 5
#define GREENPIN 6
#define BLUEPIN 3

//connection panel
#define CNT_REDPIN 11
#define CNT_GREENPIN 10
#define CNT_BLUEPIN 9
#define FADESPEED 5

//stores RGB values in case we need to fade
int r = 0;
int b = 0; 
int g = 0; 
int RGB[3]; 

//Connction sensing parameters
unsigned int touchSensor = 0;  // holds the most recent accepted sensor value. 
unsigned int sensorBuffer = 0;  // holds a provisional sensor value while we look at it
float sensorAvg = 350; 
float numToAvg = 8;
float aFewSamplesAgo = 0; 
int sensorSampleCount = 0; 
int columnTimer = 1000/16; 
int columnTimerCount = 0; 

//WIFI and CapSense Setup
char packetBuffer[255]; //buffer to hold incoming packet
WiFiUDP Udp;
int irqpin = 2;  // Digital 2
boolean touchStates[12]; //to keep track of the previous touch states

int status = WL_IDLE_STATUS;
char ssid[] = "seth"; //  your network SSID (name) 
char pass[] = "internet";    // your network password (use for WPA, or use as key for WEP)
int keyIndex = 0;            // your network key Index number (needed only for WEP)

//******UNIQUE TO EACH GALILEO
int32_t galileoID = 0;         //set unique for 0-3 for each touchpoint
IPAddress remoteAddress(192,168,0,101);  //address of remote PC
char pingStr[] = "ping -c 1 192.168.0.101"; // ping once
int releaseMode = 0;  // 0 is debug // 1 is release (determines which network to join)
boolean useSerial = true; 

//***********SAME for each Galileo
unsigned int localPort = 12001;      // local port to listen on
uint16_t remoteP = 12000;            // remote port to send to
int refreshTimerCount = 0; 
int refreshTimer = 1000 * 60; // if no messages recieved for 1 minute ping server




void setup() {
  //Initialize serial and wait for port to open:
  if(useSerial) Serial.begin(9600); 
  setupWifi(); 
  
  pinMode(REDPIN, OUTPUT);
  pinMode(GREENPIN, OUTPUT);
  pinMode(BLUEPIN, OUTPUT);
  
  pinMode(CNT_REDPIN, OUTPUT);
  pinMode(CNT_GREENPIN, OUTPUT);
  pinMode(CNT_BLUEPIN, OUTPUT);
  
  pinMode(irqpin, INPUT);
  digitalWrite(irqpin, HIGH); //enable pullup resistor
  Wire.begin();
  mpr121_setup();
}

void loop() {
  if((millis() - columnTimerCount) > columnTimer) {
    readBetweenColumns(); 
    columnTimerCount = millis(); 
    //Serial.print(" sensorBuffer: "); 
    //Serial.println(sensorBuffer); 
  }
  readTouchInputs();
  checkIncomingMsgs(); 
}


// NOTE, there is a problem with the OSC format because it inserts a null character into the byte stream
// The UDP reader stops parsing packets if it encounters a null character. 
// So rather than changing the UDP parser, we are just sending a simple string and parsing ints from it. 
void checkIncomingMsgs() {
  char firstChar = '0'; 
  int packetSize = Udp.parsePacket();
  if(packetSize)
  {   
    //confirm that you have recieved a packet
    if(useSerial) Serial.print("Received packet of size ");
    if(useSerial) Serial.println(packetSize);
    // read the packet into packetBufffer
    int len = Udp.read(packetBuffer,255);
    
    //shift all values by one, add delimter, and increase the total lenth by 1 
    //to account for the first RGB value because incoming values will be r|g|b
    if (len >0) {
      //the first character is the panel or connector (p or c) 
      firstChar = packetBuffer[0]; 
      packetBuffer[len]=0; //null char for printing via serial debug
    }
    
    //confirm contents
    if(useSerial) Serial.print("Message type: ");
    if(useSerial) Serial.print(firstChar);
    if(useSerial) Serial.print(" Length: ");
    if(useSerial) Serial.print(len); 
    if(useSerial) Serial.print(" Contents_");
    if(useSerial) Serial.print(packetBuffer);
    if(useSerial) Serial.println("_");
    
      //read values backwards to the first character
      char tempInt[4] = {'0', '0', '0', '\0'};  //string of 0s (needs a null char at the end)
      int tempCnt = 2;  //number of possible digits 0-2
      int readCnt = 2;  //number of int values we are going to read in
      //count through backwards from low digits to high digits 
      for (int i = len-1; i > -1; i--){
        //look for delimiter (commas did not work for some reason) 
        if((packetBuffer[i] != '|') && (tempCnt > -1)) {
          //put it in the tempInt buffer ONLY if it is a number
          if((packetBuffer[i] > 47) && (packetBuffer[i] < 58)) {
            tempInt[tempCnt] = packetBuffer[i];
            tempCnt--;
          }
        } 
        else {
          RGB[readCnt] = atoi(tempInt);   //***** converts char to int
          readCnt--; 
          tempInt[0] = '0';
          tempInt[1] = '0';
          tempInt[2] = '0';
          tempCnt = 2;  //reset to the last digit in the
        }
      }
      
      //handler for a panel message
      if(firstChar == 'p') {
        r = RGB[0];
        g = RGB[1];
        b = RGB[2]; 
        setRGB(r,g,b);
      }
      
      //handler for a panel message
      if(firstChar == 'c') {
        setConnectionPanel(RGB[0],RGB[1],RGB[2]);
      }
      
    /*
    if(useSerial) Serial.print("red: "); 
    if(useSerial) Serial.print(r);
    if(useSerial) Serial.print(" green: "); 
    if(useSerial) Serial.print(g); 
    if(useSerial) Serial.print(" blue: "); 
    if(useSerial) Serial.println(b); 
    */ 
    
    //reset the last message recieved timer
    refreshTimerCount = millis();  
  } 
  
  //refresh timer currently causes the program to hang. 
  //a possible fix is to call ping as a non blocking system command set to exit? 
  if((millis() - refreshTimerCount) > refreshTimer) {
    //if(useSerial) Serial.println("pinging remote server to refresh connection"); 
    //system(pingStr);  //ping the server to establish a connection
    refreshTimerCount = millis(); 
  }  
}


//connection Panels
void setConnectionPanel(int red, int green, int blue) {
  analogWrite(CNT_REDPIN,red);
  analogWrite(CNT_GREENPIN,green); 
  analogWrite(CNT_BLUEPIN,blue);
}

//main touch panel
void setRGB(int red, int green, int blue) {
  analogWrite(REDPIN,red);
  analogWrite(GREENPIN,green); 
  analogWrite(BLUEPIN,blue);
}

void debugLED(int pin, int state) {
 //quick change of color when you touch values. 
  if(pin == 0) {
     if(state == 1) setRGB(255,0,0); 
     else  setRGB(0,0,0); 
 } else if (pin == 1) {
     if(state == 1) setRGB(255,255,0); 
     else  setRGB(0,0,0); 
 } else if (pin == 2) {
     if(state == 1) setRGB(255,0,255); 
     else  setRGB(0,0,0); 
 } else if (pin == 3) {
     if(state == 1) setRGB(0,255,255); 
     else  setRGB(0,0,0); 
 }
}

//reads input from a voltage divider with a filter to determine resistance
void readBetweenColumns() {
  //column 1
  sensorBuffer = analogRead(SENSOR_PIN);  // get new sensor value to examine
  sensorAvg = (sensorAvg * numToAvg + sensorBuffer)/(numToAvg + 1); //caclulates a running average to filter noise
  //check if the sensorAvg has shifted above Hysterisis since the average was caluculated 8 samples ago. 
  if(abs(aFewSamplesAgo-sensorAvg) >= SENSOR_HYSTERESIS) {
     Serial.print("SENDING_CHANGE: "); 
     Serial.println(sensorBuffer);
     int holdOrRelease = -1; 
     if(aFewSamplesAgo > sensorAvg) holdOrRelease = 1; 
     else holdOrRelease = 0; 
     sendResistiveChange(0, holdOrRelease, sensorBuffer);
     //reset both to current sample so that you only report one state change. 
     aFewSamplesAgo = sensorBuffer;
     sensorAvg = sensorBuffer;     
  }
  if((sensorSampleCount % (int)numToAvg) == 0) {
    aFewSamplesAgo = sensorAvg; 
  }
  sensorSampleCount++; 
}

//------------ UDP OSC Handlers -----------------

void sendReply() {
    OSCMessage msg("/gotMessage");
    msg.add((int32_t)1);
    Udp.beginPacket(remoteAddress, remoteP);
    msg.send(Udp); // send the bytes to the SLIP stream
    Udp.endPacket(); // mark the end of the OSC Packet
    msg.empty(); // free space occupied by message
}

void sendResistiveChange(int column, int holdRelease, int amount) {
    OSCMessage msg("/column"); 
    msg.add((int32_t)column);
    msg.add((int32_t)holdRelease); 
    msg.add((int32_t)amount);
    Udp.beginPacket(remoteAddress, remoteP);
    msg.send(Udp); // send the bytes to the SLIP stream
    Udp.endPacket(); // mark the end of the OSC Packet
    msg.empty(); // free space occupied by message
}

void sendTouchEvent(int pin, int state) {
    
    debugLED(pin,state);   
  
    OSCMessage msg("/touch");
    msg.add((int32_t)galileoID); 
    msg.add((int32_t)pin);
    msg.add((int32_t)state);
    Udp.beginPacket(remoteAddress, remoteP);
    msg.send(Udp); // send the bytes to the SLIP stream
    Udp.endPacket(); // mark the end of the OSC Packet
    msg.empty(); // free space occupied by message
}

//------------ WIFI FUNCTIONS -----------------
void setupWifi() {
  // check for the presence of the shield:
  Serial.println("starting connection to WIFI"); 
  
  if (WiFi.status() == WL_NO_SHIELD) {
    if(useSerial) Serial.println("WiFi shield not present"); 
    // don't continue:
    while(true);
  } 
  
  // attempts to connect to Wifi network every 5 seconds
  int numAttempts = 0; 
  while ( status != WL_CONNECTED) { 
   if(useSerial)  Serial.print("Attempting to connect to SSID: ");
   if(useSerial)  Serial.println(ssid);
   if(numAttempts > 0) {
     if(useSerial) Serial.println("Setting up wpa_supplicant manually"); 
     if(releaseMode == 0) {
       system("cp /etc/wpa_supplicant.seth /etc/wpa_supplicant.conf"); 
     } else if (releaseMode == 1) {
       system("cp /etc/wpa_supplicant.eyeo /etc/wpa_supplicant.conf"); 
     }
   }
    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:    
    status = WiFi.begin(ssid);
    // wait 5 seconds for connection:
    delay(5000);
    numAttempts++; 
  }
  
  if(useSerial) Serial.println("Connected to wifi");
  if(useSerial) printWifiStatus();
  if(useSerial)  Serial.println("\nStarting connection to server...");
  Udp.begin(localPort);  
}


void printWifiStatus() {
  // print the SSID of the network you're attached to:
  if(useSerial) {
    Serial.print("SSID: ");
    Serial.println(WiFi.SSID());

    // print your WiFi shield's IP address:
    IPAddress ip = WiFi.localIP();
    Serial.print("IP Address: ");
    Serial.println(ip);

    // print the received signal strength:
    long rssi = WiFi.RSSI();
    Serial.print("signal strength (RSSI):");
    Serial.print(rssi);
    Serial.println(" dBm");
  }
}

//------------ MPR121 Capsense Control -----------------

void readTouchInputs(){
  if(!checkInterrupt()){
    
    //read the touch state from the MPR121
    Wire.requestFrom(0x5A,2); 
    byte LSB = Wire.read();
    byte MSB = Wire.read();
    uint16_t touched = ((MSB << 8) | LSB); //16bits that make up the touch states

    for (int i=0; i < 12; i++){  // Check what electrodes were pressed
      if(touched & (1<<i)){
        if(touchStates[i] == 0){
          //pin i was just touched
         if(useSerial) {
            Serial.print("pin ");
            Serial.print(i);
            Serial.println(" was just touched");
          }
          sendTouchEvent(i,1); 
        }else if(touchStates[i] == 1){
          //pin i is still being touched
        }  
        touchStates[i] = 1;      
      }else{
        if(touchStates[i] == 1){
          if(useSerial) {
            Serial.print("pin ");
            Serial.print(i);
            Serial.println(" is no longer being touched");
          }
          sendTouchEvent(i,0); 
          //pin i is no longer being touched
       }
        touchStates[i] = 0;
      }
    }
  }
}


void mpr121_setup(void){

  set_register(0x5A, ELE_CFG, 0x00); 
  
  // Section A - Controls filtering when data is > baseline.
  set_register(0x5A, MHD_R, 0x01);
  set_register(0x5A, NHD_R, 0x01);
  set_register(0x5A, NCL_R, 0x00);
  set_register(0x5A, FDL_R, 0x00);

  // Section B - Controls filtering when data is < baseline.
  set_register(0x5A, MHD_F, 0x01);
  set_register(0x5A, NHD_F, 0x01);
  set_register(0x5A, NCL_F, 0xFF);
  set_register(0x5A, FDL_F, 0x02);
  
  // Section C - Sets touch and release thresholds for each electrode
  set_register(0x5A, ELE0_T, TOU_THRESH);
  set_register(0x5A, ELE0_R, REL_THRESH);
 
  set_register(0x5A, ELE1_T, TOU_THRESH);
  set_register(0x5A, ELE1_R, REL_THRESH);
  
  set_register(0x5A, ELE2_T, TOU_THRESH);
  set_register(0x5A, ELE2_R, REL_THRESH);
  
  set_register(0x5A, ELE3_T, TOU_THRESH);
  set_register(0x5A, ELE3_R, REL_THRESH);
  
  set_register(0x5A, ELE4_T, TOU_THRESH);
  set_register(0x5A, ELE4_R, REL_THRESH);
  
  set_register(0x5A, ELE5_T, TOU_THRESH);
  set_register(0x5A, ELE5_R, REL_THRESH);
  
  set_register(0x5A, ELE6_T, TOU_THRESH);
  set_register(0x5A, ELE6_R, REL_THRESH);
  
  set_register(0x5A, ELE7_T, TOU_THRESH);
  set_register(0x5A, ELE7_R, REL_THRESH);
  
  set_register(0x5A, ELE8_T, TOU_THRESH);
  set_register(0x5A, ELE8_R, REL_THRESH);
  
  set_register(0x5A, ELE9_T, TOU_THRESH);
  set_register(0x5A, ELE9_R, REL_THRESH);
  
  set_register(0x5A, ELE10_T, TOU_THRESH);
  set_register(0x5A, ELE10_R, REL_THRESH);
  
  set_register(0x5A, ELE11_T, TOU_THRESH);
  set_register(0x5A, ELE11_R, REL_THRESH);
  
  // Section D
  // Set the Filter Configuration
  // Set ESI2
  set_register(0x5A, FIL_CFG, 0x04);
  
  // Section E
  // Electrode Configuration
  // Set ELE_CFG to 0x00 to return to standby mode
  set_register(0x5A, ELE_CFG, 0x0C);  // Enables all 12 Electrodes
  
  // Section F
  // Enable Auto Config and auto Reconfig
  /*set_register(0x5A, ATO_CFG0, 0x0B);
  set_register(0x5A, ATO_CFGU, 0xC9);  // USL = (Vdd-0.7)/vdd*256 = 0xC9 @3.3V   set_register(0x5A, ATO_CFGL, 0x82);  // LSL = 0.65*USL = 0x82 @3.3V
  set_register(0x5A, ATO_CFGT, 0xB5);*/  // Target = 0.9*USL = 0xB5 @3.3V
  
  set_register(0x5A, ELE_CFG, 0x0C);
}

boolean checkInterrupt(void){
  return digitalRead(irqpin);
}

void set_register(int address, unsigned char r, unsigned char v){
    Wire.beginTransmission(address);
    Wire.write(r);
    Wire.write(v);
    Wire.endTransmission();
}

