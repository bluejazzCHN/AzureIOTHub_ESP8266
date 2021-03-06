
#include <AzureIoTHub.h>
#include <stdio.h>
#include <stdlib.h>

#include "DHTesp.h"

#define IOT_CONFIG_WIFI_SSID            "ZSJ_HOME"
#define IOT_CONFIG_WIFI_PASSWORD        "QQqq11!!"
#define DEVICE_CONNECTION_STRING    "HostName=zsjmcuiothub.azure-devices.net;DeviceId=esp8266;SharedAccessKey=MjbHk90QfmD3X8IizuaTvpqTSC63JXV//CdKUm531rE="

#define SAMPLE_MQTT

#ifndef SAMPLE_INIT_H
#define SAMPLE_INIT_H
#if defined(ARDUINO_ARCH_ESP8266)
#define sample_init esp8266_sample_init
#define is_esp_board
void esp8266_sample_init(const char* ssid, const char* password);
#endif // ARDUINO_ARCH_ESP8266
#endif // SAMPLE_INIT_H

DHTesp dht;

#ifdef is_esp_board
#include "Esp.h"
#endif

#ifdef SAMPLE_MQTT
#include "AzureIoTProtocol_MQTT.h"
#include "iothubtransportmqtt.h"
#endif // SAMPLE_MQTT

static const char ssid[] = IOT_CONFIG_WIFI_SSID;
static const char pass[] = IOT_CONFIG_WIFI_PASSWORD;

/*Define the struct of the message that will be sent to IOTHUB*/
char  t_name[] = "Temp";
char  h_name[] = "Humi";
char TH_result[30];
float humidity = 0;
float temperature = 0;

/*Get temp and humi data from sensor into data structure*/
static void getTHData()
{
  humidity = dht.getHumidity() - random(10);
  temperature = dht.getTemperature() - random(10);
  Serial.print("T:");
  Serial.println( temperature);
  Serial.print("H:");
  Serial.println( humidity);
}

/*Build the message  using previous data struct  in JSON style*/
static void buildIOTMessage()
{
  sprintf(TH_result, "{\"%s\":%2.2f,\"%s\":%2.2f}", t_name, temperature, h_name, humidity);
  Serial.println("The message will be sent:");
  Serial.println(TH_result);
}

/* Define several constants/global variables */
static const char* connectionString = DEVICE_CONNECTION_STRING;
static bool g_continueRunning = true; // defines whether or not the device maintains its IoT Hub connection after sending (think receiving messages from the cloud)
static size_t g_message_count_send_confirmations = 0;
static bool g_run = true;

IOTHUB_MESSAGE_HANDLE message_handle;
size_t messages_sent = 0;

/*Status that control ESP8266: start, stop,exit.*/
const char* quit_msg = "quit";
const char* exit_msg = "exit";
const char* stop_msg = "stop";
const char* start_msg = "start";

IOTHUB_DEVICE_CLIENT_LL_HANDLE device_ll_handle;

static int callbackCounter;
int receiveContext = 0;

/* -- receive_message_callback --
   Callback method which executes upon receipt of a message originating from the IoT Hub in the cloud.
   Note: Modifying the contents of this method allows one to command the device from the cloud.
*/
static IOTHUBMESSAGE_DISPOSITION_RESULT receive_message_callback(IOTHUB_MESSAGE_HANDLE message, void* userContextCallback)
{
  int* counter = (int*)userContextCallback;
  const unsigned char* buffer;
  size_t size;
  const char* messageId;

  // Message properties
  if ((messageId = IoTHubMessage_GetMessageId(message)) == NULL)
  {
    messageId = "<null>";
  }
  // Message content
  if (IoTHubMessage_GetByteArray(message, (const unsigned char**)&buffer, &size) != IOTHUB_MESSAGE_OK)
  {
    LogInfo("unable to retrieve the message data\r\n");
  }
  else
  {
    LogInfo("Received Message [%d]\r\n Message ID: %s\r\n Data: <<<%.*s>>> & Size=%d\r\n", *counter, messageId, (int)size, buffer, (int)size);

    //  receive the word 'stop' then we stop running
    if (size == (strlen(stop_msg) * sizeof(char)) && memcmp(buffer, stop_msg, size) == 0)
    {
      g_run = false;
      Serial.printf("Get c-command:%s\n", stop_msg);
    }
    //  receive the word 'stop' then we stop running
    if (size == (strlen(start_msg) * sizeof(char)) && memcmp(buffer, start_msg, size) == 0)
    {
      g_run = true;
      Serial.printf("Get c-command:%s\n", start_msg);
    }
  }
  return IOTHUBMESSAGE_ACCEPTED;
}

/* -- send_confirm_callback --
   Callback method which executes upon confirmation that a message originating from this device has been received by the IoT Hub in the cloud.
*/
static void send_confirm_callback(IOTHUB_CLIENT_CONFIRMATION_RESULT result, void* userContextCallback)
{
  (void)userContextCallback;
  // When a message is sent this callback will get envoked
  g_message_count_send_confirmations++;
  LogInfo("Confirmation callback received for message %lu with result %s\r\n", (unsigned long)g_message_count_send_confirmations, MU_ENUM_TO_STRING(IOTHUB_CLIENT_CONFIRMATION_RESULT, result));
}

/* -- connection_status_callback --
   Callback method which executes on receipt of a connection status message from the IoT Hub in the cloud.
*/
static void connection_status_callback(IOTHUB_CLIENT_CONNECTION_STATUS result, IOTHUB_CLIENT_CONNECTION_STATUS_REASON reason, void* user_context)
{
  (void)reason;
  (void)user_context;
  // This sample DOES NOT take into consideration network outages.
  if (result == IOTHUB_CLIENT_CONNECTION_AUTHENTICATED)
  {
    LogInfo("The device client is connected to iothub\r\n");
  }
  else
  {
    LogInfo("The device client has been disconnected\r\n");
  }
}

