
/* Declaracion de librerias */
#include <Wire.h>
#include <HardwareSerial.h>
#include <RTClib.h>
#include <low_power.h>
#include <STM32LowPower.h>
#include <TinyGPSPlus.h> // Librería TinyGPS++

volatile bool alarmFired = false;
RTC_DS3231 rtc;
TinyGPSPlus gps1; // Objeto GPS

/* Declaracion de puertos del STM32F103C8T6 */
// const int SQW_PIN = PA0;
const int SQW_PIN = PB0;
const int STM_LED = PC13;
const int LED = PA6;
const int RED_LED = PA7;
const int YELLOW_LED = PB1;
String latitude, longitude;
//const int PUSH_BTN = PB0;

/* Constantes y Variables Globales */
const String ID = "48273619";
const String token = "7621148456:AAEp3MgQVO5qfpaf2T83QvnYm9QFSYs9yKI";
const String chat_id = "94303788";
int _timeout;
String _buffer;

//const String number = "+525620577634"; //Oxxo Cel
const String number = "+525554743913"; //Telcel

unsigned long chars;
unsigned short sentences, failed_checksum;

// Definir el puerto serial SIM800L
HardwareSerial SIM800L(PA3, PA2);
HardwareSerial NEO8M(PA10, PA9);

void setAlarmFired() {
  alarmFired = true;
}

void configureAlarm(){
  rtc.clearAlarm(1);
  rtc.clearAlarm(2);
  // turn off alarm 2 (in case it isn't off already)
  // again, this isn't done at reboot, so a previously set alarm could easily go overlooked
  rtc.disableAlarm(1);
  rtc.disableAlarm(2);
  
  // stop oscillating signals at SQW Pin9600
  // otherwise setAlarm1 will fail
  //rtc.writeSqwPinMode(DS3231_OFF);

  //Set Alarm to be trigged in X 
  rtc.setAlarm1(rtc.now() + TimeSpan(0, 0, 1, 0), DS3231_A1_Minute);  // this mode triggers the alarm when the seconds match.

  alarmFired = false;
}

