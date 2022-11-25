
/**
   Created by K. Suwatchai (Mobizt)

   Email: k_suwatchai@hotmail.com

   Github: https://github.com/mobizt/Firebase-ESP-Client

   Copyright (c) 2022 mobizt

*/

#if defined(ESP32)
#include <WiFi.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#endif
#include <DFRobot_DHT11.h>
DFRobot_DHT11 DHT;
#define DHT11_PIN 25
#include <Firebase_ESP_Client.h>

// Provide the token generation process info.
#include <addons/TokenHelper.h>

// Provide the RTDB payload printing info and other helper functions.
#include <addons/RTDBHelper.h>

/* 1. Define the WiFi credentials */
#define WIFI_SSID "ANTT SOFT_ROOM"
#define WIFI_PASSWORD "AnttRoboticsLtd123"

// For the following credentials, see examples/Authentications/SignInAsUser/EmailPassword/EmailPassword.ino

/* 2. Define the API Key */
#define API_KEY "AIzaSyDXwRiKw4t12HSsp4PRIYvr7gJ3RzXxWkw"

/* 3. Define the RTDB URL */
#define DATABASE_URL "https://leafy-database-control-system-default-rtdb.asia-southeast1.firebasedatabase.app/" //<databaseName>.firebaseio.com or <databaseName>.<region>.firebasedatabase.app

/* 4. Define the user Email and password that alreadey registerd or added in your project */
#define USER_EMAIL "admin@gmail.com"
#define USER_PASSWORD "admin12345"

float calibration_value = 20.24 - 0.7; //21.34 - 0.7
int phval = 0;
unsigned long int avgval;
int buffer_arr[10], temp;
int phPin = 35;
float ph;
#define TdsSensorPin 34
#define VREF 3.3              // analog reference voltage(Volt) of the ADC
#define SCOUNT  30            // sum of sample point

int analogBuffer[SCOUNT];     // store the analog value in the array, read from ADC
int analogBufferTemp[SCOUNT];
int analogBufferIndex = 0;
int copyIndex = 0;

float averageVoltage = 0;
float tds = 0;
float temperature = 0;
float humidity = 0;

int relay1 = 26;
int relay2 = 27;
int relay3 = 14;
int relay4 = 12;
int LED = 13;
int AC_LOAD = 15;

// Define Firebase Data object
FirebaseData fbdo;

FirebaseAuth auth;
FirebaseConfig config;

unsigned long sendDataPrevMillis = 0;

unsigned long count = 0;

void phValue() {
  for (int i = 0; i < 10; i++)
  {
    buffer_arr[i] = analogRead(phPin);
    delay(30);
  }
  for (int i = 0; i < 9; i++)
  {
    for (int j = i + 1; j < 10; j++)
    {
      if (buffer_arr[i] > buffer_arr[j])
      {
        temp = buffer_arr[i];
        buffer_arr[i] = buffer_arr[j];
        buffer_arr[j] = temp;
      }
    }
  }
  avgval = 0;
  for (int i = 2; i < 8; i++)
    avgval += buffer_arr[i];
  float volt = (float)avgval * 3.3 / 4096.0 / 6;
  //Serial.print("Voltage: ");
  //Serial.println(volt);
  ph = -5.70 * volt + calibration_value;

  return ph;
}

// median filtering algorithm
int getMedianNum(int bArray[], int iFilterLen) {
  int bTab[iFilterLen];
  for (byte i = 0; i < iFilterLen; i++)
    bTab[i] = bArray[i];
  int i, j, bTemp;
  for (j = 0; j < iFilterLen - 1; j++) {
    for (i = 0; i < iFilterLen - j - 1; i++) {
      if (bTab[i] > bTab[i + 1]) {
        bTemp = bTab[i];
        bTab[i] = bTab[i + 1];
        bTab[i + 1] = bTemp;
      }
    }
  }
  if ((iFilterLen & 1) > 0) {
    bTemp = bTab[(iFilterLen - 1) / 2];
  }
  else {
    bTemp = (bTab[iFilterLen / 2] + bTab[iFilterLen / 2 - 1]) / 2;
  }
  return bTemp;
}

