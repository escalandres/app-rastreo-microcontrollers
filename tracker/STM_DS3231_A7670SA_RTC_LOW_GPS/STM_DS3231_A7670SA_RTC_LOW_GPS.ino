
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
const int LED = PA6;
const int RED_LED = PA7;
const int YELLOW_LED = PB10;
const int BATERIA = PA0;
String latitude, longitude;
//const int PUSH_BTN = PB0;

/* Constantes y Variables Globales */
const String ID = "48273619";

int _timeout;
String _buffer;

//const String number = "+525620577600"; //Oxxo Cel
const String number = "+525554743900"; //Telcel

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
  rtc.setAlarm1(rtc.now() + TimeSpan(0, 0, 1, 0), DS3231_A1_Minute);  // this mode triggers the alarm when the seconds match.

  alarmFired = false;
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
  pinMode(LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);

  digitalWrite(STM_LED, LOW);
  digitalWrite(LED, HIGH);

  if (!rtc.begin()) {        // si falla la inicializacion del modulo
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

  rtc.disable32K();
  rtc.writeSqwPinMode(DS3231_OFF);
  configureAlarm();
  
  delay(12000);
  enviarMensaje("Rastreador encendido");
  digitalWrite(STM_LED,HIGH);
  digitalWrite(LED,LOW);
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

    sleepA7670SA(false);
    sleepA7670SA(false);
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

  while (A7670SA.available()) {
    Serial.write(A7670SA.read());
  }
  Serial.println();
}

void startA7670SA(){
   // 1. Probar comunicación AT
  enviarComando("AT", 1000);

  // 5. Establecer modo LTE (opcional)
  enviarComando("AT+CNMP=2", 2000);

  // Confirmar nivel de señal y registro otra vez
  enviarComando("AT+CSQ", 1000);
  enviarComando("AT+CREG?", 1000);
}

void sleepA7670SA(bool dormir) {
  if (dormir) {
    //Dormir A7670SA
    enviarComando("AT+CSCLK=1");    
    delay(100);
    digitalWrite(SLEEP_PIN, LOW);  // DTR HIGH -> permite sleep en idle
    Serial.println("A7670SA en modo Sleep (cuando idle).");
  } else {
    //Despertar A7670SA
    digitalWrite(SLEEP_PIN, HIGH);   // DTR LOW -> despierta módulo
    delay(100);                     // Tiempo para que despierte
    enviarComando("AT+CSCLK=0");   
    enviarComando("AT");          // Activar UART
    Serial.println("A7670SA Despierto y sin sleep automático.");
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
  while (A7670SA.available()) {
    A7670SA.read();
  }
}


void enviarMensaje(String SMS)
{
  digitalWrite(RED_LED,HIGH);
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
  digitalWrite(RED_LED,LOW);
}

void SendMessage(String datosGPS)
{
  digitalWrite(STM_LED, LOW);
  digitalWrite(LED, HIGH);
  
  String cellTowerInfo = "";
  cellTowerInfo = getCellInfo();

  String batteryCharge = "";
  batteryCharge = obtenerVoltajeBateria();

  String SMS = createMessageToSend(datosGPS, cellTowerInfo, batteryCharge);
  enviarMensaje(SMS);

  delay(2000);
  digitalWrite(STM_LED,HIGH);
  digitalWrite(LED,LOW);
}

String createMessageToSend(String datosGPS, String cellTowerInfo, String batteryCharge){

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
    output += batteryCharge + ",";
    output += datosGPS;
    output += "}";
  
  return output;
}

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
    String mcc = "";
    String mnc = "";
    String red = "";

    // Solicitar información de la red con A7670SA
    flushA7670SA();
    A7670SA.println("AT+CPSI?");
    String cpsiResponse = readA7670SAResponse();

    // Extraer datos de la respuesta de AT+CPSI?
    int startIndex = cpsiResponse.indexOf("CPSI:");
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

    // Construir JSON corregido
    String json = "{";
    json += "\"red\":\"" + red + "\",";
    json += "\"mcc\":\"" + mcc + "\",";
    json += "\"mnc\":\"" + mnc + "\",";
    json += "\"lac\":\"" + lac + "\",";
    json += "\"cid\":\"" + cellId + "\"";
    json += "}";

    return json;
}


String hexToDec(String hexStr) {
  long decVal = strtol(hexStr.c_str(), NULL, 16);
  return String(decVal);
}


String obtenerVoltajeBateria(){
    float voltaje = leerVoltaje(BATERIA);
    int nivelBateria = calcularNivelBateria(voltaje);

    String sms = "\"nb\":"+ String(nivelBateria);
    Serial.println(sms);
    
    return sms;
}

float leerVoltaje(int pin) {
    int lecturaADC = analogRead(pin);
    float voltaje = (lecturaADC / 1023.0) * 5.0; // Convierte lectura ADC a voltaje (ajusta si usas divisor)
    return voltaje;
}

int calcularNivelBateria(float voltaje) {
  const float voltajeMax = 4.2; // Voltaje máximo de la batería (por ejemplo, Li-Ion)
  const float voltajeMin = 3.0; // Voltaje mínimo antes de considerarla descargada
  float porcentaje = ((voltaje - voltajeMin) / (voltajeMax - voltajeMin)) * 100;
  porcentaje = constrain(porcentaje, 0, 100); // Limita el porcentaje entre 0 y 100%
  return (int)porcentaje;
}

