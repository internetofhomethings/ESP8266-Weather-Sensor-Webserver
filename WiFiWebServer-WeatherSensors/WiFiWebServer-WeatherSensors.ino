

/*
 *  This sketch based on the example simple HTTP-like server.
 *  The server will perform 3 functions depending on the request
 *  1.  http://ESP8266-IP:SVRPORT/gpio/0 will set the GPIO16 low,
 *  2.  http://ESP8266-IP:SVRPORT/gpio/1 will set the GPIO16 high
 *  3.  http://ESP8266-IP:SVRPORT/?request=GetSensors will return a json string with sensor values
 *
 *  ESP8266-IP is the IP address of the ESP8266 module
 *  SVRPORT is the TCP port number the server listens for connections on
 */
 
#include <OneWire.h>

#include <ESP8266WiFi.h>
#include <Wire.h>
#include <DHT.h>

#include <Adafruit_BMP085.h>
#include <UtilityFunctions.h>

extern "C" {
#include "user_interface.h"
}
//Server actions
#define SET_LED_OFF 0
#define SET_LED_ON  1
#define Get_SENSORS 2

#define SERBAUD 9600
#define SVRPORT 9701
#define ONEJSON 0
#define FIRSTJSON 1
#define NEXTJSON 2
#define LASTJSON 3

//GPIO pin assignments
#define DS18B20 5       // Temperature Sensor pin (DS18B20)
#define I2C_SCL 12      // Barometric Pressure Sensor (BMP085)
#define I2C_SDA 13      // Barometric Pressure Sensor (BMP085)
#define DHTPIN 14       // Temp/Humidity GPIO pin (DHT11)
#define LED_IND 16      // LED used for initial code testing (not included in final hardware design)

#define DHTTYPE DHT11   // DHT 11 

const char* ssid = "<wifi-ssid>";          //your wifi ssid
const char* password = "<wifi-password>";  //your wifi password
const uint8_t ipadd[4] = {192,168,0,132};  //static ip assigned to ESP8266
const uint8_t ipgat[4] = {192,168,0,1};    //local router gateway ip
const uint8_t ipsub[4] = {255,255,255,0};

//globals
int lc=0;
bool complete=false;
bool bmp085_present=true;
float bt,bp,ba,dhtt,dhth;
char btmp[20],bprs[20],balt[20],tin[20],tout[20],tatt[20],dhhumi[20],dhtemp[20]; 
uint32_t state=0;
char szT[30];

// Create an instance of the server
// specify the port to listen on as an argument
WiFiServer server(SVRPORT);
Adafruit_BMP085 bmp;
OneWire  ds(5);  // on pin 5 for ds18b20
DHT dht(DHTPIN, DHTTYPE, 15);

