/**
 * EtherShield webserver restduino for ENC28J60 shield, Nanode or EtherCard
 * Based on RESTduino http://jasongullickson.posterous.com/restduino-arduino-hacking-for-the-rest-of-us
 * 
 * See http://blog.thiseldo.co.uk/?p=599 for details on how to use this.
 * This is an updated version of the sketch written for the EtherCard library
 *
 * Written By and (c) Andrew D Lindsay, May 2011
 * http://blog.thiseldo.co.uk
 * Feel free to use this code, modify and redistribute.
 * 
 *Hej John
 */
#define DEBUG 1
//#undef DEBUG
// Use this if using a nanode so it gets MAC address from onboard chip
//#define NANODE
#include <PString.h>

#include <OneWire.h>
#include <DallasTemperature.h>
// Data wire is plugged into pin 2 on the Arduino
#define ONE_WIRE_BUS 3

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);
 
// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);


#define ULONG_MAX 4294967295
#define OneWireNextRead 4000                       //Frequency to measure temperature (ms)
volatile unsigned long _TemperatureNextRead = ULONG_MAX;   //Next time to read the temperature (ms)

#define NumberOfDevices 5                          // Set maximum number of devices in order to dimension 
                                                   // Array holding all Device Address arrays.
byte allAddress [NumberOfDevices][8];              // Device Addresses are 8-element byte arrays.
                                                   // we need one for each of our DS18B20 sensors.
byte totalDevices;                                 // Declare variable to store number of One Wire devices
                                                   // that are actually discovered.
char response[NumberOfDevices * 40];

#include "EtherCard.h"

// please modify the following three lines. mac and ip have to be unique
// in your local area network. You can not have the same numbers in
// two devices:
static uint8_t mymac[6] = { 0x54,0x55,0x58,0x10,0x20,0x35 };
static uint8_t myip[4] = { 192,168,1,203};
static uint8_t gwip[4] = { 192,168,1,1 };

// listen port for tcp/www (max range 1-254)
#define MYWWWPORT 80   

#define BUFFER_SIZE 700
byte Ethernet::buffer[BUFFER_SIZE];
BufferFiller bfill;

#define STR_BUFFER_SIZE 22
static char strbuf[STR_BUFFER_SIZE+1];



uint16_t http200ok(void)

{
  bfill = ether.tcpOffset();
  bfill.emit_p(PSTR(
    "HTTP/1.0 200 OK\r\n"
    "Content-Type: text/html\r\n"
    "Pragma: no-cache\r\n"
    "\r\n"));
  return bfill.position();
}

uint16_t http404(void)
{
  bfill = ether.tcpOffset();
  bfill.emit_p(PSTR(
    "HTTP/1.0 404 OK\r\n"
    "Content-Type: text/html\r\n"
    "\r\n"));
  return bfill.position();
}

// prepare the webpage by writing the data to the tcp send buffer
uint16_t print_webpage()
{
  bfill = ether.tcpOffset();
  bfill.emit_p(PSTR(
    "HTTP/1.0 200 OK\r\n"
    "Content-Type: text/html\r\n"
    "Pragma: no-cache\r\n"
    "\r\n"
    "<html><body>use /temp to get temps</body></html>"));
  return bfill.position();
}

// prepare the webpage by writing the data to the tcp send buffer
uint16_t print_temppage()
{  
  PString tempResponse(response, sizeof(response));
  tempResponse.print("{\"temperatures\" : [");
    
    for (byte i=0; i < totalDevices; i++)   {
      tempResponse += "{\"address\" : \"";
      for (uint8_t k = 0; k < 8; k++) {
         if (allAddress[i][k] < 16) { 
           tempResponse += '0';
         }
         tempResponse.print(allAddress[i][k],HEX);
       }
    tempResponse += "\", \"value\":";   
    tempResponse += sensors.getTempC(allAddress[i]);
        if (i<totalDevices-1) (tempResponse += "}, ");
        else tempResponse += "}";   
   }
   tempResponse += "]}";
  #ifdef DEBUG
  Serial.println( tempResponse );
#endif
  
  bfill = ether.tcpOffset();
  bfill.emit_p(PSTR(
    "HTTP/1.0 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "Pragma: no-cache\r\n"
        "\r\n"
        "$S"),  response);
  return bfill.position();
}

