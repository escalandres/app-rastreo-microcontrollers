
/* Declaracion de librerias */
#include <Wire.h>
#include <HardwareSerial.h>
#include <RTClib.h>
#include <low_power.h>
#include <STM32LowPower.h>
#include <TinyGPS.h>

volatile bool alarmFired = false;
RTC_DS3231 rtc;
TinyGPS gps;

/* Declaracion de puertos del STM32F103C8T6 */
const int SQW_PIN = PA0;
const int STM_LED = PC13;
const int LED = PA6;
const int RED_LED = PA7;
const int YELLOW_LED = PB1;
float latitude, longitude;
//const int PUSH_BTN = PB0;

/* Constantes y Variables Globales */
const String ID = "48273619";
int _timeout;
String _buffer;

const String number = "+525620577634"; //Oxxo Cel
//String number = "+525554743913"; //Telcel

const String coordenadasSinDatos = "\"lat\":\"\",\"lon\":\"\""; // Asignar directamente el texto
unsigned long chars;
unsigned short sentences, failed_checksum;


// Definir el puerto serial SIM800L
HardwareSerial SIM800L(PA3, PA2);
HardwareSerial NEO6M(PA10, PA9);
//HardwareSerial SIM800L(PA10, PA9);
//HardwareSerial NEO6M(PA3, PA2);

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
  
  // stop oscillating signals at SQW Pin
  // otherwise setAlarm1 will fail
  //rtc.writeSqwPinMode(DS3231_OFF);

  //Set Alarm to be trigged in X 
  rtc.setAlarm1(rtc.now() + TimeSpan(0, 0, 1, 0), DS3231_A1_Minute);  // this mode triggers the alarm when the seconds match.

  alarmFired = false;
}

void setup() {
  // put your setup code here, to run once:
  Wire.begin();
  // Iniciar los puertos serial a una velocidad de 9600 baudios
  //Serial.begin(9600);
  _buffer.reserve(50);
  SIM800L.begin(115200);
  NEO6M.begin(9600);

  /* COnfiguracion de puertos */
  pinMode(SQW_PIN, INPUT_PULLUP);
  //pinMode(PUSH_BTN, INPUT);
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

  delay(2000);
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
  while (NEO6M.available()) // Leer datos del GPS
  {
    int c = NEO6M.read();

    if (gps.encode(c)) // Si se recibe una sentencia válida
    {
      gps.f_get_position(&latitude, &longitude);
      gps.stats(&chars, &sentences, &failed_checksum);
    }
  }

  if(alarmFired){

    String datosGPS = leerYGuardarGPS();

    // if (datosGPS != coordenadasSinDatos) {
    //   SendMessage(datosGPS);
    // }
    //SendMessageNoData();
    SendMessage(datosGPS);
    
    // else{
    //   SendMessageNoData();
    // }

    configureAlarm();
    //LowPower.sleep();
    LowPower.deepSleep();
  }
  
}

