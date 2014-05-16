
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

//Resistive Sensing 
#define SENSOR_HYSTERESIS 30  // ignore incremental changes smaller than this value...  needed to kill noise
#define NOISE_FLOOR 1000 // out of 1023, higher than this is considered open circuit.
#define SENSOR_PIN  A0  // also known as P14
unsigned int touchSensor = 0;  // holds the most recent accepted sensor value. 
unsigned int sensorBuffer = 0;  // holds a provisional sensor value while we look at it
float sensorAvg = 350; 
float numToAvg = 8;
float aFewSamplesAgo = 0; 
int sensorSampleCount = 0; 
int columnTimer = 1000/16; 
int columnTimerCount = 0; 

int status = WL_IDLE_STATUS;
char ssid[] = "ripandrunbaby"; //  your network SSID (name) 
char pass[] = "internet";    // your network password (use for WPA, or use as key for WEP)
int keyIndex = 0;            // your network key Index number (needed only for WEP)

int32_t galileoID = 0;         //set unique for 0-3 for each touchpoint
unsigned int localPort = 12001;      // local port to listen on
uint16_t remoteP = 12000;            // remote port to send to
IPAddress remoteAddress(10,0,1,9);  //address of remote PC

char packetBuffer[255]; //buffer to hold incoming packet
WiFiUDP Udp;
int irqpin = 2;  // Digital 2
boolean touchStates[12]; //to keep track of the previous touch states

boolean useSerial = true; 


void setup() {
  //Initialize serial and wait for port to open:
  if(useSerial) Serial.begin(9600); 
  setupWifi(); 
  
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
    OSCMessage msg("/touch");
    msg.add((int32_t)galileoID); 
    msg.add((int32_t)pin);
    msg.add((int32_t)state);
    Udp.beginPacket(remoteAddress, remoteP);
    msg.send(Udp); // send the bytes to the SLIP stream
    Udp.endPacket(); // mark the end of the OSC Packet
    msg.empty(); // free space occupied by message
}

void checkIncomingMsgs() {
  int packetSize = Udp.parsePacket();
  if(packetSize)
  {   
    if(useSerial) Serial.print("Received packet of size ");
    if(useSerial) Serial.println(packetSize);
    // read the packet into packetBufffer
    int len = Udp.read(packetBuffer,255);
    if (len >0) packetBuffer[len]=0;
    if(useSerial) Serial.print("Contents:");
    if(useSerial) Serial.println(packetBuffer);
    sendReply(); 
  }
}

//------------ WIFI FUNCTIONS -----------------

void setupWifi() {
  // check for the presence of the shield:
  if (WiFi.status() == WL_NO_SHIELD) {
    if(useSerial) Serial.println("WiFi shield not present"); 
    // don't continue:
    while(true);
  } 
  
  // attempts to connect to Wifi network every 5 seconds
  // TODO: sometimes you need to have a wpa_supplicant in order for wifi to initialize
  while ( status != WL_CONNECTED) { 
   if(useSerial)  Serial.print("Attempting to connect to SSID: ");
   if(useSerial)  Serial.println(ssid);
    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:    
    status = WiFi.begin(ssid);
    // wait 10 seconds for connection:
    delay(5000);
  }
  
  if(useSerial) Serial.println("Connected to wifi");
  printWifiStatus();
  if(useSerial)  Serial.println("\nStarting connection to server...");
  
  //start the UDP connection
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

