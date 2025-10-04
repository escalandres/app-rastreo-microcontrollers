
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
const int SLEEP_PIN = PB1;
const int SQW_PIN = PB0;
const int STM_LED = PC13;
const int BATERIA = PA0;
String latitude, longitude;

/* Constantes y Variables Globales */
const int ID = 48273619;

int _timeout;
String _buffer;

const String number = "+525620577634"; // Oxxo Cel
//const String number = "+525554743913"; //Telcel
//const String number = "+525545464585"; // Mi Telcel

unsigned long chars;
unsigned short sentences, failed_checksum;

// Definir el puerto serial A7670SA
HardwareSerial A7670SA(PA3, PA2);
HardwareSerial NEO8M(PA10, PA9);

void setAlarmFired() {
  alarmFired = true;
}

void configureAlarm(){
  rtc.clearAlarm(1);
  rtc.clearAlarm(2);
  rtc.disableAlarm(1);
  rtc.disableAlarm(2);

  //Set Alarm to be trigged in X 
  rtc.setAlarm1(rtc.now() + TimeSpan(0, 0, 1 , 0), DS3231_A1_Date);  // this mode triggers the alarm when the seconds match.

  alarmFired = false;
}

void setUpdateRate(Stream &gps) {
  // UBX-CFG-RATE: set update rate to 10000 ms (0.1 Hz)
  byte rateCfg[] = {
    0xB5, 0x62,       // Sync chars
    0x06, 0x08,       // Class = CFG, ID = RATE
    0x06, 0x00,       // Length = 6
    0x10, 0x27,       // measRate = 10000 ms (0x2710)
    0x01, 0x00,       // navRate = 1
    0x01, 0x00        // timeRef = 1 (GPS time)
  };
  gps.write(rateCfg, sizeof(rateCfg));
}

void disableNMEAMessages(Stream &gps) {
  // Formato: UBX-CFG-MSG (Clase 0xF0 = NMEA, ID = tipo de mensaje)
  byte msgs[][9] = {
    {0xB5, 0x62, 0x06, 0x01, 0x03, 0x00, 0xF0, 0x00, 0x00}, // GxGGA
    {0xB5, 0x62, 0x06, 0x01, 0x03, 0x00, 0xF0, 0x01, 0x00}, // GxGLL
    {0xB5, 0x62, 0x06, 0x01, 0x03, 0x00, 0xF0, 0x02, 0x00}, // GxGSA
    {0xB5, 0x62, 0x06, 0x01, 0x03, 0x00, 0xF0, 0x03, 0x00}, // GxGSV
    {0xB5, 0x62, 0x06, 0x01, 0x03, 0x00, 0xF0, 0x05, 0x00}, // GxVTG
    {0xB5, 0x62, 0x06, 0x01, 0x03, 0x00, 0xF0, 0x06, 0x00}  // GxGRS
  };

  for (int i = 0; i < sizeof(msgs) / sizeof(msgs[0]); i++) {
    gps.write(msgs[i], sizeof(msgs[i]));
    delay(50); // Pequeña pausa entre comandos
  }
}

void configureGPS(Stream &gps){
  delay(500); // Esperar a que el GPS esté listo tras el power-up
  setUpdateRate(gps);
  delay(500); // Pausa corta entre comandos
  disableNMEAMessages(gps);
}


void setup() {
  // put your setup code here, to run once:
  Wire.begin();
  _buffer.reserve(50);
  A7670SA.begin(115200);
  NEO8M.begin(9600);

  /* COnfiguracion de puertos */
  pinMode(SLEEP_PIN, OUTPUT);
  pinMode(SQW_PIN, INPUT_PULLUP);
  pinMode(STM_LED, OUTPUT);
  analogReadResolution(12);
  digitalWrite(STM_LED, LOW);

  //configureGPS(NEO8M);

  if (!rtc.begin()) {        // si falla la inicializacion del modulo
    //Serial.println("Modulo RTC no encontrado !");  // muestra mensaje de error
    while (1);         // bucle infinito que detiene ejecucion del programa
  }

  if(rtc.lostPower()) {
      DateTime localTime(__DATE__, __TIME__);
      DateTime utcTime = localTime + TimeSpan(6 * 3600); // Convertir a UTC sumando 6 horas
      // Ajustar el RTC a la fecha y hora de compilación en UTC
      rtc.adjust(utcTime);
  }

  rtc.disable32K();
  rtc.writeSqwPinMode(DS3231_OFF);
  configureAlarm();
  
  //delay(12000);
  //enviarMensaje("Rastreador encendido");
  //delay(12000);
  //notificarEncendido();
  //delay(2000);
  digitalWrite(STM_LED,HIGH);
  //digitalWrite(LEFT_LED,LOW);
  sleepA7670SA(true);
  // Configure low power
  LowPower.begin();
  // Attach a wakeup interrupt on pin, calling repetitionsIncrease when the device is woken up
  // Last parameter (LowPowerMode) should match with the low power state used
  //LowPower.attachInterruptWakeup(digitalPinToInterrupt(SQW_PIN), setAlarmFired, FALLING, SLEEP_MODE);
  LowPower.attachInterruptWakeup(digitalPinToInterrupt(SQW_PIN), setAlarmFired, FALLING, DEEP_SLEEP_MODE);
}