void SendMessageNoData()
{
  digitalWrite(RED_LED,HIGH);
  SIM800L.println("AT+CMGF=1");    //Sets the GSM Module in Text Mode
  delay(200);
  
  //Serial.println ("Set SMS Number");
  SIM800L.println("AT+CMGS=\"" + number + "\"\r"); //Mobile phone number to send message
  delay(200);

  String SMS = "No hay datos del rastreador";
  SIM800L.println(SMS);
  delay(200);
  SIM800L.println((char)26);// ASCII code of CTRL+Z
  delay(200);
  _buffer = _readSerial();

  delay(2000);
  digitalWrite(RED_LED,LOW);
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

void SendMessage(String datosGPS)
{
  digitalWrite(STM_LED, LOW);
  digitalWrite(LED, HIGH);
  
  //Serial.println ("Sending Message");
  SIM800L.println("AT+CMGF=1");    //Sets the GSM Module in Text Mode
  delay(1500);
  
  //Serial.println ("Set SMS Number");
  SIM800L.println("AT+CMGS=\"" + number + "\"\r"); //Mobile phone number to send message
  delay(1200);

  String SMS = createMeesageToSend(datosGPS);
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


String createMeesageToSend(String datosGPS){

  activeYellowLed(2);

  DateTime now = rtc.now();

  char buffer[20];
  sprintf(buffer, "%04d-%02d-%02dT%02d:%02d:%02d", 
          now.year(), now.month(), now.day(), 
          now.hour(), now.minute(), now.second());
  
  String currentTime = String(buffer);

  activeYellowLed(3);

  String output = "{";
    output += "\"id\":\"" + ID + "\",";
    output += "\"time\":\"" + currentTime + "\",";
    output += datosGPS;
    output += "}";
  
  return output;
}

String leerYGuardarGPS() {
  activeRedLed(2);
  enviarMensaje(" ---Buscando senal--- ");
    //String lat = "", lon = "";
    float _lat, _lon = NAN;
    digitalWrite(YELLOW_LED, HIGH);
    unsigned long startTime = millis();
    while (NEO6M.available() && (millis() - startTime) < 5000) {
        int c = NEO6M.read();
        if (gps.encode(c)) {
            digitalWrite(RED_LED,HIGH);
            gps.f_get_position(&latitude, &longitude);
            enviarMensaje("Datos:" + String(latitude,6) + ". "+String(longitude,6));
            if (!isnan(latitude) && !isnan(longitude)) {
                break;
            }
        }
    }

    digitalWrite(RED_LED,LOW);
    delay(1000);
    digitalWrite(YELLOW_LED, LOW);
    // Si las coordenadas no son válidas, asignar valores predeterminados
    if (isnan(latitude) || isnan(longitude)) {
        latitude = 0.0;
        longitude= 0.0;
    }
    //return "\"lat\":\"" + _lat + "\",\"lon\":\"" + _lon + "\"";
    //  enviarMensaje("\"lat\":\"" + String(latitude,6) + "\",\"lon\":\"" + String(longitude,6) + "\"");
    return "\"lat\":\"" + String(latitude,6) + "\",\"lon\":\"" + String(longitude,6) + "\"";
}

void activeYellowLed(int option){
  switch(option){
    case 1:
      digitalWrite(YELLOW_LED,HIGH);
      delay(2000);
      digitalWrite(YELLOW_LED,LOW);
      delay(1000);
      digitalWrite(YELLOW_LED,HIGH);
      delay(1000);
      digitalWrite(YELLOW_LED,LOW);
      break;

    case 2:
      digitalWrite(YELLOW_LED,HIGH);
      delay(1000);
      digitalWrite(YELLOW_LED,LOW);
      delay(2000);
      digitalWrite(YELLOW_LED,HIGH);
      delay(1000);
      digitalWrite(YELLOW_LED,LOW);
      break;

    case 3:
      digitalWrite(YELLOW_LED,HIGH);
      delay(500);
      digitalWrite(YELLOW_LED,LOW);
      delay(500);
      digitalWrite(YELLOW_LED,HIGH);
      delay(500);
      digitalWrite(YELLOW_LED,LOW);
      delay(1000);
      digitalWrite(YELLOW_LED,LOW);
      delay(2000);
      digitalWrite(YELLOW_LED,HIGH);
      delay(1000);
      digitalWrite(YELLOW_LED,LOW);
      break;

  }
}

void activeRedLed(int option){
  switch(option){
    case 1:
      digitalWrite(RED_LED,HIGH);
      delay(1000);
      digitalWrite(RED_LED,LOW);
      delay(1000);
      digitalWrite(RED_LED,HIGH);
      delay(1000);
      digitalWrite(RED_LED,LOW);

      break;

    case 2:
      digitalWrite(RED_LED,HIGH);
      delay(500);
      digitalWrite(RED_LED,LOW);
      delay(500);
      digitalWrite(RED_LED,HIGH);
      delay(500);
      digitalWrite(RED_LED,LOW);
      break;

    case 3:
      digitalWrite(RED_LED,HIGH);
      delay(1000);
      digitalWrite(RED_LED,LOW);
      delay(500);
      digitalWrite(RED_LED,HIGH);
      delay(500);
      digitalWrite(RED_LED,LOW);
      break;

  }
}