void tdsValue() {
  static unsigned long analogSampleTimepoint = millis();
  if (millis() - analogSampleTimepoint > 40U) { //every 40 milliseconds,read the analog value from the ADC
    analogSampleTimepoint = millis();
    analogBuffer[analogBufferIndex] = analogRead(TdsSensorPin);    //read the analog value and store into the buffer
    analogBufferIndex++;
    if (analogBufferIndex == SCOUNT) {
      analogBufferIndex = 0;
    }
  }

  static unsigned long printTimepoint = millis();
  if (millis() - printTimepoint > 800U) {
    printTimepoint = millis();
    for (copyIndex = 0; copyIndex < SCOUNT; copyIndex++) {
      analogBufferTemp[copyIndex] = analogBuffer[copyIndex];

      // read the analog value more stable by the median filtering algorithm, and convert to voltage value
      averageVoltage = getMedianNum(analogBufferTemp, SCOUNT) * (float)VREF / 4096.0;

      //temperature compensation formula: fFinalResult(25^C) = fFinalResult(current)/(1.0+0.02*(fTP-25.0));
      float compensationCoefficient = 1.0 + 0.02 * (temperature - 25.0);
      //temperature compensation
      float compensationVoltage = averageVoltage / compensationCoefficient;

      //convert voltage value to tds value
      tds = (133.42 * compensationVoltage * compensationVoltage * compensationVoltage - 255.86 * compensationVoltage * compensationVoltage + 857.39 * compensationVoltage) * 0.5;
      return tds;
    }

    void setup()
    {

      Serial.begin(115200);
      pinMode(TdsSensorPin, INPUT);
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
      Serial.print("Connecting to Wi-Fi");
      while (WiFi.status() != WL_CONNECTED)
      {
        Serial.print(".");
        delay(300);
      }
      Serial.println();
      Serial.print("Connected with IP: ");
      Serial.println(WiFi.localIP());
      Serial.println();

      Serial.printf("Firebase Client v%s\n\n", FIREBASE_CLIENT_VERSION);

      /* Assign the api key (required) */
      config.api_key = API_KEY;

      /* Assign the user sign in credentials */
      auth.user.email = USER_EMAIL;
      auth.user.password = USER_PASSWORD;

      /* Assign the RTDB URL (required) */
      config.database_url = DATABASE_URL;

      /* Assign the callback function for the long running token generation task */
      config.token_status_callback = tokenStatusCallback; // see addons/TokenHelper.h

      // Or use legacy authenticate method
      // config.database_url = DATABASE_URL;
      // config.signer.tokens.legacy_token = "<database secret>";

      // To connect without auth in Test Mode, see Authentications/TestMode/TestMode.ino

      //////////////////////////////////////////////////////////////////////////////////////////////
      // Please make sure the device free Heap is not lower than 80 k for ESP32 and 10 k for ESP8266,
      // otherwise the SSL connection will fail.
      //////////////////////////////////////////////////////////////////////////////////////////////

#if defined(ESP8266)
      // In ESP8266 required for BearSSL rx/tx buffer for large data handle, increase Rx size as needed.
      fbdo.setBSSLBufferSize(2048 /* Rx buffer size in bytes from 512 - 16384 */, 2048 /* Tx buffer size in bytes from 512 - 16384 */);
#endif

      // Limit the size of response payload to be collected in FirebaseData
      fbdo.setResponseSize(2048);

      Firebase.begin(&config, &auth);

      // Comment or pass false value when WiFi reconnection will control by your code or third party library
      Firebase.reconnectWiFi(true);

      Firebase.setDoubleDigits(5);

      config.timeout.serverResponse = 10 * 1000;

      /** Timeout options.

        //WiFi reconnect timeout (interval) in ms (10 sec - 5 min) when WiFi disconnected.
        config.timeout.wifiReconnect = 10 * 1000;

        //Socket connection and SSL handshake timeout in ms (1 sec - 1 min).
        config.timeout.socketConnection = 10 * 1000;

        //Server response read timeout in ms (1 sec - 1 min).
        config.timeout.serverResponse = 10 * 1000;

        //RTDB Stream keep-alive timeout in ms (20 sec - 2 min) when no server's keep-alive event data received.
        config.timeout.rtdbKeepAlive = 45 * 1000;

        //RTDB Stream reconnect timeout (interval) in ms (1 sec - 1 min) when RTDB Stream closed and want to resume.
        config.timeout.rtdbStreamReconnect = 1 * 1000;

        //RTDB Stream error notification timeout (interval) in ms (3 sec - 30 sec). It determines how often the readStream
        //will return false (error) when it called repeatedly in loop.
        config.timeout.rtdbStreamError = 3 * 1000;

        Note:
        The function that starting the new TCP session i.e. first time server connection or previous session was closed, the function won't exit until the
        time of config.timeout.socketConnection.

        You can also set the TCP data sending retry with
        config.tcp_data_sending_retry = 1;

      */
    }

    void loop()
    {



      // Firebase.ready() should be called repeatedly to handle authentication tasks.

      if (Firebase.ready() && (millis() - sendDataPrevMillis > 15000 || sendDataPrevMillis == 0))
      {
        sendDataPrevMillis = millis();
        phValue();
        tdsValue();
        DHT.read(DHT11_PIN);
        temperature = DHT.temperature;
        humidity = DHT.humidity;



        bool bVal;
        Serial.printf("Get bool ref... %s\n", Firebase.RTDB.getBool(&fbdo, F("/command/LED"), &bVal) ? bVal ? "true" : "false" : fbdo.errorReason().c_str());

        Serial.printf("Set int... %s\n", Firebase.RTDB.setInt(&fbdo, F("/command/LED"), random(0, 1)) ? "ok" : fbdo.errorReason().c_str());


        int iVal = 0;
        Serial.printf("Get int ref... %s\n", Firebase.RTDB.getInt(&fbdo, F("/data/Humidity"), &iVal) ? String(iVal).c_str() : fbdo.errorReason().c_str());

        Serial.printf("Set float... %s\n", Firebase.RTDB.setFloat(&fbdo, F("/data/Humidity"), count + 10.2) ? "ok" : fbdo.errorReason().c_str());
        Serial.printf("Set double... %s\n", Firebase.RTDB.setFloat(&fbdo, F("/data/Temperature"), count + 35.517549723765) ? "ok" : fbdo.errorReason().c_str());



        Serial.println();

        // For generic set/get functions.

        // For generic set, use Firebase.RTDB.set(&fbdo, <path>, <any variable or value>)

        // For generic get, use Firebase.RTDB.get(&fbdo, <path>).
        // And check its type with fbdo.dataType() or fbdo.dataTypeEnum() and
        // cast the value from it e.g. fbdo.to<int>(), fbdo.to<std::string>().

        // The function, fbdo.dataType() returns types String e.g. string, boolean,
        // int, float, double, json, array, blob, file and null.

        // The function, fbdo.dataTypeEnum() returns type enum (number) e.g. fb_esp_rtdb_data_type_null (1),
        // fb_esp_rtdb_data_type_integer, fb_esp_rtdb_data_type_float, fb_esp_rtdb_data_type_double,
        // fb_esp_rtdb_data_type_boolean, fb_esp_rtdb_data_type_string, fb_esp_rtdb_data_type_json,
        // fb_esp_rtdb_data_type_array, fb_esp_rtdb_data_type_blob, and fb_esp_rtdb_data_type_file (10)

        count++;
      }
    }

    /** NOTE:
       When you trying to get boolean, integer and floating point number using getXXX from string, json
       and array that stored on the database, the value will not set (unchanged) in the
       FirebaseData object because of the request and data response type are mismatched.

       There is no error reported in this case, until you set this option to true
       config.rtdb.data_type_stricted = true;

       In the case of unknown type of data to be retrieved, please use generic get function and cast its value to desired type like this

       Firebase.RTDB.get(&fbdo, "/path/to/node");

       float value = fbdo.to<float>();
       String str = fbdo.to<String>();

    */

    /// PLEASE AVOID THIS ////

    // Please avoid the following inappropriate and inefficient use cases
    /**

       1. Call get repeatedly inside the loop without the appropriate timing for execution provided e.g. millis() or conditional checking,
       where delay should be avoided.

       Everytime get was called, the request header need to be sent to server which its size depends on the authentication method used,
       and costs your data usage.

       Please use stream function instead for this use case.

       2. Using the single FirebaseData object to call different type functions as above example without the appropriate
       timing for execution provided in the loop i.e., repeatedly switching call between get and set functions.

       In addition to costs the data usage, the delay will be involved as the session needs to be closed and opened too often
       due to the HTTP method (GET, PUT, POST, PATCH and DELETE) was changed in the incoming request.


       Please reduce the use of swithing calls by store the multiple values to the JSON object and store it once on the database.

       Or calling continuously "set" or "setAsync" functions without "get" called in between, and calling get continuously without set
       called in between.

       If you needed to call arbitrary "get" and "set" based on condition or event, use another FirebaseData object to avoid the session
       closing and reopening.

       3. Use of delay or hidden delay or blocking operation to wait for hardware ready in the third party sensor libraries, together with stream functions e.g. Firebase.RTDB.readStream and fbdo.streamAvailable in the loop.

       Please use non-blocking mode of sensor libraries (if available) or use millis instead of delay in your code.

       4. Blocking the token generation process.

       Let the authentication token generation to run without blocking, the following code MUST BE AVOIDED.

       while (!Firebase.ready()) <---- Don't do this in while loop
       {
           delay(1000);
       }

    */