void loop() {

  if(alarmFired){
    digitalWrite(STM_LED, HIGH);
    delay(2000);
    digitalWrite(STM_LED,LOW);
    sleepA7670SA(false);
    //sleepA7670SA(false);
    startA7670SA();
    String datosGPS = leerYGuardarGPS();

    SendMessage(datosGPS);
    sleepA7670SA(true);
    configureAlarm();
    //LowPower.sleep();
    LowPower.deepSleep();
  }
  
}

void enviarComando(const char* comando, int espera = 1000) {
  A7670SA.println(comando);
  delay(espera);

  // while (A7670SA.available()) {
  //   Serial.write(A7670SA.read());
  // }
  //Serial.println();a
}

void startA7670SA(){
  //digitalWrite(LEFT_LED, HIGH);
   // 1. Probar comunicación AT
  enviarComando("AT", 1000);

  // 5. Establecer modo LTE (opcional)
  enviarComando("AT+CNMP=2", 2000);

  // Confirmar nivel de señal y registro otra vez
  enviarComando("AT+CSQ", 1000);
  enviarComando("AT+CREG?", 1000);
  //digitalWrite(LEFT_LED, LOW);
}

void sleepA7670SA(bool dormir) {
  if (dormir) {
    //Dormir A7670SA
    enviarComando("AT+CSCLK=1");    
    delay(100);
    digitalWrite(SLEEP_PIN, LOW);  // DTR HIGH -> permite sleep en idle
    //Serial.println("A7670SA en modo Sleep (cuando idle).");
  } else {
    //Despertar A7670SA
    digitalWrite(SLEEP_PIN, HIGH);   // DTR LOW -> despierta módulo
    delay(100);                     // Tiempo para que despierte
    enviarComando("AT+CSCLK=0");   
    enviarComando("AT");          // Activar UART
    //Serial.println("A7670SA Despierto y sin sleep automático.");
  }
}

String readA7670SAResponse(unsigned long timeout = 2000) {
  String response = "";
  unsigned long startTime = millis();

  while (millis() - startTime < timeout) {
    while (A7670SA.available()) {
      char c = A7670SA.read();
      response += c;
    }
  }

  return response;
}

String _readSerial() {
  _timeout = 0;
  while  (!A7670SA.available() && _timeout < 12000  )
  {
    delay(13);
    _timeout++;
  }
  if (A7670SA.available()) {
    return A7670SA.readString();
  }
}

void flushA7670SA() {
    unsigned long startTime = millis();
    while (A7670SA.available()) {
        A7670SA.read();
        if (millis() - startTime > 500) { // Máximo 500 ms de espera
            break;
        }
    }
}

void enviarMensaje(String SMS)
{
  //digitalWrite(MID_LED,HIGH);
  startA7670SA();
  enviarComando("AT+CREG?",1000);
  enviarComando("AT+CMGF=1",1000);
  
  //Serial.println ("Set SMS Number");
  enviarComando(("AT+CMGS=\"" + number + "\"").c_str(), 3000); //Mobile phone number to send message

  A7670SA.println(SMS);
  delay(500);
  A7670SA.println((char)26);// ASCII code of CTRL+Z
  delay(500);
  _buffer = _readSerial();

  delay(2000);
  //digitalWrite(MID_LED,LOW);
}

void SendMessage(String datosGPS)
{
  digitalWrite(STM_LED, LOW);
  
  String cellTowerInfo = "";
  cellTowerInfo = getCellInfo();
  //digitalWrite(LEFT_LED,LOW);

  String batteryCharge = "";
  batteryCharge = obtenerVoltajeBateria();

  String SMS = createMessageToSend(datosGPS, cellTowerInfo, batteryCharge);
  enviarMensaje(SMS);

  delay(2000);
  digitalWrite(STM_LED,HIGH);
  
}