void setup() {
  // put your setup code here, to run once:
  Wire.begin();
  _buffer.reserve(50);
  SIM800L.begin(115200);
  NEO8M.begin(9600);

  /* COnfiguracion de puertos */
  pinMode(SQW_PIN, INPUT_PULLUP);
  pinMode(STM_LED, OUTPUT);
  pinMode(LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);

  digitalWrite(STM_LED, LOW);
  digitalWrite(LED, HIGH);

  if (! rtc.begin()) {        // si falla la inicializacion del modulo
    //Serial.println("Modulo RTC no encontrado !");  // muestra mensaje de error
    while (1);         // bucle infinito que detiene ejecucion del programa
  }

  if(rtc.lostPower()) {
        // this will adjust to the date and time at compilation
        rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
   
  rtc.adjust(DateTime(__DATE__, __TIME__));  // funcion que permite establecer fecha y horario
            // al momento de la compilacion. Comentar esta linea
            // y volver a subir para normal operacion

  //rtc.adjust(DateTime(2025, 1, 31, 16, 30, 0));  // año, mes, día, hora, minuto, segundo
  rtc.disable32K();
  
  // stop oscillating signals at SQW Pin
  // otherwise setAlarm1 will fail
  rtc.writeSqwPinMode(DS3231_OFF);

  //Set Alarm to be trigged in X 
  //rtc.setAlarm1( rtc.now() + TimeSpan(50), DS3231_A1_Second); // this mode triggers the alarm when the seconds match.
  configureAlarm();

  //Create Trigger
  //attachInterrupt(digitalPinToInterrupt(SQW_PIN), setAlarmFired, FALLING);
  
  delay(15000);
  enviarMensaje("Rastreador encendido");
  //enviarMensajeTelegram("Rastreador encendido " + ID);
  digitalWrite(STM_LED,HIGH);
  digitalWrite(LED,LOW);

  // Configure low power
  LowPower.begin();
  // Attach a wakeup interrupt on pin, calling repetitionsIncrease when the device is woken up
  // Last parameter (LowPowerMode) should match with the low power state used
  //LowPower.attachInterruptWakeup(digitalPinToInterrupt(SQW_PIN), setAlarmFired, FALLING, SLEEP_MODE);
  LowPower.attachInterruptWakeup(digitalPinToInterrupt(SQW_PIN), setAlarmFired, FALLING, DEEP_SLEEP_MODE);
}

void loop() {

  if(alarmFired){

    String datosGPS = leerYGuardarGPS();

    SendMessage(datosGPS);

    configureAlarm();
    //LowPower.sleep();
    LowPower.deepSleep();
  }
  
}

String readSIM800LResponse(unsigned long timeout = 2000) {
  String response = "";
  unsigned long startTime = millis();

  while (millis() - startTime < timeout) {
    while (SIM800L.available()) {
      char c = SIM800L.read();
      response += c;
    }
  }

  return response;
}

void enviarMensaje(String SMS)
{
  digitalWrite(RED_LED,HIGH);
  SIM800L.println("AT+CMGF=1");    //Sets the GSM Module in Text Mode
  delay(200);
  
  //Serial.println ("Set SMS Number");
  SIM800L.println("AT+CMGS=\"" + number + "\"\r"); //Mobile phone number to send message
  delay(200);

  SIM800L.println(SMS);
  delay(200);
  SIM800L.println((char)26);// ASCII code of CTRL+Z
  delay(200);
  _buffer = _readSerial();

  delay(2000);
  digitalWrite(RED_LED,LOW);
}

void enviarMensajeTelegram(String SMS) {
    digitalWrite(RED_LED, HIGH);

    // Activar conexión GPRS
    // SIM800L.println("AT+CGATT=1");
    // delay(1000);
    // SIM800L.println("AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\"");
    // delay(1000);
    // SIM800L.println("AT+SAPBR=1,1");
    // delay(2000);
    // SIM800L.println("AT+SAPBR=2,1");
    // delay(1000);
    SIM800L.println("AT+SAPBR=3,1,\"APN\",\"CMNET\"");
    delay(1000);
    SIM800L.println("AT+SAPBR=1,1");
    delay(2000);


    // Construir URL de la API de Telegram con texto codificado
    String api_url = "https://api.telegram.org/bot" + token + "/sendMessage?chat_id=" + chat_id + "&text=" + urlencode(SMS);

    // Inicializar HTTP
    SIM800L.println("AT+HTTPINIT");
    delay(200);
    SIM800L.println("AT+HTTPPARA=\"CID\",1");
    delay(200);
    SIM800L.println("AT+HTTPPARA=\"URL\",\"" + api_url + "\"");
    delay(200);
    SIM800L.println("AT+HTTPACTION=0");
    delay(5000); // Esperar la respuesta
    SIM800L.println("AT+HTTPREAD");
    String respuesta = _readSerial();
    Serial.println("Respuesta del servidor: " + respuesta);

    // Verificar si el mensaje fue enviado correctamente
    if (respuesta.indexOf("200") > 0) {
        Serial.println("✅ Mensaje enviado correctamente a Telegram");
    } else {
        Serial.println("⚠️ Error al enviar mensaje, revisa la URL o el estado de la conexión.");
    }

    // Finalizar conexión HTTP
    SIM800L.println("AT+HTTPTERM");
    delay(200);
    SIM800L.println("AT+SAPBR=0,1");
    delay(200);
    digitalWrite(RED_LED, LOW);
}

String urlencode(String str) {
    str.replace(" ", "%20");
    return str;
}

void SendMessage(String datosGPS)
{
  digitalWrite(STM_LED, LOW);
  digitalWrite(LED, HIGH);
  
  String cellTowerInfo = "";
  cellTowerInfo = getCellInfo();

  //Serial.println ("Sending Message");
  SIM800L.println("AT+CMGF=1");    //Sets the GSM Module in Text Mode
  delay(1500);
  
  //Serial.println ("Set SMS Number");
  SIM800L.println("AT+CMGS=\"" + number + "\"\r"); //Mobile phone number to send message
  delay(1200);

  String SMS = createMessageToSend(datosGPS, cellTowerInfo);
  SIM800L.println(SMS);
  delay(100);
  SIM800L.println((char)26);// ASCII code of CTRL+Z
  delay(200);
  _buffer = _readSerial();

  delay(2000);
  digitalWrite(STM_LED,HIGH);
  digitalWrite(LED,LOW);
}

String _readSerial() {
  _timeout = 0;
  while  (!SIM800L.available() && _timeout < 12000  )
  {
    delay(13);
    _timeout++;
  }
  if (SIM800L.available()) {
    return SIM800L.readString();
  }
}


String createMessageToSend(String datosGPS, String cellTowerInfo){

  //activeYellowLed(2);

  DateTime now = rtc.now();

  char buffer[20];
  sprintf(buffer, "%04d-%02d-%02dT%02d:%02d:%02d", 
          now.year(), now.month(), now.day(), 
          now.hour(), now.minute(), now.second());
  
  String currentTime = String(buffer);

  //activeYellowLed(3);

  String output = "{";
    output += "\"id\":\"" + ID + "\",";
    output += "\"time\":\"" + currentTime + "\",";
    // output += "\"cellTowerInfo\":" + cellTowerInfo + ",";
    output += cellTowerInfo + ",";
    output += datosGPS;
    output += "}";
  
  return output;
}

// String leerYGuardarGPS() {
//   activeRedLed(2);
//   enviarMensaje(" ---Buscando senal--- ");

//   String _latitude = "";
//   String _longitude = "";

//   digitalWrite(YELLOW_LED, HIGH);
//   unsigned long startTime = millis();

//   while ((millis() - startTime) < 5000) {
//     while (NEO8M.available()) {
//       char c = NEO8M.read();
//       gps1.encode(c);
//       if (gps1.location.isValid()) {
//         digitalWrite(RED_LED,HIGH);
//         _latitude = String(gps1.location.lat(), 6);
//         _longitude = String(gps1.location.lng(), 6);
//         enviarMensaje("Datos:" + _latitude + ". " + _longitude);
//         goto FIN;
//       }
//     }
//   }

// FIN:
//   delay(1000);
//   digitalWrite(YELLOW_LED, LOW);

//   if (_latitude == "" || _longitude == "") {
//     _latitude = "0.0";
//     _longitude = "0.0";
//   }

//   return "\"lat\":\"" + _latitude + "\",\"lon\":\"" + _longitude + "\"";
// }

// String leerYGuardarGPS() {
//   activeRedLed(2);
//   enviarMensaje(" ---Buscando señal--- ");

//   String nuevaLat = "";
//   String nuevaLon = "";
//   String anteriorLat = latitude;  // Guardamos la última ubicacion conocida
//   String anteriorLon = longitude;

//   digitalWrite(YELLOW_LED, HIGH);
//   unsigned long startTime = millis();

//   while ((millis() - startTime) < 10000) { // Ampliamos el tiempo de búsqueda a 10 segundos
//     while (NEO8M.available()) {
//       char c = NEO8M.read();
//       gps1.encode(c);

//       if (gps1.location.isValid()) {
//         nuevaLat = String(gps1.location.lat(), 6);
//         nuevaLon = String(gps1.location.lng(), 6);

//         if (nuevaLat != anteriorLat || nuevaLon != anteriorLon) { // Solo actualizar si ha cambiado
//           latitude = nuevaLat;
//           longitude = nuevaLon;
//           enviarMensaje("Ubicacion actualizada: " + nuevaLat + ", " + nuevaLon);
//           break;
//         }
//       }
//     }
//   }

//   digitalWrite(YELLOW_LED, LOW);

//   if (nuevaLat == "" || nuevaLon == "") {
//     nuevaLat = "0.0";
//     nuevaLon = "0.0";
//   }

//   return "\"lat\":\"" + nuevaLat + "\",\"lon\":\"" + nuevaLon + "\"";
// }

String leerYGuardarGPS() {
  //ctiveRedLed(2);
  //enviarMensaje(" ---Buscando señal--- ");

  String nuevaLat = "";
  String nuevaLon = "";
  String anteriorLat = latitude;  
  String anteriorLon = longitude;
  bool ubicacionActualizada = false;
  digitalWrite(YELLOW_LED, HIGH);
  unsigned long startTime = millis();
  int intentos = 0;

  while ((millis() - startTime) < 10000 && intentos < 30 && !ubicacionActualizada) { 
    while (NEO8M.available()) {
      char c = NEO8M.read();
      gps1.encode(c);

      if (gps1.location.isUpdated()) { 
        nuevaLat = String(gps1.location.lat(), 6);
        nuevaLon = String(gps1.location.lng(), 6);

        if (nuevaLat != anteriorLat || nuevaLon != anteriorLon) { 
          latitude = nuevaLat;
          longitude = nuevaLon;
          ubicacionActualizada = true;
          enviarMensaje("Ubicacion actualizada: " + nuevaLat + ", " + nuevaLon);
          //enviarMensajeTelegram("Ubicacion actualizada: " + nuevaLat + ", " + nuevaLon);
          break;
        }
      }
    }
    delay(50); 
    intentos++;
  }

  digitalWrite(YELLOW_LED, LOW);

  if(!ubicacionActualizada){
    nuevaLat = latitude;
    nuevaLon = longitude;
  }

  if (nuevaLat == "" || nuevaLon == "") {
    nuevaLat = "0.0";
    nuevaLon = "0.0";
  }

  return "\"lat\":\"" + nuevaLat + "\",\"lon\":\"" + nuevaLon + "\"";
}

String getCellInfo() {
  String lac = "";
  String cellId = "";
  //String operatorName = "";
  String mcc = "";
  String mnc = "";

  // Configurar el modo extendido para incluir LAC y Cell ID
  flushSIM800L();
  SIM800L.println("AT+CREG=2");
  delay(500);

  // Solicitar informacion de registro
  flushSIM800L();
  SIM800L.println("AT+CREG?");
  String cregResponse = readSIM800LResponse();

  // Serial.println("Respuesta AT+CREG?: " + cregResponse);

  int lacStart = cregResponse.indexOf("\"");
  int lacEnd = cregResponse.indexOf("\"", lacStart + 1);
  int cellIdStart = cregResponse.indexOf("\"", lacEnd + 1);
  int cellIdEnd = cregResponse.indexOf("\"", cellIdStart + 1);

  if (lacStart != -1 && lacEnd != -1 && cellIdStart != -1 && cellIdEnd != -1) {
    lac = cregResponse.substring(lacStart + 1, lacEnd);
    cellId = cregResponse.substring(cellIdStart + 1, cellIdEnd);
  } else {
    //Serial.println("Error al parsear LAC y Cell ID");
  }

  // Convertir de HEX a DEC si es necesario
  lac = hexToDec(lac);
  cellId = hexToDec(cellId);

  // Obtener el operador actual
  flushSIM800L();
  SIM800L.println("AT+COPS?");
  String copsResponse = readSIM800LResponse();

  // Serial.println("Respuesta AT+COPS?: " + copsResponse);

  int opStart = copsResponse.indexOf("\"");
  int opEnd = copsResponse.indexOf("\"", opStart + 1);

  if (opStart != -1 && opEnd != -1) {
    String operatorCode = copsResponse.substring(opStart + 1, opEnd); // Ej: 334020

    if (operatorCode.length() >= 5) {
      mcc = operatorCode.substring(0, 3);
      mnc = operatorCode.substring(3);
    } else {
      mcc = "000";
      mnc = "000";
    }

    //operatorName = operatorCode;  // O si quieres, mantén el nombre original
  } else {
    //operatorName = "Desconocido";
    mcc = "000";
    mnc = "000";
  }

  // Construir JSON manualmente
  // String json = "{";
  // json += "\"lac\":\"" + lac + "\",";
  // json += "\"cellId\":\"" + cellId + "\",";
  // json += "\"mcc\":\"" + mcc + "\",";
  // json += "\"mnc\":\"" + mnc + "\"";
  // json += "}";


  String json = "\"lac\":\"" + lac + "\",";
  json += "\"cid\":\"" + cellId + "\",";
  json += "\"mcc\":\"" + mcc + "\",";
  json += "\"mnc\":\"" + mnc + "\"";

  //enviarMensaje(" ---Torre celular--- ");
  //enviarMensaje(json);
  return json;
}

String hexToDec(String hexStr) {
  long decVal = strtol(hexStr.c_str(), NULL, 16);
  return String(decVal);
}

void flushSIM800L() {
  while (SIM800L.available()) {
    SIM800L.read();
  }
}

// void activeYellowLed(int option){
//   switch(option){
//     case 1:
//       digitalWrite(YELLOW_LED,HIGH);
//       delay(2000);
//       digitalWrite(YELLOW_LED,LOW);
//       delay(1000);
//       digitalWrite(YELLOW_LED,HIGH);
//       delay(1000);
//       digitalWrite(YELLOW_LED,LOW);
//       break;

//     case 2:
//       digitalWrite(YELLOW_LED,HIGH);
//       delay(1000);
//       digitalWrite(YELLOW_LED,LOW);
//       delay(2000);
//       digitalWrite(YELLOW_LED,HIGH);
//       delay(1000);
//       digitalWrite(YELLOW_LED,LOW);
//       break;

//     case 3:
//       digitalWrite(YELLOW_LED,HIGH);
//       delay(500);
//       digitalWrite(YELLOW_LED,LOW);
//       delay(500);
//       digitalWrite(YELLOW_LED,HIGH);
//       delay(500);
//       digitalWrite(YELLOW_LED,LOW);
//       delay(1000);
//       digitalWrite(YELLOW_LED,LOW);
//       delay(2000);
//       digitalWrite(YELLOW_LED,HIGH);
//       delay(1000);
//       digitalWrite(YELLOW_LED,LOW);
//       break;

//   }
// }

// void activeRedLed(int option){
//   switch(option){
//     case 1:
//       digitalWrite(RED_LED,HIGH);
//       delay(1000);
//       digitalWrite(RED_LED,LOW);
//       delay(1000);
//       digitalWrite(RED_LED,HIGH);
//       delay(1000);
//       digitalWrite(RED_LED,LOW);

//       break;

//     case 2:
//       digitalWrite(RED_LED,HIGH);
//       delay(500);
//       digitalWrite(RED_LED,LOW);
//       delay(500);
//       digitalWrite(RED_LED,HIGH);
//       delay(500);
//       digitalWrite(RED_LED,LOW);
//       break;

//     case 3:
//       digitalWrite(RED_LED,HIGH);
//       delay(1000);
//       digitalWrite(RED_LED,LOW);
//       delay(500);
//       digitalWrite(RED_LED,HIGH);
//       delay(500);
//       digitalWrite(RED_LED,LOW);
//       break;

//   }
// }


