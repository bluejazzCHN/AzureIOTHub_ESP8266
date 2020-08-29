# AzureIOTHub_ESP8266
ESP8266 gets local sensor data to send to auze iothub.
# Requirement 
To run demo, should meet below 

+ ESP8266 devkit_C
+ DHT11
+ Azure Subscription
   + IOTHub
   + StreamAnalysis
   + PowerBI
+ arduino IDE

# Description
Get DHT11 temp and humi data, through azure iothub client sdk send to azure.

Demo include:
- how to set wifi
- how to init azure iothub client
- how to send message and wait callback message

# Attention
Navigate to where your esp8266 board package is located, typically in C:\Users\<your username>\AppData\Local\Arduino15\packages on Windows and ~/.arduino15/packages/ on Linux

Locate the board's Arduino.h (hardware/esp8266/<board package version>/cores/esp8266/ and comment out the line containing #define round(x), around line 137.

Two folders up from the Arduino.h step above, in the same folder as the board's platform.txt, paste the platform.local.txt file from the esp8266 folder in the sample into it.

Note1: It is necessary to add -DDONT_USE_UPLOADTOBLOB and -DUSE_BALTIMORE_CERT to build.extra_flags= in a platform.txt in order to run the sample, however, you can define them in your own platform.txt or a platform.local.txt of your own creation.

Note2: If your device is not intended to connect to the global portal.azure.com, please change the CERT define to the appropriate cert define as laid out in certs.c

Note3: Due to RAM limits, you must select just one CERT define.