String createMessageToSend(String datosGPS, String cellTowerInfo, String batteryCharge){

  //Verificar si el RTC tiene la hora y fecha correcta
  corregirRTC();
  
  DateTime now = rtc.now();

  char buffer[20];
  sprintf(buffer, "%04d-%02d-%02dT%02d:%02d:%02d", 
          now.year(), now.month(), now.day(), 
          now.hour(), now.minute(), now.second());
  
  String currentTime = String(buffer);

  String output = "id:" + String(ID) + ",";
    output += "time:" + currentTime + ",";
    output += cellTowerInfo + ",";
    output += batteryCharge + ",";
    output += datosGPS;
  return output;
}

void notificarEncendido()
{
  digitalWrite(STM_LED, HIGH);
  delay(500);
  digitalWrite(STM_LED, LOW);
  delay(500);
  digitalWrite(STM_LED, HIGH);
  delay(500);
  digitalWrite(STM_LED, LOW);
  DateTime now = rtc.now();

  char buffer[20];
  sprintf(buffer, "%04d-%02d-%02dT%02d:%02d:%02d", 
        now.year(), now.month(), now.day(), 
        now.hour(), now.minute(), now.second());
  
  String currentTime = String(buffer);

  String SMS = "tracker:" + String(ID) + ",";
    SMS += "time:" + currentTime;
  enviarMensaje(SMS);

  delay(2000);
  digitalWrite(STM_LED,HIGH);
  
}

//String leerYGuardarGPS() {
//  //enviarMensaje(" ---Buscando señal--- ");
//
//  String nuevaLat = "";
//  String nuevaLon = "";
//  String anteriorLat = latitude;  
//  String anteriorLon = longitude;
//  bool ubicacionActualizada = false;
//  unsigned long startTime = millis();
//  int intentos = 0;
//
//  while ((millis() - startTime) < 10000 && intentos < 30 && !ubicacionActualizada) { 
//    while (NEO8M.available()) {
//      char c = NEO8M.read();
//      gps1.encode(c);
//      if (gps1.location.isUpdated()) { 
//        nuevaLat = String(gps1.location.lat(), 6);
//        nuevaLon = String(gps1.location.lng(), 6);
//        //corregirRTC();
//        if (nuevaLat != anteriorLat || nuevaLon != anteriorLon) { 
//          latitude = nuevaLat;
//          longitude = nuevaLon;
//          ubicacionActualizada = true;
//          break;
//        }
//      }
//    }
//    delay(50); 
//    intentos++;
//  }
//
//  //digitalWrite(RIGHT_LED, LOW);
//  corregirRTC();
//  if(!ubicacionActualizada){
//    nuevaLat = latitude;
//    nuevaLon = longitude;
//  }
//
//  if (nuevaLat == "" || nuevaLon == "") {
//    nuevaLat = "0.0";
//    nuevaLon = "0.0";
//  }
//
//  // return "\"lat\":\"" + nuevaLat + "\",\"lon\":\"" + nuevaLon + "\"";
//  return "lat:" + nuevaLat + ",lon:" + nuevaLon;
//
//}

String leerYGuardarGPS() {
    String nuevaLat = "";
    String nuevaLon = "";
    bool ubicacionActualizada = false;
    unsigned long startTime = millis();
    int intentos = 0;

    while ((millis() - startTime) < 10000 && intentos < 30 && !ubicacionActualizada) {
        while (NEO8M.available()) {
            char c = NEO8M.read();
            gps1.encode(c);

            // Verifica si la ubicación es válida y hay satélites disponibles
            if (gps1.location.isUpdated() && gps1.location.isValid() && gps1.satellites.value() > 0) { 
                nuevaLat = String(gps1.location.lat(), 6);
                nuevaLon = String(gps1.location.lng(), 6);

                latitude = nuevaLat;
                longitude = nuevaLon;
                ubicacionActualizada = true;
                break;
            }
        }
        delay(50);
        intentos++;
    }

    // Si NO hay conexión con satélites, actualiza los valores a 0.0 en el STM32
    if (gps1.satellites.value() == 0 || latitude == "" || longitude == "") {
        latitude = "0.0";
        longitude = "0.0";
    }

    return "lat:" + latitude + ",lon:" + longitude;
}

void corregirRTC() {
    //enviarMensaje("Verificando RTC....");

    DateTime now = rtc.now();
    if (now.year() != 2025 ) {
      delay(1500);
      //enviarMensaje("Actualizando RTC....");
        if (gps1.date.isValid() && gps1.time.isValid()) {
            int year = gps1.date.year();
            int month = gps1.date.month();
            int day = gps1.date.day();
            int hour = gps1.time.hour();
            int minute = gps1.time.minute();
            int second = gps1.time.second();

            // Validar que no vengan ceros o fechas erróneas
            if (year > 2024 && month >= 1 && month <= 12 && day >= 1 && day <= 31) {
                rtc.adjust(DateTime(year, month, day, hour, minute, second));
                enviarMensaje("RTC ajustado a la fecha y hora del GPS.");
            } else {
                // Si la fecha es inválida, ajustar a una hora fija de respaldo
                rtc.adjust(DateTime(2024, 1, 1, 0, 0, 0));
                enviarMensaje("RTC ajustado a hora predeterminada por datos inválidos.");
            }
        } else {
            // Si no hay datos válidos en el GPS
            rtc.adjust(DateTime(2024, 1, 1, 0, 0, 0));
            enviarMensaje("RTC ajustado a hora predeterminada por falta de datos.");
        }
    }
}

