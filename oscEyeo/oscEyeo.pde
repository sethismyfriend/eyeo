/**
 Modification of OSC Send Recieve to recieve incoming packets from the Galileo for EYEO
 Note: make sure McAffe Firewall is OFF and Windows Firewall is off before testing. 
 */
 
import oscP5.*;
import netP5.*;
  
OscP5 oscP5;
NetAddress myRemoteLocation;

//holds the state of all touch points
int gridSize = 12; 
int numGrids = 4; 
int[] grid0 = {0,0,0,0,0,0,0,0,0,0,0,0};
int[] grid1 = {0,0,0,0,0,0,0,0,0,0,0,0}; 
int[] grid2 = {0,0,0,0,0,0,0,0,0,0,0,0}; 
int[] grid3 = {0,0,0,0,0,0,0,0,0,0,0,0}; 
int[][] allGrids = { grid0, grid1, grid2, grid3 };

int numColumns = 2; 
int[] columns = { 0, 0 };
int[] columnState = { 0, 0 }; 

void setup() {
  size(600,550);
  frameRate(25);
  /* start oscP5, listening for incoming messages at port 12000 */
  oscP5 = new OscP5(this,12000);
  
  /* myRemoteLocation is a NetAddress. a NetAddress takes 2 parameters,
   * an ip address and a port number. myRemoteLocation is used as parameter in
   * oscP5.send() when sending osc packets to another computer, device, 
   * application. usage see below. for testing purposes the listening port
   * and the port of the remote location address are the same, hence you will
   * send messages back to this sketch.
   */
  myRemoteLocation = new NetAddress("10.0.1.17",12001);
  // myRemoteLocation = new NetAddress("127.0.0.1",12000);
}

void draw() {
  background(0);  
  drawGrids(); 
  drawColumns(); 
}

void updateColumn(int c, int holdRelease, int amount) {
  columns[c] = amount; 
  columnState[c] = holdRelease; 
}
  
void drawColumns() {
  int margin = 10;
  int colW = 15;
  int colH = 100;
  int startX = width/2 - ((margin + colW) * numColumns)/2; 
  int startY = colH + margin; 
  for(int i=0; i < numColumns; i++) {
    //note = map(touchSensor, 0, 1023, HIGHEST_NOTE, LOWEST_NOTE);
    float tempH = -10; 
    if(columns[i] > 300) {
      tempH =  map((float)columns[i], (float)400, (float)300, (float)0, (float)colH); 
      tempH *= -1; 
    }
    if(columnState[i] == 0) fill(100,100,100); //off
    else if(columnState[i] == 1) fill(255,0,0);  // on
    rect(startX, startY, colW, (int)tempH); 
    startX += margin+colW; 
  } 
}


void updateGrid(int galileo, int pin, int state) {
  allGrids[galileo][pin] = state; 
}

void drawGrids() {
  int square = 30;
  int margin = 8;
  int gridMargin = margin*3; 
  int limitY = 4; 
  int countY = 0;
  int curX = 10; 
  int curY = height - square*3;
  //generate a grid of squares to monitor each touchpoint
  for(int i=0; i<numGrids; i++) {
    for(int j=0; j<gridSize; j++) {
      if(allGrids[i][j] == 1) fill(255,0,0); 
      else fill(100,100,100); 
      curY -= square + margin; 
      if(j % limitY == 0) {
        curY = height - square*2; 
        curX += margin + square;
      }
      rect(curX,curY,square,square); 
    }
    curX += gridMargin;
  }
}


void mousePressed() {
  /* in the following different ways of creating osc messages are shown by example */
  OscMessage myMessage = new OscMessage("/test");
  myMessage.add(123); /* add an int to the osc message */
  /* send the message */
  oscP5.send(myMessage, myRemoteLocation); 
}


/* incoming osc message are forwarded to the oscEvent method. */
void oscEvent(OscMessage theOscMessage) {
  /* print the address pattern and the typetag of the received OscMessage */
  /*
  print("### received an osc message.");
  print(" addrpattern: "+theOscMessage.addrPattern());
  println(" typetag: "+theOscMessage.typetag());
  */
  
  if(theOscMessage.checkAddrPattern("/touch")==true) {
    /* check if the typetag is the right one. */
    if(theOscMessage.checkTypetag("iii")) {
      /* parse theOscMessage and extract the values from the osc message arguments. */
      int galileo = theOscMessage.get(0).intValue();
      int pin = theOscMessage.get(1).intValue();  
      int state = theOscMessage.get(2).intValue();
      updateGrid(galileo,pin,state); 
      return;
    }
  }
  
    if(theOscMessage.checkAddrPattern("/column")==true) {
    /* check if the typetag is the right one. */
    if(theOscMessage.checkTypetag("iii")) {
      /* parse theOscMessage and extract the values from the osc message arguments. */
      int column = theOscMessage.get(0).intValue();
      int holdRelease = theOscMessage.get(1).intValue();  
      int amount = theOscMessage.get(2).intValue();
      
      println("got column event:" + holdRelease + " amount:" +  amount); 

      updateColumn(column,holdRelease,amount);
      return;
    }
  }
  
}
