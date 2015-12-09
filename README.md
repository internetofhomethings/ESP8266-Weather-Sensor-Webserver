# ESP8266-WeatherSensor-Webserver

<h2><strong>ESP8266 Webserver for Weather Sensors</strong></h2>

<strong>Installation Instructions</strong>

1. Sketch compiled using Arduino IDE Version 1.6.5
2. Copy the WiFiWebServer-WeatherSensors folder to your Arduino sketch folder.
3. Copy the UtilityFunctions folder to your Arduino libraries folder.
4. Copy the Adafruit_BMP085 folder to your Arduino libraries folder.
5. Change the following sketch lines to match your network settings:

const char* ssid = "<wifi-ssid>";          //your wifi ssid<br>
const char* password = "<wifi-password>";  //your wifi password<br>
const uint8_t ipadd[4] = {192,168,0,132};  //static ip assigned to ESP8266<br>
const uint8_t ipgat[4] = {192,168,0,1};    //local router gateway ip<br>

<strong>About this sketch</strong>

Here is what I came up with when attempting to port my Home weather sensors from a Spark Core MPU to an ESP8266. I used the Arduino IDE for code development. While the sketch framework can easily be expanded to support other sensor types, the   sensors supported with this code include:

1. Barometric Pressure: BMP085
2. Temperature:         DS18B20
3. Humidity:            DHT11

This sketch implements a webserver that returns a JSON string containing the sensor values.  

After many iterations, discovering what worked and what wouldn't function, I came up with the following structure:

Everything worked "off-the-shelf" except for the BMP085 driver. The problem was that the "pow" function, used to calculate altitude, was not linked properly from the built-in IDE libraries. And if I tried to include "math.h",  which includes the "pow" function, the compiler failed with an out of memory error. I also attempted to implement a recursively called substitute function for the missing "pow"...unsuccessfully. I ended up removing the calls to the altitude function, and the need for the pow function. Not a big loss considering the fact that my sensors are positioned in a fixed location, the altitude will never change. My implementation of the pow function is in the UtilityFunctions.c file in case someone may wish to explore this further.

<strong>Structure of the Arduino IDE loop()</strong>

Here are the key attributes of my code required for the most reliable operation:

<strong>WiFi connected check</strong>

First thing I added to the top of the sketches loop was a check to determine if the WiFi was still connected. This became necessary when I noticed that sometimes the connection was dropped, resulting in a non-responsive ESP8266 to"http GET" requests.

<strong>Busy flag</strong>

As you may well know, sending an "http GET" request by entering an URL into a web browser also creates

several request for "favicon". This sometimes created a problem when the ESP8266 sent it's reply back to the browser and returned to the top of the loop. It appears that the reply, sent using "client.print(msg);" is a non-blocking call. That means the ESP8266 continues execution while the reply message send is in progress, This results in cases where the "favicon" request is received before the reply is sent. I figured this may be the cause for some of the ESP8266 lock-ups and resets I was experiencing. So I added a busy flag to block the processing of any new "http GET" requests until the current one is complete.

<strong>Sensor Reads</strong>

When all the sensor reads were attempted each iteration of the loop(), the ESP8266 kept resetting. I believe this was because the watchdog timer, set to about 5 seconds by default and does not appear to be controllable at this time, would timeout before the sensor reads were complete. Upon a timeout, the ESP8266 resets.

The solution was to limit the sensor reads to one read every 2.5 seconds. or a total of about 20 seconds to refresh all 8 sensors. This worked, and the resets no longer occurred endlessly.

<strong>Returns</strong>

The watchdog timeouts and subsequent resets  occured frequently when all the steps in the sketch loop were executed every iteration. This was significantly reduced by returning from the loop after each significant event was processed.

Loop sequence returns:

After Wifi connected, if needed
If busy
After a sensor is read
If no client detected
After client is killed
If "favicon" request detected

<strong>Watchdog Timer Resets</strong>

The wdt_feed() should reset the watchdog timer. I have sprinkled some calls to wdt_feed() in my loop() after tasks that take some time to complete to avoid timeout resets.

<strong>Reply - json string encoding</strong>

The sensor data is returned as a json string for easy processing with a php or jquery script. I have attempted to add a few different json libraries to my Arduino IDE sketch, without success. They either would not compile or blew the memory space. So I ended up adding a simple json encoder to my sketch. It only supports key:value entries at the top level, but works flawlessly and uses an absolute minimum amount of memory. Check it out in my sketch.

<strong>Heartbeat Data Logging</strong>

With all the problems I had with memory management and leaks using the nodeMCU/lua environment, I added a serial print to log 3 parameters:

  1. free heap
  2. processor time since last reset
  3. last sensor read

This was output every time a sensor was read (2.5 second intervals) and logged to a file using my terminal program. It has been very helpful in debugging problems. just like with lua and the SDK, I noticed the free heap drops for each consecutive "http GET", lingering for a minute or so. This means in the current state of the ESP8266 hardware/software combination, you cannot continuously bang the unit with "http GET" requests. For applications that need to periodically extract information from the module over the network, a minimum of 1.5 minutes between requests is needed for a reliable, stable operation.