String getCellInfo() {
    String lac = "";
    String cellId = "";
    String mcc = "";
    String mnc = "";
    String red = "";

    // Solicitar información de la red con A7670SA
    flushA7670SA();

    enviarComando("AT+CPSI?",2000);
    String cpsiResponse = readA7670SAResponse();
    //enviarMensaje("CPSI" + cpsiResponse);
    // Extraer datos de la respuesta de AT+CPSI?
    int startIndex = cpsiResponse.indexOf("CPSI:");
    //enviarMensaje("startIndex" + startIndex);
    if (startIndex != -1) {
        startIndex += 6; // Mover el índice después de "CPSI: "

        // Verificar si es LTE o GSM
        if (cpsiResponse.startsWith("LTE", startIndex)) {
            red = "lte";
            startIndex += 4; // Mover el índice después de "LTE,"
        } else if (cpsiResponse.startsWith("GSM", startIndex)) {
            red = "gsm";
            startIndex += 4; // Mover el índice después de "GSM,"
        } else {
            return "{}"; // Si no es ni GSM ni LTE, devolver JSON vacío
        }

        // Extraer MCC-MNC correctamente
        int mccMncStart = cpsiResponse.indexOf(",", startIndex) + 1;
        int mccMncEnd = cpsiResponse.indexOf(",", mccMncStart);
        String mccMncRaw = cpsiResponse.substring(mccMncStart, mccMncEnd);

        // Separar MCC y MNC
        int separatorIndex = mccMncRaw.indexOf("-");
        if (separatorIndex != -1) {
            mcc = mccMncRaw.substring(0, separatorIndex);  // MCC
            mnc = mccMncRaw.substring(separatorIndex + 1); // MNC
        }

        // Extraer LAC
        int lacStart = mccMncEnd + 1;
        int lacEnd = cpsiResponse.indexOf(",", lacStart);
        lac = cpsiResponse.substring(lacStart, lacEnd);

        // Extraer Cell ID
        int cellIdStart = lacEnd + 1;
        int cellIdEnd = cpsiResponse.indexOf(",", cellIdStart);
        cellId = cpsiResponse.substring(cellIdStart, cellIdEnd);
    }

    // Convertir de Hex a Decimal
    lac = hexToDec(lac);

    String json = "red:" + red + ",";
    json += "mcc:" + mcc + ",";
    json += "mnc:" + mnc + ",";
    json += "lac:" + lac + ",";
    json += "cid:" + cellId;
    //enviarMensaje(json);
    return json;
}

String hexToDec(String hexStr) {
  long decVal = strtol(hexStr.c_str(), NULL, 16);
  return String(decVal);
}

String obtenerVoltajeBateria(){

    //enviarMensaje("obtenerVoltajeBateria");
    float voltaje = leerVoltaje(BATERIA);
    //enviarMensaje("Voltaje:"+String(voltaje));
    //float voltaje = leerVoltajeSuavizado(BATERIA, voltajeBateria, alpha);
    int nivelBateria = calcularNivelBateria(voltaje);
    //enviarMensaje("nivelBateria:"+String(nivelBateria));
    String sms = "nb:"+ String(nivelBateria);
    return sms;
}

float leerVoltaje(int pin) {
    const float R1 = 51000.0;  // ohms
    const float R2 = 20000.0;  // ohms
    int lecturaADC = analogRead(pin);
    float voltajeSalida = (lecturaADC / 4095.0) * 3.3;  // Voltaje en el pin ADC
    float voltajeBateria = voltajeSalida * ((R1 + R2) / R2);
    return voltajeBateria;
}

int calcularNivelBateria(float voltaje) {
  const float voltajeMax = 4.2; // Voltaje máximo de la batería (por ejemplo, Li-Ion)
  const float voltajeMin = 3.0; // Voltaje mínimo antes de considerarla descargada
  float porcentaje = ((voltaje - voltajeMin) / (voltajeMax - voltajeMin)) * 100;
  porcentaje = constrain(porcentaje, 0, 100); // Limita el porcentaje entre 0 y 100%
  return (int)porcentaje;
}