/* -- reset_esp_helper --
   waits for call of exit_msg over Serial line to reset device
*/
static void reset_esp_helper()
{
#ifdef is_esp_board
  // Read from local serial
  if (Serial.available()) {
    String ebit = Serial.readStringUntil('\n');// s1 is String type variable.
    Serial.print("Received Data:");
    Serial.println(ebit);
    if (ebit == exit_msg)
    {
      ESP.restart();
    }
    if (ebit == start_msg)
    {
      g_run = true;
    }
    if (ebit == stop_msg)
    {
      g_run = false;
    }
  }
#endif // is_esp_board
}

/* -- run_demo --
   Runs active task of sending telemetry to IoTHub
*/
static void sendMessageToIOTHub(char * input)
{
  int result = 0;
  // Construct the iothub message from a string or a byte array
  message_handle = IoTHubMessage_CreateFromString(input);
  //message_handle = IoTHubMessage_CreateFromByteArray((const unsigned char*)msgText, strlen(msgText)));
  // Set Message property
  /*(void)IoTHubMessage_SetMessageId(message_handle, "MSG_ID");
    (void)IoTHubMessage_SetCorrelationId(message_handle, "CORE_ID");
    (void)IoTHubMessage_SetContentTypeSystemProperty(message_handle, "application%2fjson");
    (void)IoTHubMessage_SetContentEncodingSystemProperty(message_handle, "utf-8");*/
  // Add custom properties to message
  // (void)IoTHubMessage_SetProperty(message_handle, "property_key", "property_value");

  LogInfo("Sending message %d to IoTHub\r\n", (int)(messages_sent + 1));
  result = IoTHubDeviceClient_LL_SendEventAsync(device_ll_handle, message_handle, send_confirm_callback, NULL);
  // The message is copied to the sdk so the we can destroy it
  IoTHubMessage_Destroy(message_handle);
  messages_sent++;
}
static void doWork()
{
  IoTHubDeviceClient_LL_DoWork(device_ll_handle);
  ThreadAPI_Sleep(3);
}
/*IOTHub client destroy*/
static void clearIOTHandler()
{
  IoTHubDeviceClient_LL_Destroy(device_ll_handle);
  IoTHub_Deinit();
  LogInfo("done with sending");
  return;
}

static void IOTHubInitial()
{
  // Select the Protocol to use with the connection
#ifdef SAMPLE_MQTT
  IOTHUB_CLIENT_TRANSPORT_PROVIDER protocol = MQTT_Protocol;
#endif // SAMPLE_MQTT
#ifdef SAMPLE_HTTP
  IOTHUB_CLIENT_TRANSPORT_PROVIDER protocol = HTTP_Protocol;
#endif // SAMPLE_HTTP
  // Used to initialize IoTHub SDK subsystem
  (void)IoTHub_Init();
  // Create the iothub handle here
  device_ll_handle = IoTHubDeviceClient_LL_CreateFromConnectionString(connectionString, protocol);
  LogInfo("Creating IoTHub Device handle\r\n");

  if (device_ll_handle == NULL)
  {
    LogInfo("Error AZ002: Failure creating Iothub device. Hint: Check you connection string.\r\n");
  }
  else
  {
    // Set any option that are neccessary.
    // For available options please see the iothub_sdk_options.md documentation in the main C SDK
    // turn off diagnostic sampling
    int diag_off = 0;
    IoTHubDeviceClient_LL_SetOption(device_ll_handle, OPTION_DIAGNOSTIC_SAMPLING_PERCENTAGE, &diag_off);

#ifndef SAMPLE_HTTP
    // Example sdk status tracing for troubleshooting
    bool traceOn = true;
    IoTHubDeviceClient_LL_SetOption(device_ll_handle, OPTION_LOG_TRACE, &traceOn);
#endif
    // Setting the Trusted Certificate.
    IoTHubDeviceClient_LL_SetOption(device_ll_handle, OPTION_TRUSTED_CERT, certificates);

#if defined SAMPLE_MQTT
    //Setting the auto URL Encoder (recommended for MQTT). Please use this option unless
    //you are URL Encoding inputs yourself.
    //ONLY valid for use with MQTT
    bool urlEncodeOn = true;
    IoTHubDeviceClient_LL_SetOption(device_ll_handle, OPTION_AUTO_URL_ENCODE_DECODE, &urlEncodeOn);
    /* Setting Message call back, so we can receive Commands. */
    if (IoTHubClient_LL_SetMessageCallback(device_ll_handle, receive_message_callback, &receiveContext) != IOTHUB_CLIENT_OK)
    {
      LogInfo("ERROR: IoTHubClient_LL_SetMessageCallback..........FAILED!\r\n");
    }
#endif // SAMPLE_MQTT

    // Setting connection status callback to get indication of connection to iothub
    (void)IoTHubDeviceClient_LL_SetConnectionStatusCallback(device_ll_handle, connection_status_callback, NULL);
  }
}

void setup() {
  Serial.begin(115200);
//  init wifi
  sample_init(ssid, pass);
// init dht11
  dht.setup(14, DHTesp::DHT11);
//  init iothub
  IOTHubInitial();
}

void loop(void)
{
  delay(5000);
  if (g_run )
  {
    getTHData();
    buildIOTMessage();
    sendMessageToIOTHub(TH_result);
  }
  else
  {
  }
  doWork();
  reset_esp_helper();
}