void printStatus(char * status, int s) {
    //Serial.print("Free heap: ");
    Serial.print(system_get_free_heap_size());
    delay(100);
    //Serial.print(" Time(sec): ");
    Serial.print(" ");
    delay(100);
    Serial.print(millis()/1000);
    delay(100);
    Serial.print(status);
    delay(100);
    if(s>=0) Serial.println(s);
    else Serial.println("");
    delay(100);
}
void startWIFI(void) {
  // Connect to WiFi network
  Serial.println();
  delay(10);
  Serial.println();
  delay(10);
  Serial.print("Connecting to ");
  delay(10);
  Serial.println(ssid);
  delay(10);
  WiFi.config(ipadd, ipgat, ipsub);  //dsa added 12.04.2015
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("");
  Serial.println("WiFi connected");
  
  // Start the server
  server.begin();
  Serial.println("Server started");

  // Print the IP address
  Serial.print("ESP8266 IP: ");
  Serial.println(WiFi.localIP());

  Serial.print("ESP8266 WebServer Port: ");
  Serial.println(SVRPORT);
  delay(300);

}
void getTempDs18b20(int sensor, char * val) {
  
  byte i;
  byte present = 0;
  byte type_s;
  byte addr[8];
  byte data[12];
  float celsius, fahrenheit;
  
  switch(sensor) {
    case 1:
	//Inside Sensor
	addr[0]=0x10;
	addr[1]=0x2E;
	addr[2]=0x4B;
	addr[3]=0x2F;
	addr[4]=0x00;
	addr[5]=0x08;
	addr[6]=0x00;
	addr[7]=0x5B;
	break;
    case 2:
	//Outside Sensor
	addr[0]=0x10;
	addr[1]=0x13;
	addr[2]=0x45;
	addr[3]=0x2F;
	addr[4]=0x00;
	addr[5]=0x08;
	addr[6]=0x00;
	addr[7]=0x5E;
	break;
    case 3:
        //Attic Sensor
	addr[0]=0x10;
	addr[1]=0xCB;
	addr[2]=0x45;
	addr[3]=0x2F;
	addr[4]=0x00;
	addr[5]=0x08;
	addr[6]=0x00;
	addr[7]=0x3B;
	break;
  }
  /*
  if (OneWire::crc8(addr, 7) != addr[7]) {
      Serial.println("CRC is not valid!");
      return;
  }
  */
  type_s = 1; //DS18S20
  ds.reset();
  ds.select(addr);
  ds.write(0x44,1);         // start conversion, with parasite power on at the end
  
  delay(1000);     // maybe 750ms is enough, maybe not
  // we might do a ds.depower() here, but the reset will take care of it.
  
  present = ds.reset();
  ds.select(addr);    
  ds.write(0xBE);         // Read Scratchpad
  
  for ( i = 0; i < 9; i++) {           // we need 9 bytes
    data[i] = ds.read();
  }
  ds.reset();
  unsigned int raw = (data[1] << 8) | data[0];
  if (type_s) {
    raw = raw << 3; // 9 bit resolution default
    if (data[7] == 0x10) {
      // count remain gives full 12 bit resolution
      raw = (raw & 0xFFF0) + 12 - data[6];
    }
  }
  celsius = (float)raw / 16.0;
  fahrenheit = celsius * 1.8 + 32.0;
  ftoa(fahrenheit,val, 1);
}


void readSensorIsr() {
  yield();
  switch(state++) {
    case 0:
      getTempDs18b20(1, tin);
      break;
    case 1:
      getTempDs18b20(2, tout);
      break;
    case 2:
      getTempDs18b20(3, tatt);
      break;
    case 3: //Read Baro Temperature
      if(bmp085_present) {
        bt = (bmp.readTemperature() * 9/5) + 32;
        ftoa(bt,btmp, 1);
      }
      else {
        strcpy(btmp,tin);
      }
      break;
    case 4: //Read Baro Pressure
      if(bmp085_present) {
        bp = bmp.readPressure() / 3386;
        ftoa(bp,bprs, 1);
      }
      else {
        strcpy(btmp,"29.5");
      }
      break;
    case 5: //Calc Baro Altitude (does not work-pow from math.h library not linked properly)
      //ba = bmp.readAltitude(bmp.readSealevelPressure(555/3.28)) * 3.28;
      ba = 550.0;
      ftoa(ba,balt, 1);
      break;
    case 6:
      dhth = dht.readHumidity();
      ftoa(dhth,dhhumi, 1);
      break;
    case 7:
      dhtt = dht.readTemperature(true); 
      ftoa(dhtt,dhtemp, 1);
      state=0;
      break;
    default:
      break;
  }
  ESP.wdtFeed(); 
  yield();
}

