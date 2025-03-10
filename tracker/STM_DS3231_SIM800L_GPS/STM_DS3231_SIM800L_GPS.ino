
/* Declaracion de librerias */
#include <HardwareSerial.h>
#include <TinyGPS.h>

volatile bool alarmFired = false;
TinyGPS gps;

/* Declaracion de puertos del STM32F103C8T6 */
const int SQW_PIN = PA0;
const int STM_LED = PC13;
const int LED = PA6;
const int RED_LED = PA7;
const int YELLOW_LED = PB1;
float latitude, longitude;
const int PUSH_BTN = PB0;

/* Constantes y Variables Globales */
const String ID = "48273619";
int _timeout;
String _buffer;
//String number = "+525545464585"; //Mio
const String number = "+525620577634"; //Oxxo Cel
//String number = "+525554743913"; //Telcel

const String coordenadasSinDatos = "\"lat\":\"\",\"lon\":\"\""; // Asignar directamente el texto
unsigned long chars;
unsigned short sentences, failed_checksum;
// Definir el puerto serial SIM800L
// HardwareSerial SIM800L(PA3, PA2);
// HardwareSerial NEO6M(PA10, PA9);
HardwareSerial SIM800L(PA10, PA9);
HardwareSerial NEO6M(PA3, PA2);

void setup() {
  // Iniciar los puertos serial a una velocidad de 9600 baudios
  //Serial.begin(9600);
  _buffer.reserve(50);
  SIM800L.begin(115200);
  NEO6M.begin(9600);
  
  /* COnfiguracion de puertos */
  pinMode(PUSH_BTN, INPUT);
  pinMode(STM_LED, OUTPUT);
  pinMode(LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);

  digitalWrite(STM_LED, LOW);
  digitalWrite(LED, HIGH);
  delay(2000);
  digitalWrite(STM_LED,HIGH);
  digitalWrite(LED,LOW);
}

void loop() {

  while (NEO6M.available()) // Leer datos del GPS
  {
    int c = NEO6M.read();

    if (gps.encode(c)) // Si se recibe una sentencia válida
    {
      gps.f_get_position(&latitude, &longitude);
      //gps.stats(&chars, &sentences, &failed_checksum);
    }
  }

  // put your main code here, to run repeatedly:
  if(digitalRead(PUSH_BTN) == HIGH){
    String datosGPS = leerYGuardarGPS();
    SendMessage(datosGPS);
    //char buffer[50];
    //snprintf(buffer, sizeof(buffer), "\"lat\":\"%.6f\",\"lon\":\"%.6f\"", latitude, latitude);
    //SendMessage(String(buffer));
  }
  
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

  // DateTime now = rtc.now();

  // char buffer[20];
  // sprintf(buffer, "%04d-%02d-%02dT%02d:%02d:%02d", 
  //         now.year(), now.month(), now.day(), 
  //         now.hour(), now.minute(), now.second());
  
  // String currentTime = String(buffer);

  activeYellowLed(3);

  String output = "{";
    output += "\"id\":\"" + ID + "\",";
    // output += "\"time\":\"" + currentTime + "\",";
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
            //enviarMensaje("Datos:" + String(latitude,6) + ". "+String(latitude,6));
            if (!isnan(latitude) && !isnan(latitude)) {
                break;
            }
        }
    }

    digitalWrite(RED_LED,LOW);
    delay(1000);
    digitalWrite(YELLOW_LED, LOW);
    //enviarMensaje("latitud:" + String(latitude) + ". Lon: "+String(latitude));
    // Si las coordenadas no son válidas, asignar valores predeterminados
    // if (isnan(_lat) || isnan(_lon)) {
    //     _lat = 0.0;
    //     _lon = 0.0;
    // }
    enviarMensaje("\"lat\":\"" + String(latitude,6) + "\",\"lon\":\"" + String(latitude,6) + "\"");
    return "\"latx\":\"" + String(latitude,6) + "\",\"lon\":\"" + String(latitude,6) + "\"";
    // char buffer[50];
    // snprintf(buffer, sizeof(buffer), "\"lat\":\"%.6f\",\"lon\":\"%.6f\"", latitude, latitude);
    // enviarMensaje(String(buffer));
    // return String(buffer);
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