// prepare the webpage by writing the data to the tcp send buffer
uint16_t print_resetpage()
{  
  bfill = ether.tcpOffset();
  bfill.emit_p(PSTR(
    "HTTP/1.0 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "Pragma: no-cache\r\n"
        "\r\n"
        "OK - Reset"));
  return bfill.position();
}


#define CMDBUF 50



int16_t process_request(char *str)
{
  int8_t r=-1;
  int8_t i = 0;
  char clientline[CMDBUF];
  int index = 0;
  int plen = 0;
  
#ifdef DEBUG
  Serial.println( str );
#endif

  char ch = str[index];
  
  while( ch != ' ' && index < CMDBUF) {
    clientline[index] = ch;
    index++;
    ch = str[index];
  }
  clientline[index] = '\0';

#ifdef DEBUG
  Serial.println( clientline );
#endif

  // convert clientline into a proper
  // string for further processing
  String urlString = String(clientline);

  // extract the operation
  String op = urlString.substring(0,urlString.indexOf(' '));

  // we're only interested in the first part...
  urlString = urlString.substring(urlString.indexOf('/'), urlString.indexOf(' ', urlString.indexOf('/')));

  // put what's left of the URL back in client line
  urlString.toCharArray(clientline, CMDBUF);

  // get the first two parameters
  char *pin = strtok(clientline,"/");
  char *value = strtok(NULL,"/");

  // this is where we actually *do something*!
  char outValue[10] = "MU";
  char jsonOut[50];

  if(pin != NULL){
    if(value != NULL){

      // set the pin value
#ifdef DEBUG
      Serial.println("setting pin");
#endif
            
      // select the pin
      int selectedPin = pin[0] -'0';
#ifdef DEBUG
      Serial.println(selectedPin);
#endif

      // set the pin for output
      pinMode(selectedPin, OUTPUT);
            
      // determine digital or analog (PWM)
      if(strncmp(value, "HIGH", 4) == 0 || strncmp(value, "LOW", 3) == 0){
              
        // digital
#ifdef DEBUG
        Serial.println("digital");
#endif
              
        if(strncmp(value, "HIGH", 4) == 0){
#ifdef DEBUG
          Serial.println("HIGH");
#endif
          digitalWrite(selectedPin, HIGH);
        }
              
        if(strncmp(value, "LOW", 3) == 0){
#ifdef DEBUG
          Serial.println("LOW");
#endif
          digitalWrite(selectedPin, LOW);
        }
              
      } else {
              
        // analog
#ifdef DEBUG
        Serial.println("analog");
#endif
        // get numeric value
        int selectedValue = atoi(value);
#ifdef DEBUG
        Serial.println(selectedValue);
#endif
              
        analogWrite(selectedPin, selectedValue);
              
      }
      // return status
      return( http200ok() );
        
    } else {

      // read the pin value
#ifdef DEBUG
      Serial.println("reading pin");
#endif

      // determine analog or digital
      if(pin[0] == 'a' || pin[0] == 'A'){

        // analog
        int selectedPin = pin[1] - '0';

#ifdef DEBUG
        Serial.println(selectedPin);
        Serial.println("analog");
#endif

        sprintf(outValue,"%d",analogRead(selectedPin));
              
#ifdef DEBUG
        Serial.println(outValue);
#endif

      } else if(pin[0] != NULL) {

        // digital
        int selectedPin = pin[0] - '0';

#ifdef DEBUG
        Serial.println(selectedPin);
        Serial.println("digital");
#endif

        pinMode(selectedPin, INPUT);
              
        int inValue = digitalRead(selectedPin);
              
        if(inValue == 0){
          sprintf(outValue,"%s","LOW");
        }
              
        if(inValue == 1){
          sprintf(outValue,"%s","HIGH");
        }
              
#ifdef DEBUG
        Serial.println(outValue);
#endif
      }

      // assemble the json output reply - put straight into packet buffer
      bfill = ether.tcpOffset();
      bfill.emit_p(PSTR(
        "HTTP/1.0 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "Pragma: no-cache\r\n"
        "\r\n"
        "{\"$S\":\"$S\"}"), pin, outValue);
      return bfill.position();     
    }
  } else {
          
    // error
#ifdef DEBUG
    Serial.println("erroring");
#endif
    plen = http404();
  }
 
  return plen;
}