void jsonAdd(String *s, String key,String val) {
    *s += '"' + key + '"' + ":" + '"' + val + '"';
}
void jsonEncode(int pos, String * s, String key, String val) {
    switch (pos) {
      case ONEJSON:      
      case FIRSTJSON:
        *s += "{\r\n";
        jsonAdd(s,key,val);
        *s+= (pos==ONEJSON) ? "\r\n}" : ",\r\n";
        break;
      case NEXTJSON:    
        jsonAdd(s,key,val);
        *s+= ",\r\n";
        break;
      case LASTJSON:    
        jsonAdd(s,key,val);
        *s+= "\r\n}";
        break;
    }
}
void killclient(WiFiClient client, bool *busy) {
  lc=0;
  delay(1);
  client.flush();
  client.stop();
  complete=false;
  *busy = false;  
}
void sysloop() {
  static bool busy=false;
  static int timeout_busy=0;
  //connect wifi if not connected
  if (WiFi.status() != WL_CONNECTED) {
    delay(1);
    startWIFI();
    return;
  }
  //return if busy
  if(busy) {
    delay(1);
    if(timeout_busy++ >10000) {
      printStatus((char *)" Status: Busy timeout-resetting..",-1);
      ESP.reset(); 
      busy = false;
    }
    return;
  }
  else {
    busy = true;
    timeout_busy=0;
  }
  delay(1);
  //Read 1 sensor every 2.5 seconds
  if(lc++>2500) {
    lc=0;
    printStatus((char *)" ",state);
    readSensorIsr(); 
    busy = false;
    return;   
  }
  // Check if a client has connected
  WiFiClient client = server.available();
  if (!client) {
     busy = false;
     return;
  } 
  // Wait until the client sends some data
  while((!client.available())&&(timeout_busy++<5000)){
    delay(1);
    if(complete==true) {
      killclient(client, &busy);
      return;
    }
  }
  //kill client if timeout
  if(timeout_busy>=5000) {
    killclient(client, &busy);
    return;
  }
  
  complete=false; 
  ESP.wdtFeed(); 
  
  // Read the first line of the request
  String req = client.readStringUntil('\r');
  client.flush();
  if (req.indexOf("/favicon.ico") != -1) {
    client.stop();
    complete=true;
    busy = false;
    return;
  }
  Serial.print("Recv http: ");  
  Serial.println(req);
  delay(100);
  
    // Match the request
  int val;
  if (req.indexOf("/gpio/0") != -1)
    val = SET_LED_OFF;
  else if (req.indexOf("/gpio/1") != -1)
    val = SET_LED_ON;
  else if (req.indexOf("/?request=GetSensors") != -1) {
    val = Get_SENSORS;
    //Serial.println("Get Sensor Values Requested");
  }  
  else {
    Serial.println("invalid request");
    client.stop();
    complete=true;
    busy = false;
    return;
  }
  client.flush();

  // Prepare Response header
  String s = "HTTP/1.1 200 OK\r\n";
  String v ="";
  ESP.wdtFeed(); 
      
  switch (val) {
    case SET_LED_OFF:
    case SET_LED_ON:
      // Set GPIO4 according to the request
      digitalWrite(LED_IND , val);
  
      // Prepare the response for GPIO state
      s += "Content-Type: text/html\r\n\r\n";
      s += "<!DOCTYPE HTML>\r\nGPIO is now ";
      s += (val)?"high":"low";
      s += "</html>\n";
      // Send the response to the client
      client.print(s);
      break;
    case Get_SENSORS:
      //Create JSON return string
      s += "Content-Type: application/json\r\n\r\n";
      jsonEncode(FIRSTJSON,&s,"B_Pressure", bprs);
      jsonEncode(NEXTJSON,&s,"B_Temperature", btmp);
      jsonEncode(NEXTJSON,&s,"B_Altitude", balt);
      jsonEncode(NEXTJSON,&s,"DS_TempInside", tin);
      jsonEncode(NEXTJSON,&s,"DS_TempOutside", tout);
      jsonEncode(NEXTJSON,&s,"DS_TempAttic", tatt);
      jsonEncode(NEXTJSON,&s,"DH_Humidity", dhhumi);
      jsonEncode(NEXTJSON,&s,"DH_Temperature", dhtemp);
      v = system_get_free_heap_size();
      jsonEncode(NEXTJSON,&s,"SYS_Heap", v);
      v = millis()/1000;
      jsonEncode(LASTJSON,&s,"SYS_Time", v);
      // Send the response to the client
      client.print(s);
      yield();
      //ESP.wdtFeed(); 
      break;
    default:
      break;
   }

    delay(1);
    v ="";
    s="";
    val=0;
    Serial.println("Ending it: Client disconnected");
    delay(150);
    complete=true;
    busy = false;
    ESP.wdtFeed(); 
  

  // The client will actually be disconnected 
  // when the function returns and 'client' object is destroyed
}

void setup() {
  Serial.begin(SERBAUD);
  delay(10);
  startWIFI();
  
  // Set Indicator LED as output
  pinMode(LED_IND , OUTPUT);
  digitalWrite(LED_IND, 0);

  // Setup BMP085 (i2c)
  Wire.begin(I2C_SDA, I2C_SCL);
  
  if (!bmp.begin()) {
    Serial.println("No BMP085 sensor detected!");
    bmp085_present=false;
  }

  // Print Free Heap
  printStatus((char *)" Status: End of Setup",-1);
  delay(500);
  
}

void loop() {
    sysloop();
}
