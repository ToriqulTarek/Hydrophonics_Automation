

#include <WiFi.h>

#include "RunningAverage.h"
#include <DFRobot_DHT11.h>
DFRobot_DHT11 DHT;
#define DHT11_PIN 17


RunningAverage myRA(10);
#include <Firebase_ESP_Client.h>

// Provide the token generation process info.
#include <addons/TokenHelper.h>

// Provide the RTDB payload printing info and other helper functions.
#include <addons/RTDBHelper.h>

/* 1. Define the WiFi credentials */
#define WIFI_SSID "SSID"
#define WIFI_PASSWORD "PASSWORD"

// For the following credentials, see examples/Authentications/SignInAsUser/EmailPassword/EmailPassword.ino

/* 2. Define the API Key */
#define API_KEY "API"

/* 3. Define the RTDB URL */
#define DATABASE_URL "Priject URL" //<databaseName>.firebaseio.com or <databaseName>.<region>.firebasedatabase.app

/* 4. Define the user Email and password that alreadey registerd or added in your project */
#define USER_EMAIL "User Email"
#define USER_PASSWORD "User Password"

float calibration_value = 21.34; //21.34 - 0.7
int phval = 0;
unsigned long int avgval;
int buffer_arr[10], temp;
int phPin = 35;
float ph;
int samples = 0;
float tds = 0;
#define TdsSensorPin 25
#define VREF 3.3              // analog reference voltage(Volt) of the ADC
#define SCOUNT  30            // sum of sample point
float tempTDS = 25;
int analogBuffer[SCOUNT];     // store the analog value in the array, read from ADC
int analogBufferTemp[SCOUNT];
int analogBufferIndex = 0;
int copyIndex = 0;

float averageVoltage = 0;
float temperature = 0;
float humidity = 0;

int phUp = 26;
int TDS25 = 27;
int TDS75 = 14;
int phDown = 12;
int LED = 13;
int AC_LOAD = 15;
bool autoCommand;
bool acLoad;
bool command;

// Define Firebase Data object
FirebaseData fbdo;

FirebaseAuth auth;
FirebaseConfig config;

unsigned long sendDataPrevMillis = 0;

unsigned long count = 0;

void phValue() {
for (int i = 0; i < 10; i++)
  {
    buffer1_arr[i] = analogRead(phPin);
    delay(30);
  }
  for (int i = 0; i < 9; i++)
  {
    for (int j = i + 1; j < 10; j++)
    {
      if (buffer1_arr[i] > buffer1_arr[j])
      {
        temp1 = buffer1_arr[i];
        buffer1_arr[i] = buffer1_arr[j];
        buffer1_arr[j] = temp1;
      }
    }
  }
  avgval1 = 0;
  for (int i = 2; i < 8; i++)
    avgval1 += buffer1_arr[i];
  float volt1 = (float)avgval1 * 3.3 / 4096.0 / 6;
  //Serial.print("Voltage: ");
  //Serial.println(volt);
  ph = -5.70 * volt1 + calibration_value1;
  //long rn = random(0, 1000);
  myRA.addValue(ph);
  samples++;
  //Serial.print("Running Average: ");
  float avgpH = ( myRA.getAverage(), 2);

  if (samples == 30)
  {
    samples = 0;
    myRA.clear();
  }
  delay(100);

  //return ph;
  Serial.print("Ph Value :");
  Serial.println(ph);
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
      float compensationCoefficient = 1.0 + 0.02 * (tempTDS - 25.0);
      //temperature compensation
      float compensationVoltage = averageVoltage / compensationCoefficient;

      //convert voltage value to tds value
      tds = (133.42 * compensationVoltage * compensationVoltage * compensationVoltage - 255.86 * compensationVoltage * compensationVoltage + 857.39 * compensationVoltage) * 0.5;

    }
  }
  Serial.print("TDS :");
  Serial.print(tds);
  Serial.println("ppm");
  //return tds;
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

void setup()
{

  Serial.begin(115200);
  pinMode(phPin, INPUT);
  pinMode(TdsSensorPin, INPUT);
  pinMode(phUp, OUTPUT);
  pinMode(TDS25, OUTPUT);
  pinMode(TDS75, OUTPUT);
  pinMode(phDown, OUTPUT);
  pinMode(LED, OUTPUT);
  pinMode(AC_LOAD, OUTPUT);
  digitalWrite(phUp, HIGH);
  digitalWrite(TDS25, HIGH);
  digitalWrite(TDS75, HIGH);
  digitalWrite(phDown, HIGH);
  digitalWrite(LED, HIGH);
  digitalWrite(AC_LOAD, HIGH);
  myRA.clear(); // explicitly start clean
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
    Serial.print("Temperature: ");
    Serial.print(temperature);
    Serial.println("°C");
    Serial.print("Humidity: ");
    Serial.print(humidity);
    Serial.println("%");

    Serial.printf("Set humidity... %s\n", Firebase.RTDB.setFloat(&fbdo, F("/data/Humidity"), humidity) ? "ok" : fbdo.errorReason().c_str());
    Serial.printf("Set temperature... %s\n", Firebase.RTDB.setFloat(&fbdo, F("/data/Temperature"), temperature) ? "ok" : fbdo.errorReason().c_str());
    Serial.printf("Set ph... %s\n", Firebase.RTDB.setFloat(&fbdo, F("/data/pH"), ph) ? "ok" : fbdo.errorReason().c_str());
    Serial.printf("Set tds... %s\n", Firebase.RTDB.setFloat(&fbdo, F("/data/tds"), tds) ? "ok" : fbdo.errorReason().c_str());

    Serial.println();

    String autoC = String(Firebase.RTDB.getBool(&fbdo, F("/command/autoMode"), &autoCommand) ? autoCommand ? "true" : "false" : fbdo.errorReason().c_str());

    while (autoC == "true") {
      phValue();
      tdsValue();
      if (ph >= 6.6) {

        digitalWrite(phDown, LOW);

      }
      else if (ph <= 6.5) {
        
        digitalWrite(phDown, HIGH);
      }
      if (tds < 800) {

        digitalWrite(TDS25, LOW);
        digitalWrite(TDS75, LOW);
        delay(900);
        digitalWrite(TDS25, HIGH);
        delay(2900);
        digitalWrite(TDS75, HIGH);
      }
      else if (tds > 1200) {
        digitalWrite(TDS75, HIGH);
        digitalWrite(TDS25, HIGH);
      }
      String autoC = String(Firebase.RTDB.getBool(&fbdo, F("/command/autoMode"), &autoCommand) ? autoCommand ? "true" : "false" : fbdo.errorReason().c_str());
      phValue();
      tdsValue();
      if (autoC == "false") {

        digitalWrite(phUp, HIGH);
        digitalWrite(TDS25, HIGH);
        digitalWrite(TDS75, HIGH);
        digitalWrite(phDown, HIGH);;

        break;
      }
    }




    String Led = String (Firebase.RTDB.getBool(&fbdo, F("/command/LED"), &command) ? command ? "true" : "false" : fbdo.errorReason().c_str());

    if (Led == "true") {
      digitalWrite(LED, LOW);
    }
    else if (Led == "false") {
      digitalWrite(LED, HIGH);
    }


    String acL = String(Firebase.RTDB.getBool(&fbdo, F("/command/acLoad"), &acLoad) ? acLoad ? "true" : "false" : fbdo.errorReason().c_str());

    if (acL == "true") {

      digitalWrite(AC_LOAD, LOW);
    }
    else if (acL == "false") {
      digitalWrite(AC_LOAD, HIGH);
    }






  }
}