void setup(){

#ifdef DEBUG
  Serial.begin(9600);
  Serial.println("EtherCard RESTduino");
#endif

  //Schedule next temperature read in 1 second, allows plenty of
  //time for interrupt handler to be set
  _TemperatureNextRead = millis() + OneWireNextRead;

  if (ether.begin(sizeof Ethernet::buffer, mymac) == 0) {
    #ifdef DEBUG
        Serial.println( "Failed to access Ethernet controller");
    #endif
    while(1);
  }
  
  // Setup static IP address
  ether.staticSetup(myip, gwip);

  // Start up temp sensors
  sensors.begin(); // IC Default 9 bit. If you have troubles consider upping it 12. Ups the delay giving the IC more time to process the temperature measurement
  totalDevices = discoverOneWireDevices();         // get addresses of our one wire devices into allAddress array 
  
  for (byte i=0; i < totalDevices; i++) 
    sensors.setResolution(allAddress[i], 10);      // and set the a to d conversion resolution of each.   
    
#ifdef DEBUG
  Serial.println("Ready");
#endif

}


byte discoverOneWireDevices() {
  
  byte j=0;                                        // search for one wire devices and copy to device address arrays.
  while ((j < NumberOfDevices) && (oneWire.search(allAddress[j]))) {        
    j++;
  }  
  return j;                 // return total number of devices found.
}

void printtempserial() {
  for (byte j = 0; j < totalDevices; j++)   {
     for (uint8_t i = 0; i < 8; i++) {
       if (allAddress[j][i] < 16){ 
         Serial.print("0");
     }
       Serial.print(allAddress[j][i], HEX);
     } 
   Serial.print(":");
   Serial.println(sensors.getTempC(allAddress[j]));
   }
 }
 
 
 
 
void loop(){
  
  uint16_t plen, dat_p;
  
  int8_t cmd;

  while(1) {
    
    
    if(millis() >= _TemperatureNextRead) {
    //
  
    sensors.requestTemperatures(); // Send the command to get temperatures
    _TemperatureNextRead = millis() + OneWireNextRead;
   
   #ifdef DEBUG
      printtempserial();
   #endif
  }
  
    // read packet, handle ping and wait for a tcp packet:
   dat_p=ether.packetLoop(ether.packetReceive());

    /* dat_p will be unequal to zero if there is a valid http get */
    if(dat_p==0){
      // no http request
      continue;
    }

    // tcp port 80 begin
    if (strncmp("GET ",(char *)&(Ethernet::buffer[dat_p]),4)!=0){
      // head, post and other methods:
      dat_p = print_webpage();
      
      goto SENDTCP;
    }

    // just one web page in the "root directory" of the web server
    if (strncmp("/ ",(char *)&(Ethernet::buffer[dat_p+4]),2)==0){
#ifdef DEBUG
      Serial.println("GET / request");
#endif
      dat_p = print_webpage();
      goto SENDTCP;
    }
    
    // just one web page in the "root directory" of the web server
    if (strncmp("/temp",(char *)&(Ethernet::buffer[dat_p+4]),5)==0){
#ifdef DEBUG
      Serial.println("GET /temp request");
#endif
      dat_p = print_temppage();
      goto SENDTCP;
    }
    // just one web page in the "root directory" of the web server
    if (strncmp("/reset",(char *)&(Ethernet::buffer[dat_p+4]),6)==0){
#ifdef DEBUG
      Serial.println("GET /reset request");
#endif
      // Start up temp sensors
      sensors.begin(); // IC Default 9 bit. If you have troubles consider upping it 12. Ups the delay giving the IC more time to process the temperature measurement
      totalDevices = discoverOneWireDevices();         // get addresses of our one wire devices into allAddress array 
  
      for (byte i=0; i < totalDevices; i++) 
        sensors.setResolution(allAddress[i], 10);      // and set the a to d conversion resolution of each.   
      
      dat_p = print_resetpage();
      goto SENDTCP;
    }
    
    dat_p = process_request((char *)&(Ethernet::buffer[dat_p+4]));
    
SENDTCP:
      if( dat_p )
        ether.httpServerReply( dat_p);

    // tcp port 80 end
  }

  
}


